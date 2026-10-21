#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texCoord;
layout(location=3) in vec4 color;
layout(push_constant) uniform Transform { mat4 world; mat4 viewProjection; } transform;
layout(location=0) out vec3 worldNormal;
layout(location=1) out vec2 uv;
layout(location=2) out vec4 tint;
void main() {
    gl_Position = transform.viewProjection * transform.world * vec4(position, 1.0);
    worldNormal = transpose(inverse(mat3(transform.world))) * normal;
    uv = texCoord;
    tint = color;
}
