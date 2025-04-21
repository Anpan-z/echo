#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    mat4 lightSpaceMatrix;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 fragNormal;
layout(location = 4) out flat vec3 outViewPos;
layout(location = 5) out vec4 fragPosLightSpace;

void main() {
    fragPos  = vec3(ubo.model * vec4(inPosition, 1.0));
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
    fragColor = inColor;
    outViewPos = ubo.viewPos;

    // 用于 shadow map 的位置（light space 下）
    fragPosLightSpace = ubo.lightSpaceMatrix  * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    //fragTexCoord = inTexCoord;
}