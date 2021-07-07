#version 330 core

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec4 inColor;

uniform mat4 projection;

out vec2 uv;
out vec4 color;

void main()
{
    uv = inUV;
    color = inColor;
    gl_Position = projection * vec4(inPos, 0.0, 1.0);
}