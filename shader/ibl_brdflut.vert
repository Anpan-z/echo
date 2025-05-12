#version 450

vec2 positions_screen[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2( 1.0, -1.0)
);

void main() {
    vec3 localPos_screen = vec3(positions_screen[gl_VertexIndex], 0.0); //  固定的裁剪方向向量
    gl_Position = vec4(localPos_screen, 1.0);    // 将方向向量作为裁剪空间坐标
}