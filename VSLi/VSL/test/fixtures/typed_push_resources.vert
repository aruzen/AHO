#version 450
layout(push_constant) uniform Push { layout(offset = 16) vec4 tint; } push_data;
layout(set = 1, binding = 4, std140) uniform Bias { vec4 tint; } bias;
layout(location = 0) out vec3 color;
void main() {
    vec2 points[3] = vec2[](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
    gl_Position = vec4(points[gl_VertexIndex], 0, 1);
    color = push_data.tint.rgb + bias.tint.rgb;
}
