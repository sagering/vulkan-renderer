#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
//layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 outNormal;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
    gl_Position = vec4(inPos, 1);
	//outNormal = inNormal;
}