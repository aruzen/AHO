#version 450
layout(location = 4) in mat3 basis;
layout(location = 1) in vec2 position;
void main() {
    gl_Position = vec4(basis * vec3(position, 1), 1);
}
