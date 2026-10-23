#version 450
layout(location = 0, component = 0) in vec2 position;
layout(location = 0, component = 2) in float red;
layout(location = 0, component = 3) in float green;
layout(location = 7) in float unused_value;
layout(location = 0) out vec3 color;
void main() {
    gl_Position = vec4(position, 0, 1);
    color = vec3(red, green, .75);
}
