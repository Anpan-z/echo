#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in uint inShapeID;

layout(location = 0) out vec3 fragPosition;  // 传递到片段着色器的世界空间位置
layout(location = 1) out vec3 fragNormal;    // 传递到片段着色器的世界空间法线
layout(location = 2) out vec3 fragColor;     // 传递到片段着色器的顶点颜色

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;       // 模型矩阵
    mat4 view;        // 视图矩阵
    mat4 projection;  // 投影矩阵
} ubo;

void main() {
    // 计算世界空间位置
    vec4 worldPosition = ubo.model * vec4(inPosition, 1.0);
    fragPosition = worldPosition.xyz;

    // 计算世界空间法线
    fragNormal = normalize(mat3(ubo.model) * inNormal);

    // 传递顶点颜色
    fragColor = inColor;

    // 计算裁剪空间位置
    gl_Position = ubo.projection * ubo.view * worldPosition;
}