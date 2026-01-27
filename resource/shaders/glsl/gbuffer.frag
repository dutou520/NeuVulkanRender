#version 450

// 片段着色器输入
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragColor;

// GBuffer 输出 (Multiple Render Targets)
layout(location = 0) out vec4 outAlbedoFlags;       // GBuffer1: RGB=Albedo(sRGB), A=MaterialFlags
layout(location = 1) out vec4 outSpecularOcclusion; // GBuffer2: RGB=Specular, A=Occlusion
layout(location = 2) out vec4 outNormalSmoothness;  // GBuffer3: RGB=Normal, A=Smoothness
layout(location = 3) out vec4 outShadingEmissive;   // GBuffer4: R=ShadingID, GB=EmissiveBrightness, A=EmissiveHue

// 材质参数 Push Constants
layout(push_constant) uniform PushConstants {
    float metallic;
    float roughness;
    float shadingId;
    float emissiveIntensity;
} material;

void main() {
    // 归一化法线
    vec3 normal = normalize(fragNormal);
    
    // GBuffer1: Albedo + MaterialFlags
    // MaterialFlags: 0 = 不透明, 1-255 可用于其他标记
    outAlbedoFlags = vec4(fragColor.rgb, 0.0);
    
    // GBuffer2: Specular + Occlusion
    // 从金属度计算镜面反射 (简化的 PBR)
    vec3 specularColor = mix(vec3(0.04), fragColor.rgb, material.metallic);
    float occlusion = 1.0; // 默认无遮蔽
    outSpecularOcclusion = vec4(specularColor, occlusion);
    
    // GBuffer3: Normal + Smoothness
    // 法线压缩到 [0, 1] 范围
    vec3 encodedNormal = normal * 0.5 + 0.5;
    float smoothness = 1.0 - material.roughness;
    outNormalSmoothness = vec4(encodedNormal, smoothness);
    
    // GBuffer4: ShadingID + Emissive
    // R = 着色ID (0-255, 0=PBR, 101+=卡通, 201+=纯自发光)
    // GB = 自发光亮度 (16-bit HDR编码)
    // A = 自发光色相 (0-255)
    float shadingIdNorm = material.shadingId / 255.0;
    float emissiveBrightness = material.emissiveIntensity;
    outShadingEmissive = vec4(shadingIdNorm, emissiveBrightness, 0.0, 0.0);
}
