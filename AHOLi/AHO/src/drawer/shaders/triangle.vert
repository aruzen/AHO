#version 450
layout(push_constant) uniform Triangle {
    vec4 position1;
    vec4 position2;
    vec4 position3;
    vec4 color;
} triangle;
layout(location = 0) out vec4 color;
void main() {
    vec4 positions[3] = vec4[3](triangle.position1, triangle.position2, triangle.position3);
    gl_Position = positions[gl_VertexIndex];
    color = triangle.color;
}
