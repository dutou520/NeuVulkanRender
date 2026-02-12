#version 450

// MVP 矩阵 Uniform Buffer (Light View + Proj)
layout(binding = 0) uniform LightUniformBufferObject {
    mat4 lightVP;
} lightUBO;

// Push Constants for Model Matrix
layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

// 顶点输入
layout(location = 0) in vec3 inPosition;

void main() {
    // 直接变换到光源裁剪空间
    gl_Position = lightUBO.lightVP * pc.model * vec4(inPosition, 1.0);
}
