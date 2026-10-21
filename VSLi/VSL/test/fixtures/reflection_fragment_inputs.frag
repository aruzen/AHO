#version 450
layout(location = 2) in vec3 input_color;
layout(location = 0) out vec4 output_color;
layout(set = 0, binding = 1) uniform Parameters { vec4 scale; } parameters;
layout(push_constant) uniform Push { vec4 bias; } pushed;
void main() {
    output_color = vec4(input_color, 1) * parameters.scale + pushed.bias;
}
