#version 450

// 输入上一级的较小分辨率模糊纹理
layout(set = 0, binding = 0) uniform sampler2D srcTexture;

layout(push_constant) uniform UpsampleParams {
    vec2 texelSize; // 1.0 / 源纹理分辨率
    float radius;   // 扩散半径
    float pad;
} params;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    // 9-tap bilinear upsample filter (Dual Kawase)
    // 也能使用 3x3 盒滤波加权
    
    vec2 uv = fragTexCoord;
    vec2 srcTexelSize = params.texelSize;
    float x = params.radius * srcTexelSize.x;
    float y = params.radius * srcTexelSize.y;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(srcTexture, vec2(uv.x - x, uv.y + y)).rgb;
    vec3 b = texture(srcTexture, vec2(uv.x,     uv.y + y)).rgb;
    vec3 c = texture(srcTexture, vec2(uv.x + x, uv.y + y)).rgb;

    vec3 d = texture(srcTexture, vec2(uv.x - x, uv.y)).rgb;
    vec3 e = texture(srcTexture, vec2(uv.x,     uv.y)).rgb;
    vec3 f = texture(srcTexture, vec2(uv.x + x, uv.y)).rgb;

    vec3 g = texture(srcTexture, vec2(uv.x - x, uv.y - y)).rgb;
    vec3 h = texture(srcTexture, vec2(uv.x,     uv.y - y)).rgb;
    vec3 i = texture(srcTexture, vec2(uv.x + x, uv.y - y)).rgb;

    // Apply weights
    vec3 upsample = e * 4.0;
    upsample += (b + d + f + h) * 2.0;
    upsample += (a + c + g + i) * 1.0;
    upsample *= 1.0 / 16.0;

    outColor = vec4(upsample, 1.0);
}
