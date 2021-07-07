#ifndef GENERATOR_H
#define GENERATOR_H

#include "expr.h"
#include "mesh.h"

#include <cglm/cglm.h>

typedef struct {
    vec3 min, max;
} Window;

typedef struct Generator Generator;

Generator *createGenerator();
void setGeneratorSize(Generator *gen, int subdivisions);
void setGeneratorWindow(Generator *gen, Window window);
void setGeneratorSDF(Generator *gen, Token *expr);
void setGeneratorThreshold(Generator *gen, float threshold);
void generateMesh(Generator *gen, Mesh *mesh, bool invertNormals);
void destroyGenerator(Generator *gen);

#endif
