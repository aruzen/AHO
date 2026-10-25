#version 450
layout(push_constant) uniform Push { layout(offset = 8) vec2 extra; } push_data;
layout(location = 0) in vec3 color;
layout(location = 0) out vec4 output_color;
void main() { output_color = vec4(color + vec3(0, 0, push_data.extra.x * 2), push_data.extra.y); }
