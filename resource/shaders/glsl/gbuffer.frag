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
layout(location = 3) out vec4 outShadingEmissive;   // GBuffer4: R=ShadingID, G=EmissiveR, B=EmissiveG, A=EmissiveB * intensity

// 材质参数 Push Constants
layout(push_constant) uniform PushConstants {
    layout(offset = 64) float metallic;
    layout(offset = 68) float roughness;
    layout(offset = 72) float shadingId;
    layout(offset = 76) float emissiveIntensity;
} material;

void main() {
    // 归一化法线
    vec3 normal = normalize(fragNormal);
    
    // GBuffer1: Albedo + MaterialFlags
    outAlbedoFlags = vec4(fragColor.rgb, 0.0);
    
    // GBuffer2: Specular + Occlusion
    vec3 specularColor = mix(vec3(0.04), fragColor.rgb, material.metallic);
    float occlusion = 1.0;
    outSpecularOcclusion = vec4(specularColor, occlusion);
    
    // GBuffer3: Normal + Smoothness
    vec3 encodedNormal = normal * 0.5 + 0.5;
    float smoothness = 1.0 - material.roughness;
    outNormalSmoothness = vec4(encodedNormal, smoothness);
    
    // GBuffer4: ShadingID + Emissive Color (使用albedo作为自发光颜色)
    // R = 着色ID归一化
    // GBA = 自发光颜色 * 强度 (HDR)
    float shadingIdNorm = material.shadingId / 255.0;
    vec3 emissiveColor = fragColor.rgb * material.emissiveIntensity;
    outShadingEmissive = vec4(shadingIdNorm, emissiveColor.r, emissiveColor.g, emissiveColor.b);
}
