#version 450
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
layout(set = 0, binding = 0, std430) buffer Output { vec4 value; }
result;
layout(push_constant) uniform Push { vec4 bias; }
pushed;
void main() {
    result.value = pushed.bias;
    gl_Position = gl_in[0].gl_Position + pushed.bias;
    EmitVertex();
    EndPrimitive();
}
