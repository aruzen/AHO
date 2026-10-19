#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;
layout(location = 0) out vec3 vertex_color;
layout(set = 0, binding = 0) uniform Transform { mat4 model; }
transform_data;
layout(push_constant) uniform Push { vec4 tint; }
push_data;
void main() {
    gl_Position = transform_data.model * vec4(position, 0, 1);
    vertex_color = color * push_data.tint.rgb;
}
