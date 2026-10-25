#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;
layout(set = 0, binding = 0, std140) uniform Scene { vec4 tint; } scene;
layout(push_constant) uniform Draw { vec4 transform; } draw;
layout(location = 0) out vec3 vertex_color;
void main() {
    float c = cos(draw.transform.w), s = sin(draw.transform.w);
    vec2 point = mat2(c, s, -s, c) * position * draw.transform.z;
    gl_Position = vec4(point + draw.transform.xy, 0, 1);
    vertex_color = color * scene.tint.rgb;
}
