#version 450

layout(push_constant) uniform Push {
    vec2 p0;
    vec2 p1;
    vec2 p2;
    vec2 p3;
    vec2 uv0;
    vec2 uv1;
    vec2 uv2;
    vec2 uv3;
} pushData;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    vec2 pos;
    vec2 uv;
    if (gl_VertexIndex == 0) {
        pos = pushData.p0;
        uv = pushData.uv0;
    } else if (gl_VertexIndex == 1) {
        pos = pushData.p1;
        uv = pushData.uv1;
    } else if (gl_VertexIndex == 2) {
        pos = pushData.p2;
        uv = pushData.uv2;
    } else {
        pos = pushData.p3;
        uv = pushData.uv3;
    }

    gl_Position = vec4(pos, 0.0, 1.0);
    fragTexCoord = uv;
}
