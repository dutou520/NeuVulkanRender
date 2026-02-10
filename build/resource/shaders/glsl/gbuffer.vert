#version 450

// MVP 矩阵 Uniform Buffer (View + Proj)
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// Push Constants for Model Matrix
layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

// 顶点输入
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inTangent;

// 片段着色器输出
layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragColor;
layout(location = 4) out vec4 fragTangent;

void main() {
    // 世界空间位置
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    fragPosition = worldPos.xyz;
    
    // 裁剪空间位置
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    // 法线变换到世界空间 (使用法线矩阵的近似)
    fragNormal = mat3(transpose(inverse(pc.model))) * inNormal;
    
    // 传递纹理坐标和颜色
    fragTexCoord = inTexCoord;
    fragColor = inColor;
    
    // 切线变换到世界空间
    if (length(inTangent.xyz) > 0.0001) {
        fragTangent.xyz = normalize(mat3(pc.model) * inTangent.xyz);
    } else {
        fragTangent.xyz = vec3(0.0);
    }
    fragTangent.w = inTangent.w;
}
