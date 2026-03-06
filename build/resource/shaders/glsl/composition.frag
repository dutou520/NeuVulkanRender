#version 450

// GBuffer 采样器
layout(set = 0, binding = 0) uniform sampler2D gbuffer1; // Albedo + MaterialFlags
layout(set = 0, binding = 1) uniform sampler2D gbuffer2; // Specular + Occlusion
layout(set = 0, binding = 2) uniform sampler2D gbuffer3; // Normal + Smoothness
layout(set = 0, binding = 3) uniform sampler2D gbuffer4; // ShadingID + Emissive
layout(set = 0, binding = 4) uniform sampler2D depthBuffer; // Depth
layout(set = 0, binding = 5) uniform sampler2D shadowMap; // Shadow Map

// 方向光数据
layout(set = 1, binding = 0) uniform LightData {
    vec3 lightDir;
    float _pad1;
    vec3 lightColor;
    float _pad2;
    vec3 viewPos;
    mat4 invViewProj; // 逆 视图-投影 矩阵
    mat4 u_LightVP;   // 光源 视图-投影 矩阵
    float u_LightNear;
    float u_LightFar;
} light;

// 点光源数据
#define MAX_POINT_LIGHTS 128

struct PointLight {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

layout(set = 1, binding = 1) uniform PointLightsData {
    PointLight lights[MAX_POINT_LIGHTS];
    uint count;
} pointLights;

// PCSS参数
layout(set = 1, binding = 2) uniform PCSSParams {
    float u_LightSize;
    float u_ShadowDistance;
    uint u_BlockerSamples;
    uint u_PCFSamples;
    uint u_ShadowMapRes;
    uint enableDirectionalLight;
    uint enableShadow;
    float u_Bias;
    float u_MinFilterSize;
} pcss;

// 天空盒与 IBL 参数
layout(set = 1, binding = 3) uniform SkyboxParams {
    vec4 sh[9];        // 9 Spherical Harmonics coefficients, each xyz is rgb
    float brightness;
    float rotationY;
} skyboxParams;

// IBL 高光 (split sum)
layout(set = 2, binding = 0) uniform samplerCube prefilteredMap;
layout(set = 2, binding = 1) uniform sampler2D brdfLUT;

// 视口信息
layout(push_constant) uniform PushConstants {
    vec2 viewportSize;
} pc;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

// ===================== 常量 =====================
const float PI = 3.14159265359;

// ===================== 辅助函数 =====================

// HSL 转 RGB (用于自发光色相计算)
vec3 hslToRgb(vec3 hsl) {
    float h = hsl.x / 360.0;
    float s = hsl.y;
    float l = hsl.z;
    
    float c = (1.0 - abs(2.0 * l - 1.0)) * s;
    float x = c * (1.0 - abs(mod(h * 6.0, 2.0) - 1.0));
    float m = l - c / 2.0;
    
    vec3 rgb;
    if (h < 1.0/6.0) rgb = vec3(c, x, 0.0);
    else if (h < 2.0/6.0) rgb = vec3(x, c, 0.0);
    else if (h < 3.0/6.0) rgb = vec3(0.0, c, x);
    else if (h < 4.0/6.0) rgb = vec3(0.0, x, c);
    else if (h < 5.0/6.0) rgb = vec3(x, 0.0, c);
    else rgb = vec3(c, 0.0, x);
    
    return rgb + m;
}

// 获取自发光颜色
vec3 getEmissiveColor(float hue, float brightness) {
    if (brightness <= 0.001) return vec3(0.0);
    float h = hue * 360.0;
    return hslToRgb(vec3(h, 1.0, brightness));
}

// 评估球谐光照 (3阶, 9个系数)
vec3 evaluateSH(vec3 N) {
    vec3 result = 
        skyboxParams.sh[0].xyz +
        skyboxParams.sh[1].xyz * N.y +
        skyboxParams.sh[2].xyz * N.z +
        skyboxParams.sh[3].xyz * N.x +
        skyboxParams.sh[4].xyz * N.y * N.x +
        skyboxParams.sh[5].xyz * N.y * N.z +
        skyboxParams.sh[6].xyz * (3.0 * N.z * N.z - 1.0) +
        skyboxParams.sh[7].xyz * N.z * N.x +
        skyboxParams.sh[8].xyz * (N.x * N.x - N.y * N.y);
    return max(result, vec3(0.0));
}

// Schlick Fresnel 近似
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// 考虑粗糙度的 Fresnel (用于 IBL 环境光)
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// GGX 分布函数
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return a2 / denom;
}

