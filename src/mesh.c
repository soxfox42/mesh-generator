#include "mesh.h"

#include <string.h>
#include <cglm/cglm.h>
#include <glad/glad.h>

typedef struct {
    vec3 pos;
    vec3 normal;
} Vertex;

struct Mesh {
    GLuint VAO;
    GLuint VBO;
    GLuint shaderProgram;
    mat4 model;
    Vertex *vertices;
    size_t vertex_length;
    size_t vertex_capacity;
    size_t draw_length;
};

const size_t INITIAL_CAPACITY = 4096;

void setupVAO(GLuint VAO, GLuint VBO) {
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Set up the attributes based on the Vertex struct.
    // Position and normal are passed to the GPU.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void *)offsetof(Vertex, pos));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void *)offsetof(Vertex, normal));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
}

Mesh *createMesh(GLuint shaderProgram) {
    Mesh *mesh = malloc(sizeof(Mesh));
    glGenVertexArrays(1, &mesh->VAO);
    glGenBuffers(1, &mesh->VBO);
    setupVAO(mesh->VAO, mesh->VBO);

    mesh->shaderProgram = shaderProgram;
    glm_mat4_identity(mesh->model);

    mesh->vertices = malloc(INITIAL_CAPACITY * sizeof(Vertex));
    mesh->vertex_length = 0;
    mesh->vertex_capacity = INITIAL_CAPACITY;

    mesh->draw_length = 0;

    return mesh;
}

void clearMesh(Mesh *mesh) {
    mesh->vertex_length = 0;
    mesh->draw_length = 0;
}

/// Doubles the capacity of a mesh's internal vertex buffer.
void expandVertices(Mesh *mesh) {
    mesh->vertex_capacity *= 2;
    mesh->vertices =
        realloc(mesh->vertices, mesh->vertex_capacity * sizeof(Vertex));
}

void pushVertex(Mesh *mesh, vec3 pos, vec3 normal) {
    if (mesh->vertex_length >= mesh->vertex_capacity) {
        expandVertices(mesh);
    }
    glm_vec3_copy(pos, mesh->vertices[mesh->vertex_length].pos);
    glm_vec3_copy(normal, mesh->vertices[mesh->vertex_length].normal);
    mesh->vertex_length++;
}

void addQuad(Mesh *mesh, vec3 a, vec3 b, vec3 c, vec3 d, bool invertNormals) {
    // Calculate edge vectors
    vec3 ab, bc, cd, da;
    glm_vec3_sub(b, a, ab);
    glm_vec3_sub(c, b, bc);
    glm_vec3_sub(d, c, cd);
    glm_vec3_sub(a, d, da);

    vec3 normalA, normalB, normalC, normalD;

    glm_vec3_cross(da, ab, normalA);
    glm_vec3_cross(ab, bc, normalB);
    glm_vec3_cross(bc, cd, normalC);
    glm_vec3_cross(cd, da, normalD);

    if (invertNormals) {
        glm_vec3_negate(normalA);
        glm_vec3_negate(normalB);
        glm_vec3_negate(normalC);
        glm_vec3_negate(normalD);
    }

    // Push all vertices, with correct winding
    if (!invertNormals) {
        pushVertex(mesh, a, normalA);
        pushVertex(mesh, b, normalB);
        pushVertex(mesh, d, normalD);
        pushVertex(mesh, d, normalD);
        pushVertex(mesh, b, normalB);
        pushVertex(mesh, c, normalC);
    } else {
        pushVertex(mesh, a, normalA);
        pushVertex(mesh, d, normalD);
        pushVertex(mesh, b, normalB);
        pushVertex(mesh, b, normalB);
        pushVertex(mesh, d, normalD);
        pushVertex(mesh, c, normalC);
    }
}

void updateMeshBuffer(Mesh *mesh) {
    glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh->vertex_length * sizeof(Vertex),
                 mesh->vertices, GL_STATIC_DRAW);
    mesh->draw_length = mesh->vertex_length;
}

void renderMesh(Mesh *mesh, mat4 view, mat4 projection) {
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    glUseProgram(mesh->shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(mesh->shaderProgram, "model"), 1,
                       GL_FALSE, (float *)mesh->model);
    glUniformMatrix4fv(glGetUniformLocation(mesh->shaderProgram, "view"), 1,
                       GL_FALSE, (float *)view);
    glUniformMatrix4fv(glGetUniformLocation(mesh->shaderProgram, "projection"),
                       1, GL_FALSE, (float *)projection);
    glBindVertexArray(mesh->VAO);
    glDrawArrays(GL_TRIANGLES, 0, mesh->draw_length);
}

void outputVertex(vec3 pos, FILE *file) {
    fprintf(file, "v %f %f %f\n", pos[0], pos[1], pos[2]);
}

void exportMesh(Mesh *mesh, FILE *file) {
    int vertIndex = 1;
    for (int i = 0; i < mesh->vertex_length; i += 6) {
        outputVertex(mesh->vertices[i].pos, file);
        outputVertex(mesh->vertices[i + 1].pos, file);
        outputVertex(mesh->vertices[i + 5].pos, file);
        outputVertex(mesh->vertices[i + 2].pos, file);
        fprintf(file, "f");
        for (int j = 0; j < 4; j++) {
            fprintf(file, " %d", vertIndex++);
        }
        fprintf(file, "\n");
    }
}

void destroyMesh(Mesh *mesh) {
    glDeleteVertexArrays(1, &mesh->VAO);
    glDeleteBuffers(1, &mesh->VBO);
    free(mesh->vertices);
    free(mesh);
}
