#version 450

// 片段着色器输入
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragColor;

// 输出到HDR场景颜色
layout(location = 0) out vec4 outColor;

// 光照数据
layout(set = 1, binding = 0) uniform LightData {
    vec3 lightDir;
    float _pad1;
    vec3 lightColor;
    float _pad2;
    vec3 viewPos;
    float _pad3;
} light;

// 材质参数 Push Constants
layout(push_constant) uniform PushConstants {
    layout(offset = 64) float metallic;
    layout(offset = 68) float roughness;
    layout(offset = 72) float alpha;
    layout(offset = 76) float emissiveIntensity;
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
    vec3 albedo = fragColor.rgb;
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(light.viewPos - fragPosition);
    vec3 L = normalize(light.lightDir);
    vec3 H = normalize(V + L);
    
    // F0 从金属度计算
    vec3 F0 = mix(vec3(0.04), albedo, material.metallic);
    
    // Cook-Torrance BRDF
    float D = distributionGGX(N, H, material.roughness);
    float G = geometrySmith(N, V, L, material.roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - material.metallic);
    
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
    vec3 emissive = albedo * material.emissiveIntensity;
    
    vec3 finalColor = ambient + Lo + emissive;
    
    // 输出带透明度的颜色
    outColor = vec4(finalColor, material.alpha);
}
