#ifndef MESH_H
#define MESH_H

#include <cglm/cglm.h>
#include <glad/glad.h>

typedef struct Mesh Mesh;

/// Creates a new mesh, with its own vertex buffer, using shaderProgram for
/// drawing.
Mesh *createMesh(GLuint shaderProgram);
/// Clears the mesh's vertex list.
void clearMesh(Mesh *mesh);
/// Adds a quad with correct normals, to a mesh.
void addQuad(Mesh *mesh, vec3 a, vec3 b, vec3 c, vec3 d, bool invertNormals);
/// Copies the internal vertex buffer of a mesh to the GPU.
void updateMeshBuffer(Mesh *mesh);
/// Renders a mesh.
void renderMesh(Mesh *mesh, mat4 view, mat4 projection);
/// Exports a mesh, writing to the given file handle.
void exportMesh(Mesh *mesh, FILE *file);
/// Destroys a mesh, freeing all buffers and GPU objects.
void destroyMesh(Mesh *mesh);

#endif
