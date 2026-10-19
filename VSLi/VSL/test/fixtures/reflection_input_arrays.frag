#version 450
layout(input_attachment_index = 2, set = 1, binding = 7) uniform subpassInput source_colors[2];
layout(location = 0) out vec4 result;
void main() { result = subpassLoad(source_colors[0]) + subpassLoad(source_colors[1]); }
