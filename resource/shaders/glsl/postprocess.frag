#version 450

// 输入纹理
layout(set = 0, binding = 0) uniform sampler2D sceneColor;      // HDR场景颜色
layout(set = 0, binding = 1) uniform sampler2D depthBuffer;     // 深度缓冲
layout(set = 0, binding = 2) uniform sampler2D normalBuffer;    // 法线缓冲 (GBuffer3)
layout(set = 0, binding = 3) uniform sampler2D bloomTexture;    // Bloom模糊结果
layout(set = 0, binding = 4) uniform sampler2D ssaoNoise;       // SSAO噪声纹理

// 新增 GBuffer 纹理用于调试
layout(set = 0, binding = 6) uniform sampler2D gbuffer1;      // Albedo + MaterialFlags
layout(set = 0, binding = 7) uniform sampler2D gbuffer2;      // Specular + Occlusion
layout(set = 0, binding = 8) uniform sampler2D gbuffer4;      // ShadingID + Emissive

// SSAO采样核
layout(set = 0, binding = 5) uniform SSAOKernel {
    vec4 samples[64];
} ssaoKernel;

// 视图矩阵用于SSAO
layout(set = 1, binding = 0) uniform CameraData {
    mat4 view;
    mat4 projection;
    mat4 invView;
    mat4 invProjection;
    vec2 viewportSize;
    float nearPlane;
    float farPlane;
} camera;

// 后处理设置 Push Constants
layout(push_constant) uniform PostProcessSettings {
    uint enableSSAO;
    uint enableBloom;
    uint enableToneMapping;
    uint enableGamma;
    float bloomIntensity;
    float bloomThreshold;
    float bloomRadius; // 对齐 C++
    float ssaoRadius;
    float ssaoStrength;
    uint debugMode; // 0=Shaded, 1=Wireframe, 2=Albedo, 3=Normal, 4=Depth, 5=Smoothness, 6=Specular, 7=Occlusion, 8=MaterialFlags, 9=ShadingID, 10=Emission
    uint enableSSR;
    uint ssrMaxSteps;
    float ssrStepSize;
    float ssrThickness;
    float ssrStrength;
    uint enableSSRSpatial; // 对齐 C++
} settings;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

// ===================== SSAO =====================

// 从深度重建视图空间位置
vec3 reconstructViewPosition(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = camera.invProjection * clipPos;
    return viewPos.xyz / viewPos.w;
}

float calculateSSAO(vec2 uv) {
    float depth = texture(depthBuffer, uv).r;
    if (depth >= 0.9999) return 1.0; // 天空区域
    
    vec3 fragPos = reconstructViewPosition(uv, depth);
    
    // 获取法线 (从GBuffer解码)
    vec3 normal = texture(normalBuffer, uv).rgb * 2.0 - 1.0;
    normal = mat3(camera.view) * normal; // 转换到视图空间
    normal = normalize(normal);
    
    // 获取随机向量用于旋转采样核
    vec2 noiseScale = camera.viewportSize / 4.0;
    vec3 randomVec = texture(ssaoNoise, uv * noiseScale).xyz * 2.0 - 1.0;
    
    // 创建TBN矩阵
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    // 采样核遍历
    float occlusion = 0.0;
    int sampleCount = 32; // 使用32个采样点
    
    for (int i = 0; i < sampleCount; ++i) {
        // 获取采样点位置
        vec3 samplePos = TBN * ssaoKernel.samples[i].xyz;
        samplePos = fragPos + samplePos * settings.ssaoRadius;
        
        // 投影采样点到屏幕空间
        vec4 offset = camera.projection * vec4(samplePos, 1.0);
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        
        // 获取采样点深度
        float sampleDepth = texture(depthBuffer, offset.xy).r;
        vec3 sampleViewPos = reconstructViewPosition(offset.xy, sampleDepth);
        
        // 范围检查和遮蔽计算
        float rangeCheck = smoothstep(0.0, 1.0, settings.ssaoRadius / abs(fragPos.z - sampleViewPos.z));
        occlusion += (sampleViewPos.z >= samplePos.z + 0.025 ? 1.0 : 0.0) * rangeCheck;
    }
    
    occlusion = 1.0 - (occlusion / float(sampleCount));
    return pow(occlusion, settings.ssaoStrength);
}

