#version 450

// 片段着色器输入
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragColor;
layout(location = 4) in vec4 fragTangent;

// GBuffer 输出 (Multiple Render Targets)
layout(location = 0) out vec4 outAlbedoFlags;       // GBuffer1: RGB=Albedo(sRGB), A=MaterialFlags
layout(location = 1) out vec4 outSpecularOcclusion; // GBuffer2: RGB=Specular, A=Occlusion
layout(location = 2) out vec4 outNormalSmoothness;  // GBuffer3: RGB=Normal, A=Smoothness
layout(location = 3) out vec4 outShadingEmissive;   // GBuffer4: R=ShadingID, G=EmissiveR, B=EmissiveG, A=EmissiveB * intensity

// 材质纹理 (set 1)
layout(set = 1, binding = 0) uniform sampler2D baseColorMap;
layout(set = 1, binding = 1) uniform sampler2D metallicMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D emissiveMap;
layout(set = 1, binding = 4) uniform sampler2D occlusionMap;
layout(set = 1, binding = 5) uniform sampler2D roughnessMap;

// 材质参数 Push Constants
// offset 0-63: mat4 model (in vertex shader)
// offset 64+: material parameters
layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 baseColorFactor;       // 基础颜色因子
    layout(offset = 80) float metallicFactor;       // 金属度因子
    layout(offset = 84) float roughnessFactor;      // 粗糙度因子
    layout(offset = 88) float normalScale;          // 法线贴图强度
    layout(offset = 92) float occlusionStrength;    // AO 强度
    layout(offset = 96) float shadingId;            // 着色模型 ID
    layout(offset = 100) float emissiveIntensity;   // 自发光强度
    layout(offset = 104) uint textureFlags;         // 纹理标志位
    // bit 0: useBaseColorMap
    // bit 1: useMetallicMap
    // bit 2: useNormalMap
    // bit 3: useEmissiveMap
    // bit 4: useOcclusionMap
    // bit 5: useRoughnessMap
} material;

void main() {
    // ========== 基础颜色 ==========
    vec4 baseColor;
    if ((material.textureFlags & 1u) != 0u) {
        // 从纹理采样并乘以因子
        baseColor = texture(baseColorMap, fragTexCoord) * material.baseColorFactor;
    } else {
        // 使用顶点颜色乘以因子
        baseColor = fragColor * material.baseColorFactor;
    }
    
    // ========== 金属度/粗糙度 ==========
    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    
    if ((material.textureFlags & 2u) != 0u) {
        // Metallic: 通常存储在 B 或 R 通道，取决于导出约定。这里暂时保持 B 通道逻辑但仅从独立贴图采样。
        metallic = texture(metallicMap, fragTexCoord).b * material.metallicFactor;
    }
    
    if ((material.textureFlags & 32u) != 0u) {
        // Roughness: 通常存储在 G 通道。
        roughness = texture(roughnessMap, fragTexCoord).g * material.roughnessFactor;
    }
    
    // ========== 法线 ==========
    vec3 normal = normalize(fragNormal);
    if ((material.textureFlags & 4u) != 0u) {
        // 采样法线贴图
        vec3 tangentNormal = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
        tangentNormal.xy *= material.normalScale;
        tangentNormal = normalize(tangentNormal);
        
        // 使用顶点切线构建 TBN 矩阵，如果缺失则回退到屏幕空间导数近似
        vec3 N = normalize(fragNormal);
        vec3 T, B;
        
        if (length(fragTangent.xyz) > 1e-4) {
            T = normalize(fragTangent.xyz);
            // 重新正交化切线 (Gram-Schmidt process)
            T = normalize(T - dot(T, N) * N);
            // 计算投影到世界空间的副法线，考虑右手系/左手系 (fragTangent.w)
            B = cross(N, T) * fragTangent.w;
        } else {
            // 在没有切线数据时，使用屏幕空间导数近似TBN
            vec3 dpx = dFdx(fragPosition);
            vec3 dpy = dFdy(fragPosition);
            vec2 duvx = dFdx(fragTexCoord);
            vec2 duvy = dFdy(fragTexCoord);
            T = normalize(dpx * duvy.y - dpy * duvx.y);
            B = normalize(dpy * duvx.x - dpx * duvy.x);
        }
        
        mat3 TBN = mat3(T, B, N);
        normal = normalize(TBN * tangentNormal);
    }
    
    // ========== 环境遮蔽 ==========
    float occlusion = 1.0;
    if ((material.textureFlags & 16u) != 0u) {
        occlusion = texture(occlusionMap, fragTexCoord).r;
        occlusion = 1.0 + material.occlusionStrength * (occlusion - 1.0);
    }
    
    // ========== 自发光 ==========
    vec3 emissiveColor = vec3(0.0);
    if ((material.textureFlags & 8u) != 0u) {
        emissiveColor = texture(emissiveMap, fragTexCoord).rgb * material.emissiveIntensity;
    } else if (material.emissiveIntensity > 0.0) {
        emissiveColor = baseColor.rgb * material.emissiveIntensity;
    }
    
    // ========== GBuffer 输出 ==========
    
    // GBuffer1: Albedo + MaterialFlags
    outAlbedoFlags = vec4(baseColor.rgb, 0.0);
    
    // GBuffer2: Specular + Occlusion
    vec3 specularColor = mix(vec3(0.04), baseColor.rgb, metallic);
    outSpecularOcclusion = vec4(specularColor, occlusion);
    
    // GBuffer3: Normal + Smoothness
    vec3 encodedNormal = normal * 0.5 + 0.5;
    float smoothness = 1.0 - roughness;
    outNormalSmoothness = vec4(encodedNormal, smoothness);
    
    // GBuffer4: ShadingID + Emissive Color
    float shadingIdNorm = material.shadingId / 255.0;
    outShadingEmissive = vec4(shadingIdNorm, emissiveColor.r, emissiveColor.g, emissiveColor.b);
}
