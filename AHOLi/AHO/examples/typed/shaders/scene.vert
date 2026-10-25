#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;
layout(set = 0, binding = 0, std140) uniform Scene { vec4 tint; vec4 camera; } scene;
layout(push_constant) uniform Draw { vec4 transform; } draw;
layout(location = 0) out vec3 vertex_color;
void main() {
    float angle = draw.transform.w * scene.camera.w;
    float c = cos(angle), s = sin(angle);
    vec2 point = mat2(c, s, -s, c) * position * draw.transform.z;
    vec2 aspect = vec2(min(1.0, 1.0 / scene.camera.z), min(1.0, scene.camera.z));
    gl_Position = vec4((point + draw.transform.xy - scene.camera.xy) * aspect, 0, 1);
    vertex_color = color * scene.tint.rgb;
}
