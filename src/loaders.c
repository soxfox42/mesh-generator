#include "loaders.h"

#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>

GLuint loadShader(char *path, GLenum type) {
    // rb is required for ftell to return file size.
    FILE *file = fopen(path, "rb");
    if (!file) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    int length = ftell(file);
    fseek(file, 0, SEEK_SET);

    GLchar *buffer = malloc(length);
    if (!buffer) {
        return 0;
    }
    fread(buffer, 1, length, file);

    fclose(file);

    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, (const GLchar **)&buffer, &length);
    free(buffer);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        printf("%s\n", infoLog);
    }

    return shader;
}

GLuint loadShaderProgram(char *vertexPath, char *fragmentPath) {
    GLuint vertexShader = loadShader(vertexPath, GL_VERTEX_SHADER);
    GLuint fragmentShader = loadShader(fragmentPath, GL_FRAGMENT_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        fprintf(stderr, "%s\n", infoLog);
        program = 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}
