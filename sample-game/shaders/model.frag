#version 450
layout(location=0) in vec3 worldNormal;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 tint;
layout(location=3) in vec3 worldPosition;
layout(location=4) flat in vec4 eyeNight;
layout(set=0,binding=0) uniform sampler2D baseColor;
layout(location=0) out vec4 outputColor;
void main() {
    float night = eyeNight.w;
    float diffuse = max(dot(normalize(worldNormal), normalize(vec3(-.65, .75, -.65))), 0.);
    vec3 lighting = mix(vec3(.51, .68, .66) + diffuse * vec3(.72, .61, .4), vec3(.22, .39, .44) + diffuse * vec3(.22, .32, .29), night);
    vec3 c = texture(baseColor, uv).rgb * tint.rgb * lighting;
    float fog = 1. - exp(-length(worldPosition - eyeNight.xyz) * mix(.0022, .0035, night));
    c = mix(c, mix(vec3(.65, .78, .68), vec3(.08, .19, .23), night), clamp(fog, 0., .9));
    c = pow(c / (c + vec3(.65)), vec3(1.32));
    outputColor = vec4(c, 1.);
}
