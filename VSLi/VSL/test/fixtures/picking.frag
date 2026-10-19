#version 450
layout(location = 0) in vec3 color;
layout(location = 0) out vec4 result;
layout(location = 1) out uint objectId;
void main() {
    result = vec4(color, 1);
    objectId = 42u;
}
