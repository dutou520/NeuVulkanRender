#version 450

// 输入HDR场景
layout(set = 0, binding = 0) uniform sampler2D sceneColor;

// 提取阈值
layout(push_constant) uniform ThresholdParams {
    float threshold;
    float softThreshold;
    vec2 _pad;
} params;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = texture(sceneColor, fragTexCoord).rgb;
    
    // 计算亮度
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
    // 软阈值提取
    float soft = brightness - params.threshold + params.softThreshold;
    soft = clamp(soft / (2.0 * params.softThreshold + 0.00001), 0.0, 1.0);
    soft = soft * soft;
    
    float contribution = max(soft, step(params.threshold, brightness));
    
    outColor = vec4(color * contribution, 1.0);
}
