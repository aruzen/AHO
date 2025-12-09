#version 450

layout(push_constant) uniform Push {
    vec3 p0;
    vec3 p1;
    vec3 p2;
    vec3 color;
} pushData;

layout(location = 0) out vec3 fragColor;

void main() {
    vec3 pos = (gl_VertexIndex == 0 ? pushData.p0 :
                gl_VertexIndex == 1 ? pushData.p1 :
                                      pushData.p2);
    gl_Position = vec4(pos, 1.0);
    fragColor = pushData.color;
}