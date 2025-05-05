#version 450

layout(location = 0) out vec3 localPos; // 输出到片段着色器的方向向量

vec3 positions[36] = vec3[](
    // +X face (Right)
    vec3(1.0, -1.0, -1.0), vec3(1.0,  1.0, -1.0), vec3(1.0,  1.0,  1.0),
    vec3(1.0, -1.0, -1.0), vec3(1.0,  1.0,  1.0), vec3(1.0, -1.0,  1.0),

    // -X face (Left)
    vec3(-1.0, -1.0,  1.0), vec3(-1.0,  1.0,  1.0), vec3(-1.0,  1.0, -1.0),
    vec3(-1.0, -1.0,  1.0), vec3(-1.0,  1.0, -1.0), vec3(-1.0, -1.0, -1.0),

    // +Y face (Top)
    vec3(-1.0, 1.0, -1.0), vec3(-1.0, 1.0,  1.0), vec3( 1.0, 1.0,  1.0),
    vec3(-1.0, 1.0, -1.0), vec3( 1.0, 1.0,  1.0), vec3( 1.0, 1.0, -1.0),

    // -Y face (Bottom)
    vec3(-1.0, -1.0,  1.0), vec3(-1.0, -1.0, -1.0), vec3( 1.0, -1.0, -1.0),
    vec3(-1.0, -1.0,  1.0), vec3( 1.0, -1.0, -1.0), vec3( 1.0, -1.0,  1.0),

    // +Z face (Front)
    vec3(-1.0, -1.0, 1.0), vec3(-1.0,  1.0, 1.0), vec3( 1.0,  1.0, 1.0),
    vec3(-1.0, -1.0, 1.0), vec3( 1.0,  1.0, 1.0), vec3( 1.0, -1.0, 1.0),

    // -Z face (Back)
    vec3( 1.0, -1.0, -1.0), vec3( 1.0,  1.0, -1.0), vec3(-1.0,  1.0, -1.0),
    vec3( 1.0, -1.0, -1.0), vec3(-1.0,  1.0, -1.0), vec3(-1.0, -1.0, -1.0)
);


vec2 positions_screen[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2( 1.0, -1.0)
);

void main() {
    localPos = positions[gl_VertexIndex]; // 输出方向向量
    vec3 localPos_screen = vec3(positions_screen[gl_VertexIndex % 6], 0.0); //  固定的裁剪方向向量
    gl_Position = vec4(localPos_screen, 1.0);    // 将方向向量作为裁剪空间坐标
}
