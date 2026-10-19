#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
layout(location = 0) in f16vec2 half_value;
layout(location = 1) in i16vec3 signed_short_value;
layout(location = 2) in u16vec4 unsigned_short_value;
layout(location = 3) in dvec3 double_value;
layout(location = 5) in dmat3 basis;
layout(location = 11) in i64vec2 signed_long_value;
layout(location = 12) in u64vec2 unsigned_long_value;
layout(location = 13) in dmat2 compact_basis;
layout(location = 15) in dvec4 double_four;
void main() {
    vec3 point = vec3(basis * double_value) + vec3(signed_short_value) + vec3(vec2(half_value), 0);
    point += vec3(unsigned_short_value.xyz) + vec3(vec2(signed_long_value), 0) + vec3(vec2(unsigned_long_value), 0);
    point += vec3(vec2(compact_basis[0]), 0) + vec3(double_four.xyz);
    gl_Position = vec4(point, 1);
}
