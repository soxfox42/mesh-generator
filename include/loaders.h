#ifndef LOADERS_H
#define LOADERS_H

#include <glad/glad.h>

/// Loads and compiles a single shader of a specified type.
GLuint loadShader(char *path, GLenum type);
/// Loads a vertex and a fragment shader, and links them into a program.
GLuint loadShaderProgram(char *vertexPath, char *fragmentPath);

#endif
