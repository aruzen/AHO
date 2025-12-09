#version 450

layout(push_constant) uniform Push {
    vec2 p0;
    vec2 p1;
    vec2 p2;
    vec3 color;
} pushData;

layout(location = 0) out vec3 fragColor;

void main() {
    vec2 pos = (gl_VertexIndex == 0 ? pushData.p0 :
                gl_VertexIndex == 1 ? pushData.p1 :
                                      pushData.p2);
    gl_Position = vec4(pos, 0.0, 1.0);
    fragColor = pushData.color;
}