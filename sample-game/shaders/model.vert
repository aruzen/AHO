#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texCoord;
layout(location=3) in vec4 color;
layout(location=4) in vec4 jointIndices;
layout(location=5) in vec4 weights;
layout(set=0,binding=1,std430) readonly buffer JointPalette { mat4 joints[]; };
layout(push_constant) uniform Transform { mat4 world; mat4 viewProjection; } transform;
layout(location=0) out vec3 worldNormal;
layout(location=1) out vec2 uv;
layout(location=2) out vec4 tint;
layout(location=3) out vec3 worldPosition;
layout(location=4) flat out vec4 eyeNight;
void main() {
    mat4 skin = mat4(1.0);
    if (dot(weights, vec4(1.0)) > 0.0) {
        ivec4 indices = ivec4(jointIndices);
        skin = joints[indices.x] * weights.x + joints[indices.y] * weights.y
             + joints[indices.z] * weights.z + joints[indices.w] * weights.w;
    }
    mat4 world = transform.world * skin;
    vec4 p = world * vec4(position, 1.0);
    gl_Position = transform.viewProjection * p;
    worldPosition = p.xyz;
    worldNormal = transpose(inverse(mat3(world))) * normal;
    uv = texCoord;
    tint = color;
    eyeNight = joints[joints.length() - 1][0];
}
