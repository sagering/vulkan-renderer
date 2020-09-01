#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inNormal;
layout(location = 0) out vec4 outColor;

const vec3 lightDir = vec3(0, 0, 1);
const vec3 color = vec3(1, 1, 0);

void main() {
	//float d = dot(inNormal, lightDir);
	//float colorFactor = clamp(d, 0, 1) + 0.3 + pow(d, 3);
	//outColor = vec4(color * colorFactor / 2, 1);
	
	outColor = vec4(color, 1);
}