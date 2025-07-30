#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    const vec2 positions[3] = vec2[](
        vec2( 0.0, -0.5),
        vec2( 0.5,  0.5),
        vec2(-0.5,  0.5)
    );

    // 段階的に原因を特定していく：
    // gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);                       // ✅ 必ず表示される
    // gl_Position = ubo.model * vec4(positions[gl_VertexIndex], 0.0, 1.0);          // ← model 行列の確認
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0); // ← view 行列の確認
    // gl_Position = ubo.proj * ubo.view * ubo.model * vec4(positions[gl_VertexIndex], 0.0, 1.0); // ← 最終段階
    fragColor = inColor;
}