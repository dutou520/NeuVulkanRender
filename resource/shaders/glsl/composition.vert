#version 450

// 输出到片段着色器
layout(location = 0) out vec2 fragTexCoord;

void main() {
    // 生成全屏三角形 (无需顶点缓冲)
    // 使用 3 个顶点覆盖整个屏幕
    // gl_VertexIndex: 0, 1, 2
    // 生成的坐标:
    //   0: (-1, -1) texCoord (0, 0)
    //   1: ( 3, -1) texCoord (2, 0)
    //   2: (-1,  3) texCoord (0, 2)
    
    fragTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragTexCoord * 2.0 - 1.0, 0.0, 1.0);
}
