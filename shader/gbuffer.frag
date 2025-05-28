#version 450

layout(location = 0) in vec3 fragPosition;  // 从顶点着色器传递的世界空间位置
layout(location = 1) in vec3 fragNormal;    // 从顶点着色器传递的世界空间法线
layout(location = 2) in vec3 fragColor;     // 从顶点着色器传递的顶点颜色

layout(location = 0) out vec4 outColor;     // 输出到颜色附件
layout(location = 1) out vec4 outNormal;    // 输出到法线附件
layout(location = 2) out vec4 outPosition;  // 输出到位置附件

void main() {
    // 输出漫反射颜色
    outColor = vec4(fragColor, 1.0);

    // 输出法线（转换到 [0, 1] 范围以存储）
    outNormal = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);

    // 输出世界空间位置
    outPosition = vec4(fragPosition, 1.0);
}