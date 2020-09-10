#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) out vec4 outColor;

layout (constant_id = 0) const uint colorId = 0;

void main() {
	if(colorId == 0) {
		outColor = vec4(1, 0, 0, 1);
	} else {
		outColor = vec4(0, 1, 0, 1);
	}
}