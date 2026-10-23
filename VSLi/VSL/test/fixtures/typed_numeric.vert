#version 450
layout(location = 0) in ivec2 signed_value;
layout(location = 1) in uvec3 unsigned_value;
void main() {
    gl_Position = vec4(vec2(signed_value), float(unsigned_value.x), 1);
}
