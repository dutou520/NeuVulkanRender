#version 450

// 输入纹理
layout(set = 0, binding = 0) uniform sampler2D inputTexture;

// 模糊参数
layout(push_constant) uniform BlurParams {
    vec2 direction;    // (1,0) 水平, (0,1) 垂直
    vec2 texelSize;    // 1.0 / textureSize
} params;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

// 高斯权重 (9-tap)
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec3 result = texture(inputTexture, fragTexCoord).rgb * weights[0];
    
    for (int i = 1; i < 5; ++i) {
        vec2 offset = params.direction * params.texelSize * float(i);
        result += texture(inputTexture, fragTexCoord + offset).rgb * weights[i];
        result += texture(inputTexture, fragTexCoord - offset).rgb * weights[i];
    }
    
    outColor = vec4(result, 1.0);
}