// Smith 几何遮蔽函数
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
    
    return ggx1 * ggx2;
}

// 点光源衰减函数
float calculateAttenuation(float distance, float radius) {
    // 平滑衰减: 在半径边缘衰减到0
    float attenuation = clamp(1.0 - distance / radius, 0.0, 1.0);
    // 距离平方反比衰减
    attenuation *= 1.0 / (distance * distance + 1.0);
    return attenuation;
}

// ===================== 着色函数 =====================

// Interleaved Gradient Noise (比一般随机噪声质量更好)
float interleavedGradientNoise(vec2 position_screen) {
    vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
    return fract(magic.z * fract(dot(position_screen, magic.xy)));
}

// Vogel Disk Sample Pattern (Golden Angle)
vec2 vogelDiskSample(int sampleIndex, int samplesCount, float phi) {
    float GoldenAngle = 2.4f;
    float r = sqrt(float(sampleIndex) + 0.5f) / sqrt(float(samplesCount));
    float theta = float(sampleIndex) * GoldenAngle + phi;
    
    float sine = sin(theta);
    float cosine = cos(theta);
    
    return vec2(r * cosine, r * sine);
}
// Poisson Disk采样点(16个)
// const vec2 poissonDisk[16] = vec2[]( ... );

// PCSS第一步: 遮挡物搜索
float findBlockerDistance(vec3 shadowCoord, float searchRadius) {
    float blockerSum = 0.0;
    int blockerCount = 0;
    
    int samples = int(pcss.u_BlockerSamples);
    float phi = interleavedGradientNoise(gl_FragCoord.xy) * 6.2831853; 
    
    for (int i = 0; i < samples; i++) {
        vec2 offset = vogelDiskSample(i, samples, phi) * searchRadius;
        float shadowDepth = texture(shadowMap, shadowCoord.xy + offset).r;
        
        if (shadowDepth < shadowCoord.z - pcss.u_Bias) {
            blockerSum += shadowDepth;
            blockerCount++;
        }
    }
    
    if (blockerCount == 0) {
        return -1.0; // 无遮挡
    }
    
    return blockerSum / float(blockerCount);
}

// PCSS第二步: 半影半径计算
float penumbraSize(float zReceiver, float zBlocker) {
    return (zReceiver - zBlocker) * pcss.u_LightSize / zBlocker;
}

// PCSS第三步: PCF滤波
float PCF_Filter(vec3 shadowCoord, float filterRadius) {
    float shadow = 0.0;
    int samples = int(pcss.u_PCFSamples);
    float phi = interleavedGradientNoise(gl_FragCoord.xy) * 6.2831853; 

    for (int i = 0; i < samples; i++) {
        vec2 offset = vogelDiskSample(i, samples, phi) * filterRadius;
        float shadowDepth = texture(shadowMap, shadowCoord.xy + offset).r;
        shadow += (shadowCoord.z - pcss.u_Bias <= shadowDepth) ? 1.0 : 0.0;
    }
    
    return shadow / float(samples);
}

// PCSS完整实现
float calculatePCSSShadow(vec3 worldPos) {
    // 如果阴影未启用,返回完全光照
    if (pcss.enableShadow == 0u) {
        return 1.0;
    }
    
    // 转换到光源空间
    vec4 lightSpacePos = light.u_LightVP * vec4(worldPos, 1.0);
    vec3 shadowCoord = lightSpacePos.xyz / lightSpacePos.w;
    
    // 转换到[0,1]范围
    shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;
    
    // 边界检查
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0) {
        return 1.0;
    }
    
    // 计算搜索半径
    float texelSize = 1.0 / float(pcss.u_ShadowMapRes);
    float searchRadius = pcss.u_LightSize * texelSize;
    
    // 第一步: 遮挡物搜索
    float avgBlockerDepth = findBlockerDistance(shadowCoord, searchRadius);
    
    // 如果没有遮挡物,完全光照
    if (avgBlockerDepth < 0.0) {
        return 1.0;
    }
    
    // 第二步: 半影半径计算
    float penumbra = penumbraSize(shadowCoord.z, avgBlockerDepth);
    float filterRadius = penumbra * texelSize;
    
    // 应用最小模糊半径 (Min Filter Size)
    filterRadius = max(filterRadius, pcss.u_MinFilterSize * texelSize);
    
    // 第三步: PCF滤波
    return PCF_Filter(shadowCoord, filterRadius);
}