// ===================== SSR (Screen Space Reflection) =====================

vec3 calculateSSR(vec2 uv, vec3 fragPos, vec3 normalVS, float smoothness, vec3 specular) {
    if (smoothness < 0.01) return vec3(0.0);

    vec3 viewDir = normalize(fragPos);
    vec3 reflectDir = reflect(viewDir, normalVS);

    // 如果反射方向朝向相机（Z正方向），屏幕空间内无法投射，尽早剔除
    if (reflectDir.z > 0.3) return vec3(0.0);

    int maxSteps = int(settings.ssrMaxSteps);
    float stepSize = settings.ssrStepSize;
    float thickness = settings.ssrThickness;

    vec3 currentRayPos = fragPos;
    vec2 hitUV = vec2(0.0);
    bool hit = false;

    for (int i = 0; i < maxSteps; i++) {
        currentRayPos += reflectDir * stepSize;

        // 投影到屏幕空间
        vec4 projectedPos = camera.projection * vec4(currentRayPos, 1.0);
        projectedPos.xyz /= projectedPos.w;
        vec2 sampleUV = projectedPos.xy * 0.5 + 0.5;

        // 边界检查
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
            break;
        }

        // 采样深度并重建位置
        float sampledDepth = texture(depthBuffer, sampleUV).r;
        if (sampledDepth >= 0.9999) continue;

        vec3 sampledPos = reconstructViewPosition(sampleUV, sampledDepth);

        float depthDiff = sampledPos.z - currentRayPos.z;
        // 在观察空间中，Z值是负的，且负得越多说明越远
        if (depthDiff >= 0.0 && depthDiff < thickness) {
            // 发生碰撞，使用二分查找精细调整
            vec3 minRayPos = currentRayPos - reflectDir * stepSize;
            vec3 maxRayPos = currentRayPos;
            
            for (int j = 0; j < 5; j++) {
                vec3 midRayPos = mix(minRayPos, maxRayPos, 0.5);
                vec4 midProj = camera.projection * vec4(midRayPos, 1.0);
                midProj.xyz /= midProj.w;
                vec2 midUV = midProj.xy * 0.5 + 0.5;
                
                float d = texture(depthBuffer, midUV).r;
                vec3 p = reconstructViewPosition(midUV, d);
                
                float diff = p.z - midRayPos.z;
                if (diff >= 0.0 && diff < thickness) {
                    maxRayPos = midRayPos;
                    hitUV = midUV;
                } else {
                    minRayPos = midRayPos;
                }
            }
            hit = true;
            break;
        }
    }

    if (hit) {
        // 边缘渐变淡出
        vec2 edge = vec2(1.0) - abs(hitUV * 2.0 - 1.0);
        float edgeFade = clamp(edge.x * edge.y * 10.0, 0.0, 1.0);

        // 采样反射点颜色
        vec3 reflectedColor = texture(sceneColor, hitUV).rgb;

        // 菲涅尔效应
        float cosTheta = max(dot(normalVS, -viewDir), 0.0);
        // fresnelSchlick F0 = specular
        vec3 F = specular + (vec3(1.0) - specular) * pow(1.0 - cosTheta, 5.0);

        return reflectedColor * F * smoothness * settings.ssrStrength * edgeFade;
    }

    return vec3(0.0);
}

// ===================== 色调映射 =====================

// ACES Filmic Tone Mapping
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ===================== Gamma 校正 =====================

vec3 gammaCorrect(vec3 color) {
    return pow(color, vec3(1.0 / 2.2));
}

// 辅助函数：HSL转RGB（用于自发光色相计算）
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

// ===================== 主函数 =====================

