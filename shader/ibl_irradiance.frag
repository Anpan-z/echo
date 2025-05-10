#version 450
//余弦加权的采样积分 Cosine-weighted Hemisphere Sampling
layout(location = 0) in vec3 localPos;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform samplerCube environmentMap;

const float PI = 3.14159265359;

void main() {
    vec3 normal = normalize(localPos); 
    vec3 irradiance = vec3(0.0);

    // 构造 TBN basis
    vec3 up = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = normalize(cross(normal, tangent));

    // 对环境贴图进行离散采样
    const int SAMPLE_COUNT = 256; // 采样数量
    int sqrtSampleCount = int(sqrt(SAMPLE_COUNT)); // 每个维度的采样数量

    for (int y = 0; y < sqrtSampleCount; ++y) {
        for (int x = 0; x < sqrtSampleCount; ++x) {
            // 计算步进值
            float u = float(x) / float(sqrtSampleCount);
            float v = float(y) / float(sqrtSampleCount);

            // 将步进值转换为半球方向（余弦加权分布）
            vec2 Xi = vec2(u, v);
            float phi = 2.0 * PI * Xi.x; // 方位角
            float cosTheta = sqrt(1.0 - Xi.y); // 余弦加权
            float sinTheta = sqrt(Xi.y);

            // 半球方向
            vec3 sampleDir = vec3(
                cos(phi) * sinTheta,
                sin(phi) * sinTheta,
                cosTheta
            );

            // 将采样方向转换到世界空间
            vec3 worldDir = tangent * sampleDir.x + bitangent * sampleDir.y + normal * sampleDir.z;

            // 计算辐照度，使用余弦加权
            float weight = max(dot(normal, worldDir), 0.0);
            irradiance += texture(environmentMap, worldDir).rgb * weight;
        }
    }

    // 归一化并乘以 PI
    irradiance = irradiance * (1.0 / float(SAMPLE_COUNT)) * PI;

    fragColor = vec4(irradiance, 1.0);
}

// #version 450

// layout(location = 0) in vec3 texCoords;

// layout(set = 0, binding = 0) uniform samplerCube skybox;

// layout(location = 0) out vec4 outColor;

// void main() {
//     outColor = texture(skybox, texCoords);
// }