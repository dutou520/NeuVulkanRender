#version 450

// 输入上一级的大分辨率纹理
layout(set = 0, binding = 0) uniform sampler2D srcTexture;

layout(push_constant) uniform DownsampleParams {
    vec2 texelSize; // 1.0 / 源纹理分辨率
    float mipLevel; // 可选的，用于一些特定的计算，但基础实现中无需，占位用
    float pad;
} params;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    // 13-tap Dual Kawase Downsample filter
    // https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_marius_2D00_notes.pdf
    
    vec2 uv = fragTexCoord;
    vec2 srcTexelSize = params.texelSize;
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(srcTexture, vec2(uv.x - 2.0*x, uv.y + 2.0*y)).rgb;
    vec3 b = texture(srcTexture, vec2(uv.x,         uv.y + 2.0*y)).rgb;
    vec3 c = texture(srcTexture, vec2(uv.x + 2.0*x, uv.y + 2.0*y)).rgb;

    vec3 d = texture(srcTexture, vec2(uv.x - 2.0*x, uv.y)).rgb;
    vec3 e = texture(srcTexture, vec2(uv.x,         uv.y)).rgb;
    vec3 f = texture(srcTexture, vec2(uv.x + 2.0*x, uv.y)).rgb;

    vec3 g = texture(srcTexture, vec2(uv.x - 2.0*x, uv.y - 2.0*y)).rgb;
    vec3 h = texture(srcTexture, vec2(uv.x,         uv.y - 2.0*y)).rgb;
    vec3 i = texture(srcTexture, vec2(uv.x + 2.0*x, uv.y - 2.0*y)).rgb;

    vec3 j = texture(srcTexture, vec2(uv.x - x, uv.y + y)).rgb;
    vec3 k = texture(srcTexture, vec2(uv.x + x, uv.y + y)).rgb;
    vec3 l = texture(srcTexture, vec2(uv.x - x, uv.y - y)).rgb;
    vec3 m = texture(srcTexture, vec2(uv.x + x, uv.y - y)).rgb;

    // Apply Kawase weights
    vec3 downsample = e * 0.125;
    downsample += (a + c + g + i) * 0.03125;
    downsample += (b + d + f + h) * 0.0625;
    downsample += (j + k + l + m) * 0.125;

    // 为了防止过亮的像素闪烁（Firefly），通常可以在降采样时做一个亮度抑制，
    // 或者用 Karis Average，既然这里是基础的 Bloom，我们先保留无偏降采样即可。
    outColor = vec4(max(downsample, vec3(0.0)), 1.0);
}