void main() {
    // Debug mode visualization
    if (settings.debugMode == 2) {
        // Albedo mode
        vec3 albedo = texture(gbuffer1, fragTexCoord).rgb;
        outColor = vec4(albedo, 1.0);
        return;
    }
    else if (settings.debugMode == 3) {
        // Normal mode
        vec3 normal = texture(normalBuffer, fragTexCoord).rgb;
        normal = normal * 2.0 - 1.0;
        normal = normalize(normal);
        normal = normal * 0.5 + 0.5;
        outColor = vec4(normal, 1.0);
        return;
    }
    else if (settings.debugMode == 4) {
        // Depth mode
        float depth = texture(depthBuffer, fragTexCoord).r;
        float near = camera.nearPlane;
        float far = camera.farPlane;
        float z = depth * 2.0 - 1.0;
        float linearDepth = (2.0 * near * far) / (far + near - z * (far - near));
        linearDepth = linearDepth / far;
        outColor = vec4(vec3(linearDepth), 1.0);
        return;
    }
    else if (settings.debugMode == 5) {
        // Smoothness mode
        float smoothness = texture(normalBuffer, fragTexCoord).a;
        outColor = vec4(vec3(smoothness), 1.0);
        return;
    }
    else if (settings.debugMode == 6) {
        // Specular mode
        vec3 specular = texture(gbuffer2, fragTexCoord).rgb;
        outColor = vec4(specular, 1.0);
        return;
    }
    else if (settings.debugMode == 7) {
        // Occlusion mode
        float occlusion = texture(gbuffer2, fragTexCoord).a;
        outColor = vec4(vec3(occlusion), 1.0);
        return;
    }
    else if (settings.debugMode == 8) {
        // MaterialFlags mode
        float flags = texture(gbuffer1, fragTexCoord).a;
        outColor = vec4(vec3(flags), 1.0);
        return;
    }
    else if (settings.debugMode == 9) {
        // ShadingID mode
        float id = texture(gbuffer4, fragTexCoord).r;
        // Distribute the ID visualize it better
        vec3 idColor = vec3(
            mod(id * 255.0, 4.0) / 3.0,
            mod(id * 255.0 / 4.0, 4.0) / 3.0,
            mod(id * 255.0 / 16.0, 4.0) / 3.0
        );
        outColor = vec4(idColor, 1.0);
        return;
    }
    else if (settings.debugMode == 10) {
        // Emission mode
        vec4 g4 = texture(gbuffer4, fragTexCoord);
        float hue = g4.a * 255.0;
        vec2 brightness = g4.gb;
        float l = (brightness.r * 255.0) * 256.0 + (brightness.g * 255.0);
        l = l / 65535.0 * 10.0; // Scale for visualization
        
        vec3 emissive = hslToRgb(vec3(hue * 360.0 / 255.0, 1.0, l));
        // Apply tonemapping to emission debug for better visibility
        vec3 mapped = emissive;
        if (settings.enableToneMapping != 0) {
            mapped = ACESFilm(emissive);
        }
        outColor = vec4(mapped, 1.0);
        return;
    }
    
    // Default: Shaded mode (debugMode == 0 or 1)
    // Note: Wireframe (debugMode == 1) would need geometry shader support
    
    // 采样场景颜色 (HDR)
    vec3 hdrColor = texture(sceneColor, fragTexCoord).rgb;
    
    // SSAO
    float ao = 1.0;
    if (settings.enableSSAO != 0) {
        ao = calculateSSAO(fragTexCoord);
        hdrColor *= ao;
    }

    // SSR 已经移至 TAA Pass (taa.comp) 内部处理，以使 TAA 能够直接作用于反射噪点。
    // 在此处屏蔽二次 SSR 叠加。
    
    // Bloom
    if (settings.enableBloom != 0) {
        vec3 bloomColor = texture(bloomTexture, fragTexCoord).rgb;
        hdrColor += bloomColor * settings.bloomIntensity;
    }
    
    // 色调映射
    vec3 mapped = hdrColor;
    if (settings.enableToneMapping != 0) {
        mapped = ACESFilm(hdrColor);
    }
    
    // Gamma 校正
    vec3 finalColor = mapped;
    if (settings.enableGamma != 0) {
        finalColor = gammaCorrect(mapped);
    }
    
    outColor = vec4(finalColor, 1.0);
}
