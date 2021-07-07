#include "generator.h"
#include "expr.h"
#include "mesh.h"

#include <stdlib.h>
#include <cglm/cglm.h>

#define VEC_DELTA 0.01
#define STEP_SIZE 0.3
#define MASS_BIAS 0.1
#define MIN_MOVE_FRAC (1.0 / 20.0)
#define ZERO_TOLERANCE 0.001

typedef enum { INTERSECT_POS, INTERSECT_NEG, INTERSECT_NONE } IntersectType;
typedef struct {
    IntersectType intersectType;
    vec3 position;
    vec3 normal;
} Edge;

typedef struct {
    Edge *intersections[12];
    int intersectionCount;
    vec3 massPoint;
} Cell;

typedef enum { DIR_X, DIR_Y, DIR_Z } EdgeDir;

struct Generator {
    int subdivisions;  // Number of cells in each axis.
    Window window;
    Token *sdfExpr;
    float threshold;
    float *samples;
    Edge *edges;
    vec3 *vertices;
};

Generator *createGenerator() {
    Generator *gen = malloc(sizeof(Generator));
    gen->samples = NULL;
    gen->edges = NULL;
    gen->vertices = NULL;
    return gen;
}

void setGeneratorSize(Generator *gen, int subdivisions) {
    if (subdivisions == gen->subdivisions) return;
    gen->subdivisions = subdivisions;
    // subdivisions is the number of cells, and there must be samples on all
    // vertices of each cell.
    // Required memory: (subdivisions + 1)^3 floats.
    int sampleSide = subdivisions + 1;
    int sampleMem = sampleSide * sampleSide * sampleSide * sizeof(float);
    gen->samples = realloc(gen->samples, sampleMem);
    // For each sample point, up to three edges may be present.
    // Required memory: (subdivisions + 1)^3 * 3 Edges.
    int edgeMem = sampleSide * sampleSide * sampleSide * sizeof(Edge) * 3;
    gen->edges = realloc(gen->edges, edgeMem);
    // The cube is subdivided in each axis into [subdivisions] cells.
    // Required memory: subdivisions^3 vec3s.
    int vertexMem = subdivisions * subdivisions * subdivisions * sizeof(vec3);
    gen->vertices = realloc(gen->vertices, vertexMem);
}

void setGeneratorWindow(Generator *gen, Window window) { gen->window = window; }

void setGeneratorSDF(Generator *gen, Token *expr) { gen->sdfExpr = expr; }

void setGeneratorThreshold(Generator *gen, float threshold) {
    gen->threshold = threshold;
}

// Calculate 1D memory indices for 3D sample coordinates.
int sampleIndex(Generator *gen, int x, int y, int z) {
    int stride = gen->subdivisions + 1;
    return (z * stride + y) * stride + x;
}

// Calculate 1D memory indices for 3D edge coordinates + a direction.
int edgeIndex(Generator *gen, int x, int y, int z, EdgeDir dir) {
    int stride = gen->subdivisions + 1;
    return ((z * stride + y) * stride + x) * 3 + dir;
}

// Calculate 1D memory indices for 3D cell coordinates.
int vertexIndex(Generator *gen, int x, int y, int z) {
    int stride = gen->subdivisions;
    return (z * stride + y) * stride + x;
}

// Calculate the vector to evaluate the SDF at for a sample.
void getSampleVector(Generator *gen, int x, int y, int z, vec3 out) {
    vec3 gridVector = {x, y, z};
    vec3 unitCubeVector;
    glm_vec3_divs(gridVector, gen->subdivisions, unitCubeVector);
    vec3 windowExtent;
    glm_vec3_sub(gen->window.max, gen->window.min, windowExtent);
    glm_vec3_copy(gen->window.min, out);
    glm_vec3_muladd(unitCubeVector, windowExtent, out);
}

void generateOneSample(Generator *gen, int x, int y, int z) {
    vec3 sampleVector;
    getSampleVector(gen, x, y, z, sampleVector);
    float sampledValue = evaluateExpression(gen->sdfExpr, sampleVector);
    gen->samples[sampleIndex(gen, x, y, z)] = sampledValue;
}

void generateSamples(Generator *gen) {
    int sideLength = gen->subdivisions + 1;
    for (int z = 0; z < sideLength; z++) {
        for (int y = 0; y < sideLength; y++) {
            for (int x = 0; x < sideLength; x++) {
                generateOneSample(gen, x, y, z);
            }
        }
    }
}

