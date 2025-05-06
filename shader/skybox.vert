#version 450

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform Camera {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec3 viewPos;
    mat4 lightSpaceMatrix;
} camera;

layout(location = 0) out vec3 texCoords;

void main() {
    texCoords = inPosition;
    vec4 pos = camera.projection * mat4(mat3(camera.view)) * vec4(inPosition, 1.0);
    gl_Position = pos.xyww; // 保持深度值为 1.0
}