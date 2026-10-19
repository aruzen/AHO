#version 450

layout(push_constant) uniform PushConstants {
    mat4 modelMatrix; // vertex shaderで使う
    vec4 color;       // fragment shaderで使う
}
pc;

// ----------------------
// 1. スカラー入力
// ----------------------
layout(location = 0) in float inScalarF;
layout(location = 1) in int inScalarI;
layout(location = 2) in uint inScalarU;
// layout(location = 3) in bool  inScalarB;

// ----------------------
// 2. ベクトル入力
// ----------------------
layout(location = 4) in vec2 inVec2;
layout(location = 5) in vec3 inVec3;
layout(location = 6) in vec4 inVec4;

layout(location = 7) in ivec2 inIVec2;
layout(location = 8) in ivec3 inIVec3;
layout(location = 9) in ivec4 inIVec4;

layout(location = 10) in uvec2 inUVec2;
layout(location = 11) in uvec3 inUVec3;
layout(location = 12) in uvec4 inUVec4;

// ----------------------
// 3. 行列入力
// ----------------------
// 行列は列ごとに location を1つ消費します。
// mat2 → location 13,14
// mat3 → location 15,16,17
// mat4 → location 18,19,20,21
layout(location = 13) in mat2 inMat2;
layout(location = 15) in mat3 inMat3;
layout(location = 18) in mat4 inMat4;

// 非正方行列も例として：
layout(location = 22) in mat2x3 inMat2x3; // consumes 2 locations (22,23)
layout(location = 24) in mat3x2 inMat3x2; // consumes 3 locations (24,25,26)
layout(location = 27) in mat4x3 inMat4x3; // consumes 4 locations (27,28,29,30)
layout(location = 31) in mat3x4 inMat3x4; // consumes 3 locations (31,32,33,34)

// --- 4. Sampler types (combined image samplers) ---
layout(binding = 0) uniform sampler1D u_sampler1D;
layout(binding = 1) uniform sampler2D u_sampler2D;
layout(binding = 2) uniform sampler3D u_sampler3D;
layout(binding = 3) uniform samplerCube u_samplerCube;
layout(binding = 4) uniform sampler1DArray u_sampler1DArray;
layout(binding = 5) uniform sampler2DArray u_sampler2DArray;
layout(binding = 6) uniform samplerCubeArray u_samplerCubeArray;
layout(binding = 7) uniform sampler2DRect u_sampler2DRect;
layout(binding = 8) uniform sampler2DMS u_sampler2DMS;
layout(binding = 9) uniform sampler2DMSArray u_sampler2DMSArray;

// Shadow samplers
layout(binding = 10) uniform sampler2DShadow u_sampler2DShadow;
layout(binding = 11) uniform samplerCubeShadow u_samplerCubeShadow;

// --- 5. Image types (read/write images) ---
layout(binding = 12, rgba8) uniform image1D u_image1D;
layout(binding = 13, rgba8) uniform image2D u_image2D;
layout(binding = 14, rgba8) uniform image3D u_image3D;
layout(binding = 15, rgba8) uniform imageCube u_imageCube;
layout(binding = 16, rgba8) uniform image1DArray u_image1DArray;
layout(binding = 17, rgba8) uniform image2DArray u_image2DArray;
layout(binding = 18, rgba8) uniform imageCubeArray u_imageCubeArray;
layout(binding = 19, rgba8) uniform image2DMS u_image2DMS;
layout(binding = 20, rgba8) uniform image2DMSArray u_image2DMSArray;
layout(binding = 21, rgba8) uniform imageBuffer u_imageBuffer;

// --- 6. Subpass input ---
// layout(input_attachment_index = 0, set = 0, binding = 22) uniform subpassInput u_subpassInput0;
// layout(input_attachment_index = 1, set = 0, binding = 23) uniform subpassInput u_subpassInput1;
// layout(input_attachment_index = 2, set = 0, binding = 24) uniform subpassInput u_subpassInput2;

// --- fragment output ---
layout(location = 0) out vec4 fragColor;

void main() {
    // dummy access
    vec4 col = vec4(0.0);
    col += texture(u_sampler1D, 0.5);
    col += texture(u_sampler2D, vec2(0.5));
    col += texture(u_sampler3D, vec3(0.5));
    col += texture(u_samplerCube, vec3(0.5));
    col += imageLoad(u_image1D, 0);
    col += imageLoad(u_image2D, ivec2(0));
    col += imageLoad(u_image3D, ivec3(0));
    //     col += subpassLoad(u_subpassInput0);
    fragColor = col;
}
