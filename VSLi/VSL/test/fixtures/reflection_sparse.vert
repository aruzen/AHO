#version 450
layout(location = 3) in vec2 position;
layout(location = 7) in vec3 input_color;
layout(location = 0) out vec3 color;
void main() {
    gl_Position = vec4(position, 0, 1);
    color = input_color;
}