// Approximate a normal from the SDF by sampling at arbitrarily small offsets.
void generateApproxNormal(Generator *gen, vec3 pos, vec3 normal, float delta) {
    float value = evaluateExpression(gen->sdfExpr, pos);
    vec3 temp;
    glm_vec3_add(pos, (vec3){delta, 0, 0}, temp);
    normal[0] = evaluateExpression(gen->sdfExpr, temp) - value;
    glm_vec3_add(pos, (vec3){0, delta, 0}, temp);
    normal[1] = evaluateExpression(gen->sdfExpr, temp) - value;
    glm_vec3_add(pos, (vec3){0, 0, delta}, temp);
    normal[2] = evaluateExpression(gen->sdfExpr, temp) - value;
    glm_vec3_normalize(normal);
}

// Use iterative interpolation to find a zero along an edge.
void generateOneEdge(Generator *gen, int x, int y, int z, EdgeDir dir) {
    vec3 a, b;
    float valueA, valueB;
    getSampleVector(gen, x, y, z, a);
    valueA = gen->samples[sampleIndex(gen, x, y, z)];
    switch (dir) {
        case DIR_X:
            getSampleVector(gen, x + 1, y, z, b);
            valueB = gen->samples[sampleIndex(gen, x + 1, y, z)];
            break;
        case DIR_Y:
            getSampleVector(gen, x, y + 1, z, b);
            valueB = gen->samples[sampleIndex(gen, x, y + 1, z)];
            break;
        case DIR_Z:
            getSampleVector(gen, x, y, z + 1, b);
            valueB = gen->samples[sampleIndex(gen, x, y, z + 1)];
            break;
    }
    float range = 1.0;
    int iterations = 0;
    while (range > 0.01 && iterations < 5) {
        float t = (gen->threshold - valueA) / (valueB - valueA);
        vec3 interpolatedVector;
        glm_vec3_lerp(a, b, t, interpolatedVector);
        float newValue = evaluateExpression(gen->sdfExpr, interpolatedVector);
        if (fabs(newValue - gen->threshold) < ZERO_TOLERANCE) break;
        if ((newValue > gen->threshold) == (valueA > gen->threshold)) {
            valueA = newValue;
            glm_vec3_copy(interpolatedVector, a);
            range *= (1 - t);
        } else {
            valueB = newValue;
            glm_vec3_copy(interpolatedVector, b);
            range *= t;
        }
        iterations++;
    }

    int index = edgeIndex(gen, x, y, z, dir);
    float t = (gen->threshold - valueA) / (valueB - valueA);
    glm_vec3_lerp(a, b, t, gen->edges[index].position);
    generateApproxNormal(gen, gen->edges[index].position,
                         gen->edges[index].normal, VEC_DELTA);
}

bool checkEdgeIntersection(Generator *gen, int x, int y, int z, EdgeDir dir) {
    float valueA = gen->samples[sampleIndex(gen, x, y, z)];
    float valueB;
    switch (dir) {
        case DIR_X:
            valueB = gen->samples[sampleIndex(gen, x + 1, y, z)];
            break;
        case DIR_Y:
            valueB = gen->samples[sampleIndex(gen, x, y + 1, z)];
            break;
        case DIR_Z:
            valueB = gen->samples[sampleIndex(gen, x, y, z + 1)];
            break;
    }
    valueA -= gen->threshold;
    valueB -= gen->threshold;
    bool isIntersection = (valueA > 0) != (valueB > 0);
    if (isIntersection) {
        int index = edgeIndex(gen, x, y, z, dir);
        if (valueA > valueB) {
            gen->edges[index].intersectType = INTERSECT_NEG;
        } else {
            gen->edges[index].intersectType = INTERSECT_POS;
        }
    }
    return isIntersection;
}

void clearEdges(Generator *gen) {
    int edgeSide = gen->subdivisions + 1;
    int edgeCount = edgeSide * edgeSide * edgeSide * 3;

    for (int i = 0; i < edgeCount; i++) {
        gen->edges[i].intersectType = INTERSECT_NONE;
    }
}

void generateEdges(Generator *gen) {
    clearEdges(gen);
    // Only need the first n - 1 vertices, the others are always external edges.
    int sideLength = gen->subdivisions;
    for (int z = 0; z < sideLength; z++) {
        for (int y = 0; y < sideLength; y++) {
            for (int x = 0; x < sideLength; x++) {
                if (y > 0 && z > 0 &&
                    checkEdgeIntersection(gen, x, y, z, DIR_X)) {
                    generateOneEdge(gen, x, y, z, DIR_X);
                }
                if (x > 0 && z > 0 &&
                    checkEdgeIntersection(gen, x, y, z, DIR_Y)) {
                    generateOneEdge(gen, x, y, z, DIR_Y);
                }
                if (x > 0 && y > 0 &&
                    checkEdgeIntersection(gen, x, y, z, DIR_Z)) {
                    generateOneEdge(gen, x, y, z, DIR_Z);
                }
            }
        }
    }
}

