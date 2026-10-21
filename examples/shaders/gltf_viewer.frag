#version 450
layout(location=0) in vec3 worldNormal;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 tint;
layout(set=0,binding=0) uniform sampler2D baseColor;
layout(location=0) out vec4 outputColor;
void main() {
    float light = 0.3 + 0.7 * max(dot(normalize(worldNormal), normalize(vec3(1.5,2.0,3.0))), 0.0);
    vec4 surface = texture(baseColor, uv) * tint;
    outputColor = vec4(surface.rgb * light, surface.a);
}
