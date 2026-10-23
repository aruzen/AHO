#version 450
#extension GL_EXT_nonuniform_qualifier : require
layout(location = 0) in vec2 position;
layout(set = 0, binding = 0) uniform sampler2D images[];
void main() {
    gl_Position = vec4(position + texture(images[nonuniformEXT(gl_VertexIndex)], vec2(0)).xy, 0, 1);
}