float vertexError(vec3 point, Cell *cell) {
    float faceError = 0.0;
    for (int i = 0; i < cell->intersectionCount; i++) {
        Edge *edge = cell->intersections[i];
        vec3 relativePosition;
        glm_vec3_sub(point, edge->position, relativePosition);
        float planeDistance = glm_vec3_dot(relativePosition, edge->normal);
        faceError += planeDistance * planeDistance;
    }
    faceError /= cell->intersectionCount;
    float massError = glm_vec3_distance2(point, cell->massPoint);
    return faceError + massError * MASS_BIAS;
}

void descentStep(vec3 point, Cell *cell, float delta, vec3 stepDir) {
    float value = vertexError(point, cell);
    vec3 temp;
    glm_vec3_add(point, (vec3){delta, 0, 0}, temp);
    stepDir[0] = vertexError(temp, cell) - value;
    glm_vec3_add(point, (vec3){0, delta, 0}, temp);
    stepDir[1] = vertexError(temp, cell) - value;
    glm_vec3_add(point, (vec3){0, 0, delta}, temp);
    stepDir[2] = vertexError(temp, cell) - value;
    glm_vec3_divs(stepDir, -delta, stepDir);
}

// Add an intersection to a cell.
void pushCellIntersection(Cell *cell, Edge *intersection) {
    cell->intersections[cell->intersectionCount] = intersection;
    cell->intersectionCount++;
}

// Calculate the mass point of a cell.
void calculateMassPoint(Cell *cell) {
    glm_vec3_zero(cell->massPoint);
    for (int i = 0; i < cell->intersectionCount; i++) {
        Edge *intersection = cell->intersections[i];
        glm_vec3_add(cell->massPoint, intersection->position, cell->massPoint);
    }
    glm_vec3_divs(cell->massPoint, cell->intersectionCount, cell->massPoint);
}

void generateOneVertex(Generator *gen, int x, int y, int z, float minMove) {
    // The most efficient method of checking all edges here is to simply list
    // out their indices, then scan through checking which are intersections.
    int indices[] = {
        edgeIndex(gen, x, y, z, DIR_X),
        edgeIndex(gen, x, y, z + 1, DIR_X),
        edgeIndex(gen, x, y + 1, z, DIR_X),
        edgeIndex(gen, x, y + 1, z + 1, DIR_X),
        edgeIndex(gen, x, y, z, DIR_Y),
        edgeIndex(gen, x, y, z + 1, DIR_Y),
        edgeIndex(gen, x + 1, y, z, DIR_Y),
        edgeIndex(gen, x + 1, y, z + 1, DIR_Y),
        edgeIndex(gen, x, y, z, DIR_Z),
        edgeIndex(gen, x, y + 1, z, DIR_Z),
        edgeIndex(gen, x + 1, y, z, DIR_Z),
        edgeIndex(gen, x + 1, y + 1, z, DIR_Z),
    };
    Cell cell;
    cell.intersectionCount = 0;
    for (int i = 0; i < 12; i++) {
        if (gen->edges[indices[i]].intersectType != INTERSECT_NONE) {
            pushCellIntersection(&cell, &gen->edges[indices[i]]);
        }
    }
    if (cell.intersectionCount == 0) {
        return;
    }
    calculateMassPoint(&cell);

    vec3 *vertex = &gen->vertices[vertexIndex(gen, x, y, z)];
    glm_vec3_copy(cell.massPoint, *vertex);
    int iterations = 0;
    vec3 stepDir;
    do {
        descentStep(*vertex, &cell, VEC_DELTA, stepDir);
        glm_vec3_muladds(stepDir, STEP_SIZE, *vertex);
        iterations++;
    } while (iterations < 10 && glm_vec3_norm2(stepDir) > minMove * minMove);
}

void generateVertices(Generator *gen) {
    vec3 windowExtent;
    glm_vec3_sub(gen->window.max, gen->window.min, windowExtent);
    float diagonalLength = glm_vec3_norm(windowExtent);
    float cellDiagonalLength = diagonalLength / gen->subdivisions;
    float minMove = cellDiagonalLength * MIN_MOVE_FRAC;
    // This pass iterates through each internal cell, the number of which is
    // specified by subdivisions.
    int sideLength = gen->subdivisions;
    for (int z = 0; z < sideLength; z++) {
        for (int y = 0; y < sideLength; y++) {
            for (int x = 0; x < sideLength; x++) {
                generateOneVertex(gen, x, y, z, minMove);
            }
        }
    }
}