// 计算单个方向光的PBR贡献
vec3 calculateDirectionalLight(vec3 N, vec3 V, vec3 albedo, vec3 F0, float roughness, vec3 worldPos) {
    // 如果平行光未启用,返回黑色
    if (pcss.enableDirectionalLight == 0u) {
        return vec3(0.0);
    }
    
    vec3 L = normalize(light.lightDir);
    vec3 H = normalize(V + L);
    
    // Cook-Torrance BRDF
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - length(F0 - vec3(0.04)) / 0.96);
    
    float NdotL = max(dot(N, L), 0.0);
    
    vec3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specularBRDF = numerator / denominator;
    
    vec3 diffuse = kD * albedo / PI;
    
    // 计算阴影因子
    float shadowFactor = calculatePCSSShadow(worldPos);
    
    return (diffuse + specularBRDF) * light.lightColor * NdotL * shadowFactor;
}

// 计算单个点光源的PBR贡献
vec3 calculatePointLight(int index, vec3 worldPos, vec3 N, vec3 V, vec3 albedo, vec3 F0, float roughness) {
    PointLight pl = pointLights.lights[index];
    
    vec3 lightVec = pl.position - worldPos;
    float distance = length(lightVec);
    
    // 超出半径则不计算
    if (distance > pl.radius) {
        return vec3(0.0);
    }
    
    vec3 L = normalize(lightVec);
    vec3 H = normalize(V + L);
    
    // 衰减
    float attenuation = calculateAttenuation(distance, pl.radius);
    vec3 radiance = pl.color * pl.intensity * attenuation;
    
    // Cook-Torrance BRDF
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - length(F0 - vec3(0.04)) / 0.96);
    
    float NdotL = max(dot(N, L), 0.0);
    
    vec3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specularBRDF = numerator / denominator;
    
    vec3 diffuse = kD * albedo / PI;
    
    return (diffuse + specularBRDF) * radiance * NdotL;
}

// PBR 着色 (ShadingID 0-100)
vec3 shadePBR(vec3 albedo, vec3 normal, vec3 specular, float smoothness, 
              float occlusion, vec3 emissive, vec3 worldPos) {
    float roughness = 1.0 - smoothness;
    
    vec3 N = normalize(normal);
    vec3 V = normalize(light.viewPos - worldPos);
    
    // F0 取 specular 通道
    vec3 F0 = specular;
    
    // 累加所有光源贡献
    vec3 Lo = vec3(0.0);
    
    // 方向光贡献
    Lo += calculateDirectionalLight(N, V, albedo, F0, roughness, worldPos);
    
    // 点光源贡献
    for (uint i = 0u; i < pointLights.count && i < MAX_POINT_LIGHTS; i++) {
        Lo += calculatePointLight(int(i), worldPos, N, V, albedo, F0, roughness);
    }
    
    // 漫反射环境光 (IBL 球谐光照)
    vec3 irradiance = evaluateSH(N) * skyboxParams.brightness;
    float NdotV = max(dot(N, V), 0.0);
    vec3 kS_env = fresnelSchlickRoughness(NdotV, F0, roughness);
    // kD: 金属度越高 (F0越大于0.04), 漫反射越弱，钳制到 [0,1]
    float metallic_approx = clamp(length(F0 - vec3(0.04)) / 0.96, 0.0, 1.0);
    vec3 kD_env = (1.0 - kS_env) * (1.0 - metallic_approx);
    vec3 ambient_diffuse = kD_env * irradiance * albedo * occlusion;

    // 镜面反射 IBL 高光 (Split-Sum)
    vec3 R = reflect(-V, N);
    float lod = roughness * float(6 - 1); // PREFILTER_MIP_LEVELS - 1
    // 显式 LOD 采样（textureLod 不是 LOD bias）
    vec3 prefilteredColor = textureLod(prefilteredMap, R, lod).rgb;
    // RGBA16F 线性空间，直接使用，无需 gamma 解码
    prefilteredColor *= skyboxParams.brightness;

    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular_ibl = prefilteredColor * (kS_env * brdf.x + brdf.y);

    // 漫反射用 AO，高光不额外乘以 AO（避免高金属度时过暗）
    vec3 ambient = ambient_diffuse + specular_ibl * occlusion;

    return ambient + Lo + emissive;
}

