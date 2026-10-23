#version 450
layout(push_constant) uniform Push { layout(offset = 16) vec4 tint; } push_data;
layout(location = 0) in vec3 color;
layout(location = 0) out vec4 output_color;
void main() { output_color = vec4(color + push_data.tint.rgb, 1); }