void generateFaces(Generator *gen, Mesh *mesh, bool invertNormals) {
    clearMesh(mesh);
    int sideLength = gen->subdivisions;
    vec3 *vertexA, *vertexB, *vertexC, *vertexD;
    for (int z = 0; z < sideLength; z++) {
        for (int y = 0; y < sideLength; y++) {
            for (int x = 0; x < sideLength; x++) {
                Edge *edgeX = &gen->edges[edgeIndex(gen, x, y, z, DIR_X)];
                Edge *edgeY = &gen->edges[edgeIndex(gen, x, y, z, DIR_Y)];
                Edge *edgeZ = &gen->edges[edgeIndex(gen, x, y, z, DIR_Z)];
                if (edgeX->intersectType == INTERSECT_POS) {
                    vertexA = &gen->vertices[vertexIndex(gen, x, y - 1, z - 1)];
                    vertexB = &gen->vertices[vertexIndex(gen, x, y, z - 1)];
                    vertexC = &gen->vertices[vertexIndex(gen, x, y, z)];
                    vertexD = &gen->vertices[vertexIndex(gen, x, y - 1, z)];
                    addQuad(mesh, *vertexA, *vertexB, *vertexC, *vertexD,
                            invertNormals);
                } else if (edgeX->intersectType == INTERSECT_NEG) {
                    vertexA = &gen->vertices[vertexIndex(gen, x, y - 1, z - 1)];
                    vertexB = &gen->vertices[vertexIndex(gen, x, y - 1, z)];
                    vertexC = &gen->vertices[vertexIndex(gen, x, y, z)];
                    vertexD = &gen->vertices[vertexIndex(gen, x, y, z - 1)];
                    addQuad(mesh, *vertexA, *vertexB, *vertexC, *vertexD,
                            invertNormals);
                }
                if (edgeY->intersectType == INTERSECT_POS) {
                    vertexA = &gen->vertices[vertexIndex(gen, x - 1, y, z - 1)];
                    vertexB = &gen->vertices[vertexIndex(gen, x - 1, y, z)];
                    vertexC = &gen->vertices[vertexIndex(gen, x, y, z)];
                    vertexD = &gen->vertices[vertexIndex(gen, x, y, z - 1)];
                    addQuad(mesh, *vertexA, *vertexB, *vertexC, *vertexD,
                            invertNormals);
                } else if (edgeY->intersectType == INTERSECT_NEG) {
                    vertexA = &gen->vertices[vertexIndex(gen, x - 1, y, z - 1)];
                    vertexB = &gen->vertices[vertexIndex(gen, x, y, z - 1)];
                    vertexC = &gen->vertices[vertexIndex(gen, x, y, z)];
                    vertexD = &gen->vertices[vertexIndex(gen, x - 1, y, z)];
                    addQuad(mesh, *vertexA, *vertexB, *vertexC, *vertexD,
                            invertNormals);
                }
                if (edgeZ->intersectType == INTERSECT_POS) {
                    vertexA = &gen->vertices[vertexIndex(gen, x - 1, y - 1, z)];
                    vertexB = &gen->vertices[vertexIndex(gen, x, y - 1, z)];
                    vertexC = &gen->vertices[vertexIndex(gen, x, y, z)];
                    vertexD = &gen->vertices[vertexIndex(gen, x - 1, y, z)];
                    addQuad(mesh, *vertexA, *vertexB, *vertexC, *vertexD,
                            invertNormals);
                } else if (edgeZ->intersectType == INTERSECT_NEG) {
                    vertexA = &gen->vertices[vertexIndex(gen, x - 1, y - 1, z)];
                    vertexB = &gen->vertices[vertexIndex(gen, x - 1, y, z)];
                    vertexC = &gen->vertices[vertexIndex(gen, x, y, z)];
                    vertexD = &gen->vertices[vertexIndex(gen, x, y - 1, z)];
                    addQuad(mesh, *vertexA, *vertexB, *vertexC, *vertexD,
                            invertNormals);
                }
            }
        }
    }
}

void generateMesh(Generator *gen, Mesh *mesh, bool invertNormals) {
    generateSamples(gen);
    generateEdges(gen);
    generateVertices(gen);
    generateFaces(gen, mesh, invertNormals);
}

void destroyGenerator(Generator *gen) { free(gen); }