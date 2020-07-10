#version 410

layout (location = 0) in vec2 v_position;
layout (location = 1) in vec2 v_uv;

out vec2 f_uv;

uniform mat4 proj;
uniform mat4 model;

void main() {
    f_uv = v_uv;
    gl_Position = proj * model * vec4(v_position, 0.0, 1.0);
}
