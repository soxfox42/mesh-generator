#version 330 core

in vec3 normal;
in vec3 position;

out vec4 outColor;

void main()
{
    vec3 color = vec3(0.4, 0.6, 0.8);

    float ambientStrength = 0.3;
    vec3 ambient = color * ambientStrength;

    float diffuseValue = max(dot(-normalize(position), normalize(normal)), 0.0);
    vec3 diffuse = color * diffuseValue;

    outColor = vec4(ambient + diffuse, 1.0);
}