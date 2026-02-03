#version 450

// 输出到片段着色器
layout(location = 0) out vec2 fragTexCoord;

void main() {
    // 生成全屏三角形 (无需顶点缓冲)
    fragTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragTexCoord * 2.0 - 1.0, 0.0, 1.0);
}
