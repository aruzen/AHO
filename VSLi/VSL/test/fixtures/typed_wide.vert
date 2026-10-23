#version 450
#extension GL_ARB_gpu_shader_fp64 : require
layout(location = 2) in dvec3 vector;
layout(location = 3, component = 2) in double tail;
void main() {
    gl_Position = vec4(vector, tail);
}
