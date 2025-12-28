#version 450

layout(push_constant) uniform Push {
    ivec2 window;
    ivec2 p0;
    ivec2 p1;
    ivec2 p2;
    vec3 color;
} pushData;

layout(location = 0) out vec3 fragColor;

void main() {
    vec2 pos = (gl_VertexIndex == 0 ? vec2(pushData.p0 * 2):
                gl_VertexIndex == 1 ? vec2(pushData.p1 * 2):
                                      vec2(pushData.p2 * 2));
    pos.x /= pushData.window.x;
    pos.y /= pushData.window.y;
    gl_Position = vec4(pos - vec2(1.0, 1.0), 0.0, 1.0);
    fragColor = pushData.color;
}