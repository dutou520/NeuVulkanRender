#version 450

// GBuffer 采样器
layout(set = 0, binding = 0) uniform sampler2D gbuffer1; // Albedo + MaterialFlags
layout(set = 0, binding = 1) uniform sampler2D gbuffer2; // Specular + Occlusion
layout(set = 0, binding = 2) uniform sampler2D gbuffer3; // Normal + Smoothness
layout(set = 0, binding = 3) uniform sampler2D gbuffer4; // ShadingID + Emissive
layout(set = 0, binding = 4) uniform sampler2D depthBuffer; // Depth

// 光照数据
layout(set = 1, binding = 0) uniform LightData {
    vec3 lightDir;
    float _pad1;
    vec3 lightColor;
    float _pad2;
    vec3 viewPos;
    float _pad3;
} light;

// 视口信息
layout(push_constant) uniform PushConstants {
    vec2 viewportSize;
} pc;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

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

// Schlick Fresnel 近似
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// GGX 分布函数
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265 * denom * denom;
    
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

// ===================== 着色函数 =====================

// PBR 着色 (ShadingID 0-100)
vec3 shadePBR(vec3 albedo, vec3 normal, vec3 specular, float smoothness, 
              float occlusion, vec3 emissive, vec3 worldPos) {
    float roughness = 1.0 - smoothness;
    
    vec3 N = normalize(normal);
    vec3 V = normalize(light.viewPos - worldPos);
    vec3 L = normalize(light.lightDir);
    vec3 H = normalize(V + L);
    
    // F0 取 specular 通道
    vec3 F0 = specular;
    
    // Cook-Torrance BRDF
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - length(specular - vec3(0.04)) / 0.96);
    
    float NdotL = max(dot(N, L), 0.0);
    
    vec3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specularBRDF = numerator / denominator;
    
    vec3 diffuse = kD * albedo / 3.14159265;
    
    // 最终颜色
    vec3 Lo = (diffuse + specularBRDF) * light.lightColor * NdotL;
    
    // 环境光 (简化)
    vec3 ambient = vec3(0.03) * albedo * occlusion;
    
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
    
    // 如果深度为 1.0 (远平面), 显示背景色
    if (depth >= 0.9999) {
        outColor = vec4(0.1, 0.1, 0.15, 1.0); // 深灰蓝背景
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
    float emissiveBrightness = g4.g;
    float emissiveHue = g4.a;
    vec3 emissive = getEmissiveColor(emissiveHue, emissiveBrightness);
    
    // 重建世界位置 (简化版, 实际应使用深度)
    // TODO: 从深度重建世界位置
    vec3 worldPos = vec3(0.0);
    
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
