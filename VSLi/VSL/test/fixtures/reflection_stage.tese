#version 450
layout(triangles, equal_spacing, cw) in;
layout(set = 0, binding = 0, std430) buffer Output { vec4 value; }
result;
layout(push_constant) uniform Push { vec4 bias; }
pushed;
void main() {
    result.value = pushed.bias;
    gl_Position = gl_in[0].gl_Position + pushed.bias;
}
