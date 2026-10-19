#version 450
layout(vertices = 3) out;
layout(set = 0, binding = 0, std430) buffer Output { vec4 value; }
result;
layout(push_constant) uniform Push { vec4 bias; }
pushed;
void main() {
    result.value = pushed.bias;
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position + pushed.bias;
    gl_TessLevelInner[0] = 1;
    gl_TessLevelOuter[0] = 1;
    gl_TessLevelOuter[1] = 1;
    gl_TessLevelOuter[2] = 1;
}
