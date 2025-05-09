#version 450

layout(set = 0, binding = 0) uniform sampler2D equirectangularMap; // 输入的 equirectangular HDR 环境贴图

layout(location = 0) in vec3 localPos; // 从顶点着色器接收的方向向量
layout(location = 0) out vec4 fragColor;

const vec2 invAtan = vec2(0.1591, 0.3183); // 预计算的 1 / (2 * PI) 和 1 / PI，用于归一化 UV 坐标

vec2 sampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec3 dir = normalize(localPos);
    vec2 uv = sampleSphericalMap(dir);
    vec3 color = texture(equirectangularMap, uv).rgb;

    float exposure = 0.5; // 曝光因子，适当调整
    color = vec3(1.0) - exp(-color * exposure); // 简单的色调映射
    
    fragColor = vec4(color, 1.0);
    // fragColor = vec4(1.0, 0.0, 1.0, 1.0); // Magenta
}

// void main() {
//     // 将立方体方向向量转换为球面坐标
//     vec3 dir = normalize(localPos);
//     float phi = atan(dir.z, dir.x); // 方位角
//     float theta = asin(dir.y);     // 极角

//     // 将球面坐标转换为 equirectangular 纹理坐标
//     vec2 uv = vec2(
//         (phi / (2.0 * PI)) + 0.5,  // U 坐标
//         (theta / PI) + 0.5         // V 坐标
//     );

//     // 从 equirectangular 环境贴图中采样颜色
//     vec3 color = texture(equirectangularMap, uv).rgb;

//     fragColor = vec4(color, 1.0); // 输出颜色
// }

