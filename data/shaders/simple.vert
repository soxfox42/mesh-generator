#version 330 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 normal;
out vec3 position;

void main()
{
    normal = mat3(view * model) * inNormal;
    position = vec3(view * model * vec4(inPos, 1.0));
    gl_Position = projection * view * model * vec4(inPos, 1.0);
}