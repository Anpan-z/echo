#version 450

layout(binding = 1) uniform sampler2D shadowMap;
//layout(binding = 1) uniform sampler2D texSampler;
layout(location = 0) in vec3 fragColor;
//layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in flat vec3 viewPos;
layout(location = 5) in vec4 fragPosLightSpace; // 传入的光源空间坐标

layout(location = 0) out vec4 outColor;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // 做透视除法
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; // [-1,1] -> [0,1]

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0; // 不加阴影

    // 从 shadow map 中采样深度
    float closestDepth = texture(shadowMap, projCoords.xy).r;

    float currentDepth = fragPosLightSpace.z;

    // 加上 bias 防止阴影失真（shadow acne）
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    //float bias = 0.005; // 0.005 是一个可调参数
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    // 如果超出 shadow map 范围就不投影阴影（可选）

    return shadow;
}

void main() {
    //outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);

    //vec3 lightPos = vec3(-0.1, 0.2, 0.2); // 上方光源
    vec3 lightPos = vec3(0, 1.5, 0); // 上方光源
    vec3 lightColor = vec3(1.0);         // 白光

    float ambientStrength = 0.1;         // 环境光强度
    float specStrength = 0.5;           // 镜面反射强度
    float shininess = 32;             // 镜面反射光泽度
    //vec3 fragNormal = vec3(0, 1, 0);

    // 输入向量
    vec3 normal = normalize(fragNormal);
    //vec3 lightDir = normalize(lightPos - fragPos);
    vec3 lightDir = normalize(vec3(1, 1, 1)); 
    vec3 viewDir = normalize(viewPos - fragPos);

    // 环境光
    vec3 ambient = ambientStrength * lightColor;

    // 漫反射（Lambert）
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // 镜面反射（Blinn-Phong）
    vec3 halfVec = normalize(lightDir + viewDir);
    float spec = pow(max(dot(fragNormal, halfVec), 0.0), shininess); // shininess 是一个可调参数
    vec3 specular = specStrength * spec * lightColor;

    float shadow = ShadowCalculation(fragPosLightSpace, normal, lightDir);
    vec3 color = (ambient +  diffuse + specular) * fragColor;

    vec3 lighting = (ambient +  (1.0-shadow)*diffuse + specular) * fragColor;

    outColor = vec4(lighting, 1.0);
    outColor.rgb = pow(outColor.rgb, vec3(1.0/2.2));
    //outColor = vec4(vec3(shadow), 1.0);
    //outColor = vec4(fragNormal * 0.5 + 0.5, 1.0);
}