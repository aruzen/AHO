#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 color;
layout(location=3) in float material;
layout(push_constant) uniform Scene {
    mat4 viewProjection;
    vec4 eyeTime;
    vec4 settings; // night, aspect, HUD visibility, reserved
    vec4 forward;
    vec4 right;
} scene;
layout(location=0) out vec3 world;
layout(location=1) out vec3 faceNormal;
layout(location=2) out vec3 albedo;
layout(location=3) flat out int kind;
void main() {
    kind = int(material + .5);
    vec3 p = position;
    if (kind == 2)
        p = scene.eyeTime.xyz + p * 900.;
    if (kind == 3) {
        p.x += sin(scene.eyeTime.w * .25 + position.z) * 1.4;
        p.y = mod(position.y - scene.eyeTime.w * .65, 45.);
    }
    world = p;
    faceNormal = normal;
    albedo = color;
    gl_Position = scene.viewProjection * vec4(p, 1);
    if (kind == 2)
        gl_Position.z = gl_Position.w * .99999;
}
