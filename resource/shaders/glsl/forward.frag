#version 450

// 片段着色器输入
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragColor;

// 输出到HDR场景颜色
layout(location = 0) out vec4 outColor;

// 材质纹理 (set 1) - 与gbuffer共用布局
layout(set = 1, binding = 0) uniform sampler2D baseColorMap;
layout(set = 1, binding = 1) uniform sampler2D metallicRoughnessMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D emissiveMap;
layout(set = 1, binding = 4) uniform sampler2D occlusionMap;

// 光照数据 (set 2)
layout(set = 2, binding = 0) uniform LightData {
    vec3 lightDir;
    float _pad1;
    vec3 lightColor;
    float _pad2;
    vec3 viewPos;
    float _pad3;
} light;

// 材质参数 Push Constants (与gbuffer保持一致)
layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 baseColorFactor;       // 基础颜色因子
    layout(offset = 80) float metallicFactor;       // 金属度因子
    layout(offset = 84) float roughnessFactor;      // 粗糙度因子
    layout(offset = 88) float normalScale;          // 法线贴图强度
    layout(offset = 92) float alpha;                // 透明度
    layout(offset = 96) float shadingId;            // 着色模型 ID
    layout(offset = 100) float emissiveIntensity;   // 自发光强度
    layout(offset = 104) uint textureFlags;         // 纹理标志位
} material;

// ===================== PBR 函数 =====================

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

// ===================== 主函数 =====================

void main() {
    // ========== 基础颜色 ==========
    vec4 baseColor;
    if ((material.textureFlags & 1u) != 0u) {
        baseColor = texture(baseColorMap, fragTexCoord) * material.baseColorFactor;
    } else {
        baseColor = fragColor * material.baseColorFactor;
    }
    
    vec3 albedo = baseColor.rgb;
    float finalAlpha = baseColor.a * material.alpha;
    
    // Alpha test (for MASK mode)
    if (finalAlpha < 0.01) {
        discard;
    }
    
    // ========== 金属度/粗糙度 ==========
    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    if ((material.textureFlags & 2u) != 0u) {
        vec4 mr = texture(metallicRoughnessMap, fragTexCoord);
        metallic = mr.b * material.metallicFactor;
        roughness = mr.g * material.roughnessFactor;
    }
    
    // ========== 法线 ==========
    vec3 N = normalize(fragNormal);
    if ((material.textureFlags & 4u) != 0u) {
        vec3 tangentNormal = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
        tangentNormal.xy *= material.normalScale;
        tangentNormal = normalize(tangentNormal);
        
        // 屏幕空间导数近似TBN
        vec3 dpx = dFdx(fragPosition);
        vec3 dpy = dFdy(fragPosition);
        vec2 duvx = dFdx(fragTexCoord);
        vec2 duvy = dFdy(fragTexCoord);
        
        vec3 T = normalize(dpx * duvy.y - dpy * duvx.y);
        vec3 B = normalize(dpy * duvx.x - dpx * duvy.x);
        mat3 TBN = mat3(T, B, N);
        
        N = normalize(TBN * tangentNormal);
    }
    
    vec3 V = normalize(light.viewPos - fragPosition);
    vec3 L = normalize(light.lightDir);
    vec3 H = normalize(V + L);
    
    // F0 从金属度计算
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // Cook-Torrance BRDF
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    
    float NdotL = max(dot(N, L), 0.0);
    
    vec3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specular = numerator / denominator;
    
    vec3 diffuse = kD * albedo / 3.14159265;
    
    // 最终颜色
    vec3 Lo = (diffuse + specular) * light.lightColor * NdotL;
    
    // 环境光 (简化)
    vec3 ambient = vec3(0.03) * albedo;
    
    // 自发光
    vec3 emissive = vec3(0.0);
    if ((material.textureFlags & 8u) != 0u) {
        emissive = texture(emissiveMap, fragTexCoord).rgb * material.emissiveIntensity;
    } else if (material.emissiveIntensity > 0.0) {
        emissive = albedo * material.emissiveIntensity;
    }
    
    vec3 finalColor = ambient + Lo + emissive;
    
    // 输出带透明度的颜色
    outColor = vec4(finalColor, finalAlpha);
}
