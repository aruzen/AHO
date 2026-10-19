#version 450
#extension GL_ARB_gpu_shader_fp64 : require
layout(location = 2) in mat2x3 transforms[2];
layout(location = 7) in dmat2x3 wide_transforms[2];
layout(location = 20) in vec2 grid[2][2];
void main() {
    gl_Position = vec4(transforms[0][0] + transforms[1][1] + vec3(wide_transforms[1][0]) + vec3(grid[1][1], 0), 1);
}