// 卡通着色 (ShadingID 101-200)
vec3 shadeCartoon(vec3 albedo, vec3 normal, vec3 emissive) {
    vec3 N = normalize(normal);
    vec3 L = normalize(light.lightDir);
    
    // 二值化漫反射
    float NdotL = dot(N, L);
    float diffuse = NdotL > 0.3 ? (NdotL > 0.6 ? 1.0 : 0.6) : 0.3;
    
    vec3 cartoonColor = albedo * diffuse * light.lightColor;
    
    // 边缘检测 (简化版本)
    // TODO: 使用 Sobel 算子进行更好的边缘检测
    
    return cartoonColor + emissive;
}

// 纯自发光材质 (ShadingID 201-255)
vec3 shadeEmissive(vec3 emissive) {
    return emissive;
}

// ===================== 主函数 =====================

void main() {
    // 采样 GBuffer
    vec4 g1 = texture(gbuffer1, fragTexCoord);
    vec4 g2 = texture(gbuffer2, fragTexCoord);
    vec4 g3 = texture(gbuffer3, fragTexCoord);
    vec4 g4 = texture(gbuffer4, fragTexCoord);
    float depth = texture(depthBuffer, fragTexCoord).r;
    
    // 如果深度为 1.0 (远平面), 渲染天空盒
    if (depth >= 0.9999) {
        // 用 invViewProj 重建视线方向（NDC -> 世界空间射线）
        vec2 ndc = fragTexCoord * 2.0 - 1.0;
        vec4 clipNear = vec4(ndc, 0.0, 1.0);
        vec4 clipFar  = vec4(ndc, 1.0, 1.0);
        vec4 worldNear = light.invViewProj * clipNear;
        vec4 worldFar  = light.invViewProj * clipFar;
        worldNear /= worldNear.w;
        worldFar  /= worldFar.w;
        vec3 rayDir = normalize(worldFar.xyz - worldNear.xyz);

        // 应用天空盒 rotationY（绕 Y 轴旋转）
        float sinR = sin(skyboxParams.rotationY);
        float cosR = cos(skyboxParams.rotationY);
        vec3 rotDir = vec3(
            rayDir.x * cosR + rayDir.z * sinR,
            rayDir.y,
            -rayDir.x * sinR + rayDir.z * cosR
        );

        // 采样 cubemap (lod=0 即原始全精度天空)
        // 使用原生分辨率渲染天空：直接在 composition pass 中采样
        vec3 skyColor = textureLod(prefilteredMap, rotDir, 0.0).rgb;
        skyColor *= skyboxParams.brightness;

        outColor = vec4(skyColor, 1.0);
        return;
    }
    
    // 解析 GBuffer
    vec3 albedo = g1.rgb;
    float materialFlags = g1.a;
    
    vec3 specular = g2.rgb;
    float occlusion = g2.a;
    
    vec3 normal = normalize(g3.rgb * 2.0 - 1.0);
    float smoothness = g3.a;
    
    uint shadingId = uint(g4.r * 255.0);
    // 直接读取自发光颜色 (GBA通道存储RGB)
    vec3 emissive = vec3(g4.g, g4.b, g4.a);
    
    // 重建世界位置
    vec4 clipPos = vec4(fragTexCoord * 2.0 - 1.0, depth, 1.0);
    vec4 worldPosH = light.invViewProj * clipPos;
    vec3 worldPos = worldPosH.xyz / worldPosH.w;
    
    // 根据着色 ID 选择着色逻辑
    vec3 finalColor;
    
    if (shadingId <= 100) {
        // PBR 着色
        finalColor = shadePBR(albedo, normal, specular, smoothness, 
                              occlusion, emissive, worldPos);
    } else if (shadingId <= 200) {
        // 卡通着色
        finalColor = shadeCartoon(albedo, normal, emissive);
    } else {
        // 纯自发光
        finalColor = shadeEmissive(emissive);
    }
    
    // Gamma 校正 (如果交换链是 UNORM 格式)
    // finalColor = pow(finalColor, vec3(1.0/2.2));
    
    outColor = vec4(finalColor, 1.0);
}
