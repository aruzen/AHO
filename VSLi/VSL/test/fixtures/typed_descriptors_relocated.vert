#version 450
layout(set = 3, binding = 3, std140) uniform Tint { vec4 rgba; } tints[2];
layout(set = 1, binding = 5, std430) readonly buffer Extra { vec4 rgba; } extra;
layout(location = 0) out vec3 color;
void main() {
    vec2 points[3] = vec2[](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
    gl_Position = vec4(points[gl_VertexIndex], 0, 1);
    color = (tints[0].rgba.rgb + tints[1].rgba.rgb) * .5 + extra.rgba.rgb;
}
