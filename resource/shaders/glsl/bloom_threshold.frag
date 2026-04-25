#version 450

// 输入HDR场景
layout(set = 0, binding = 0) uniform sampler2D sceneColor;

// 提取阈值
layout(push_constant) uniform ThresholdParams {
    float threshold;
    float softThreshold;
    vec2 texelSize;
} params;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = fragTexCoord;
    vec2 halfTexel = params.texelSize * 0.5;

    // 4-tap box filter downsample to prevent checkerboard artifacts
    vec3 c0 = texture(sceneColor, uv + vec2(-halfTexel.x, -halfTexel.y)).rgb;
    vec3 c1 = texture(sceneColor, uv + vec2( halfTexel.x, -halfTexel.y)).rgb;
    vec3 c2 = texture(sceneColor, uv + vec2(-halfTexel.x,  halfTexel.y)).rgb;
    vec3 c3 = texture(sceneColor, uv + vec2( halfTexel.x,  halfTexel.y)).rgb;
    vec3 color = (c0 + c1 + c2 + c3) * 0.25;
    
    // Fix NaN and Inf causing black squares and grid artifacts
    if (any(isnan(color)) || any(isinf(color))) {
        color = vec3(0.0);
    }
    
    // Prevent extreme fireflies from blowing up the bloom filter
    color = clamp(color, 0.0, 100.0);
    
    // 计算亮度
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
    // 软阈值提取
    float soft = brightness - params.threshold + params.softThreshold;
    soft = clamp(soft / (2.0 * params.softThreshold + 0.00001), 0.0, 1.0);
    soft = soft * soft;
    
    float contribution = max(soft, step(params.threshold, brightness));
    
    outColor = vec4(color * contribution, 1.0);
}
