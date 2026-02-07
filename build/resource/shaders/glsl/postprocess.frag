#version 450

// 输入纹理
layout(set = 0, binding = 0) uniform sampler2D sceneColor;      // HDR场景颜色
layout(set = 0, binding = 1) uniform sampler2D depthBuffer;     // 深度缓冲
layout(set = 0, binding = 2) uniform sampler2D normalBuffer;    // 法线缓冲 (GBuffer3)
layout(set = 0, binding = 3) uniform sampler2D bloomTexture;    // Bloom模糊结果
layout(set = 0, binding = 4) uniform sampler2D ssaoNoise;       // SSAO噪声纹理

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
    float ssaoRadius;
    float ssaoStrength;
    uint debugMode; // 0=Shaded, 1=Wireframe, 2=Albedo, 3=Normal, 4=Depth
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

// ===================== 主函数 =====================

void main() {
    // Debug mode visualization
    if (settings.debugMode == 2) {
        // Albedo mode - sample from sceneColor which contains the composition output
        // We need to access GBuffer directly, but it's not bound here
        // For now, just show scene color
        vec3 color = texture(sceneColor, fragTexCoord).rgb;
        outColor = vec4(color, 1.0);
        return;
    }
    else if (settings.debugMode == 3) {
        // Normal mode
        vec3 normal = texture(normalBuffer, fragTexCoord).rgb;
        // Convert from [0,1] to [-1,1] and back to [0,1] for visualization
        normal = normal * 2.0 - 1.0;
        normal = normalize(normal);
        normal = normal * 0.5 + 0.5;
        outColor = vec4(normal, 1.0);
        return;
    }
    else if (settings.debugMode == 4) {
        // Depth mode
        float depth = texture(depthBuffer, fragTexCoord).r;
        // Linearize depth for better visualization
        float near = 0.1;
        float far = 100.0;
        float z = depth * 2.0 - 1.0;
        float linearDepth = (2.0 * near * far) / (far + near - z * (far - near));
        linearDepth = linearDepth / far; // Normalize to [0,1]
        outColor = vec4(vec3(linearDepth), 1.0);
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
