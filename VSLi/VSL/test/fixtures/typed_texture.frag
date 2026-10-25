#version 450
layout(set = 1, binding = 2) uniform sampler2D images[2];
layout(location = 0) out vec4 output_color;
void main() {
    output_color = (texture(images[0], vec2(.5)) + texture(images[1], vec2(.5))) * .5;
}
