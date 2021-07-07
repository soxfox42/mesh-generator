#version 330 core

uniform sampler2D tex;

in vec2 uv;
in vec4 color;

out vec4 outColor;

void main()
{
    outColor = color * texture(tex, uv);
}