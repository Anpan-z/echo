#version 450

layout(location = 0) in vec3 fragColor;
layout (location = 2) in vec3 fragPositionWorld;
layout (location = 3) in vec3 fragNormalWorld;
// layout (location = 2) in vec2 fragUV;
layout(location = 4) in flat vec3 viewPos;
layout(location = 5) in vec4 fragPosLightSpace; // 传入的光源空间坐标
layout(location = 6) in flat uint fragShapeID; // 实例 ID

layout (location = 0) out vec4 outColor;

struct Material {
    vec3 albedo;
    float metallic;
    float roughness;
    float ambientOcclusion;
    float padding1; // pad to 16 bytes
    float emission;
};

layout(binding = 2) uniform MaterialBlock {
    Material materials[8]; // Max 128 shapes
}Material_ubo;

layout(set = 0, binding = 4) uniform samplerCube irradianceMap; // 辐照度贴图
layout(set = 0, binding = 5) uniform samplerCube prefilteredMap; // 预过滤环境贴图
layout(set = 0, binding = 6) uniform sampler2D brdfLUT; // BRDF LUT

// // === Material Parameters ===
// uniform vec3 albedo = vec3(1.0, 0.0, 0.0);   // Base color
// uniform float metallic = 0.0;
// uniform float roughness = 0.5;
// uniform float ambientOcclusion = 1.0;

// // === Camera & Light ===
// uniform vec3 cameraPosition = viewPos;
// uniform vec3 lightPosition = fragPosLightSpace;
// uniform vec3 lightColor = vec3(1.0);

// === Constants ===
const float PI = 3.14159265359;

// === Helper Functions ===
float DistributionGGX(vec3 normal, vec3 halfVector, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(normal, halfVector), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 normal, vec3 viewDir, vec3 lightDir, float roughness) {
    float NdotV = max(dot(normal, viewDir), 0.0);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
} 

// === Main Shader ===
void main() {
    // === Material Parameters ===
    // vec3 albedo = vec3(1.0, 0.0, 0.0);   // Base color
    Material mat = Material_ubo.materials[fragShapeID];
    // Material mat = materials[fragShapeID];
    vec3 albedo = fragColor;   // Base color
    float metallic = mat.metallic;
    float roughness = mat.roughness;
    float ambientOcclusion = mat.ambientOcclusion;

    // === Camera & Light ===
    vec3 cameraPosition = viewPos;
    vec3 lightPosition = vec3(0.0, 1.0, 1); // 透视除法
    vec3 lightColor = vec3(1.0);
    
    // 基础向量
    vec3 flipNormal = dot(fragNormalWorld, normalize(viewPos - fragPositionWorld)) > 0.0 ? fragNormalWorld : -fragNormalWorld; // 反转法线 
    vec3 normal = normalize(flipNormal);
    vec3 viewDirection = normalize(cameraPosition - fragPositionWorld);
    vec3 lightDirection = normalize(lightPosition - fragPositionWorld);
    vec3 halfVector = normalize(viewDirection + lightDirection);

    // 表面反射率 F0
    vec3 baseReflectance = vec3(0.04); // 常用于非金属
    vec3 F0 = mix(baseReflectance, albedo, metallic);

    // 计算各项参数
    float NDF = DistributionGGX(normal, halfVector, roughness);
    float G   = GeometrySmith(normal, viewDirection, lightDirection, roughness);
    vec3  F   = FresnelSchlick(max(dot(halfVector, viewDirection), 0.0), F0);

    // Cook-Torrance BRDF
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDirection), 0.0) * max(dot(normal, lightDirection), 0.0) + 0.001;
    vec3 specular     = numerator / denominator;

    // 漫反射分量
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; // 金属无漫反射

    float NdotL = max(dot(normal, lightDirection), 0.0);
    vec3 irradiance = lightColor * NdotL;

    vec3 finalColor = (kD * albedo / PI + specular) * irradiance;

    // === 环境光照 IBL ===
    F   = fresnelSchlickRoughness(max(dot(normal, viewDirection), 0.0), F0, roughness);
    kS = F;
    kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; // 金属无漫反射

    vec3 R = reflect(-viewDirection, normal);
    R = normalize(R);

    // 环境漫反射
    vec3 irradianceEnv = texture(irradianceMap, normal).rgb;
    vec3 diffuseEnv = irradianceEnv * albedo;

    // 环境镜面反射（预过滤环境贴图）
    const float MAX_REFLECTION_LOD = 5.0;
    vec3 prefilteredColor = textureLod(prefilteredMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(brdfLUT, vec2(max(dot(normal, viewDirection), 0.0), roughness)).rg;
    vec3 specularEnv = prefilteredColor * (F * brdf.x + brdf.y);

    // 合并环境光分量
    vec3 ambient = (kD * diffuseEnv + specularEnv) * ambientOcclusion;

    // 环境光（AO）
    finalColor = finalColor * ambientOcclusion;
    finalColor += ambient;
    // Gamma 矫正
    // finalColor = pow(finalColor, vec3(1.0/2.2));

    outColor = vec4(finalColor, 1.0);
}
