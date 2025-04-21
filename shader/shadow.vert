#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform LightMVP {
    mat4 lightSpaceMatrix;
} ubo;

void main() {
    gl_Position = ubo.lightSpaceMatrix  * vec4(inPosition, 1.0);
}