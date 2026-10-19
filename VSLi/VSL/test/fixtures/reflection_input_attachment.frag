#version 450
layout(input_attachment_index = 0, set = 0, binding = 4) uniform subpassInput source_color;
layout(location = 0) out vec4 result;
void main() { result = subpassLoad(source_color); }
