#include "expr.h"
#include "loaders.h"
#include "mesh.h"
#include "generator.h"
#include "gui.h"
#include "user_data.h"

#include <stdio.h>
#include <stdlib.h>
#define CGLM_DEFINE_PRINTS
#include <cglm/cglm.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define NK_IMPLEMENTATION
#include "nk_options.h"
#include <nuklear.h>

// Settings
const double MOVE_SPEED = 2.0;
const double MOUSE_SENSITIVITY = 0.003;
const double MAX_PITCH = M_PI / 2.0 - 0.01;

/// Creates a GLFW window with the necessary OpenGL context.
GLFWwindow *createWindow(AppUserData *userData) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow *window =
        glfwCreateWindow(1024, 768, "SDF Mesh Generator", NULL, NULL);
    if (window) glfwMakeContextCurrent(window);

    glfwSetWindowUserPointer(window, userData);

    // Load OpenGL function pointers using Glad.
    if (!gladLoadGL()) {
        fprintf(stderr, "Failed to load OpenGL.\n");
    }

    return window;
}

int main() {
    glfwInit();

    AppUserData userData = {0};
    GLFWwindow *window = createWindow(&userData);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window.\n");
        glfwTerminate();
        return 1;
    }

    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Load the shaders used for cube display.
    GLuint shaderProgram = loadShaderProgram("data/shaders/simple.vert",
                                             "data/shaders/simple.frag");

    // Set up a Nuklear context and a GUIState to handle the GUI.
    GUIState *gui = initGUI(window);
    struct nk_context *nuklear = getGUIContext(gui);

    // Camera state - an eye position, two angles, and two axes to rotate on.
    vec3 eye = {0.0f, 0.0f, 3.0f};
    float yaw = glm_rad(180.0f), pitch = 0.0f;
    vec3 up = {0.0f, 1.0f, 0.0f};
    vec3 right = {1.0f, 0.0f, 0.0f};

    double cursorX, cursorY, previousX, previousY;
    glfwGetCursorPos(window, &previousX, &previousY);
    bool flyPressed, previousPressed, currentPressed;

    double lastTime = glfwGetTime();

    Mesh *genMesh = createMesh(shaderProgram);
    Generator *gen = createGenerator();

    bool autoUpdate = true;

    int subdivisions = 32;
    Window genWindow = {{-1.5, -1.5, -1.5}, {1.5, 1.5, 1.5}};
    float threshold = 1.5;
    bool invertNormals = false;
    char sdfExpression[512] = "x^2 + y^2 + z^2 + noise(x, y, z)";
    char errMsg[128] = "";

    char exportFilename[64] = "sdf_export.obj";

    while (!glfwWindowShouldClose(window)) {
        double delta = glfwGetTime() - lastTime;
        lastTime = glfwGetTime();

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Camera angle is stored as yaw and pitch, here a vector is rotated by
        // these angles to generate the camera forward vector.
        vec3 forward = {0.0f, 0.0f, 1.0f};
        glm_vec3_rotate(forward, pitch, right);
        glm_vec3_rotate(forward, yaw, up);

        currentPressed =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (!flyPressed && currentPressed && !previousPressed) {
            // When the mouse button has just been pressed, if not hovering over
            // the GUI, switch into flycam mode and disable GUI input.
            if (!nk_item_is_any_active(nuklear)) {
                flyPressed = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                setInputState(gui, false);
            }
        } else if (flyPressed && previousPressed && !currentPressed) {
            // When released, reset state to normal, re-enabling GUI input.
            flyPressed = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            setInputState(gui, true);
        }
        previousPressed = currentPressed;

        glfwGetCursorPos(window, &cursorX, &cursorY);
        if (flyPressed) {
            yaw -= (cursorX - previousX) * MOUSE_SENSITIVITY;
            pitch += (cursorY - previousY) * MOUSE_SENSITIVITY;
            if (pitch > MAX_PITCH) {
                pitch = MAX_PITCH;
            }
            if (pitch < -MAX_PITCH) {
                pitch = -MAX_PITCH;
            }

            vec3 move = {0.0f, 0.0f, 0.0f};
            vec3 move_right;
            glm_vec3_crossn(forward, up, move_right);
            vec3 move_forward;
            glm_vec3_crossn(up, move_right, move_forward);
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                glm_vec3_add(move, move_forward, move);
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                glm_vec3_sub(move, move_forward, move);
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                glm_vec3_add(move, move_right, move);
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                glm_vec3_sub(move, move_right, move);
            }
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                glm_vec3_add(move, up, move);
            }
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                glm_vec3_sub(move, up, move);
            }
            glm_vec3_scale_as(move, MOVE_SPEED * delta, move);
            glm_vec3_add(eye, move, eye);
        }
        previousX = cursorX;
        previousY = cursorY;

        inputGUI(gui);

        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.0f, 0.1f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view;
        glm_look(eye, forward, up, view);
        mat4 projection;
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glm_perspective(glm_rad(60.0), (float)width / height, 0.1, 100.0,
                        projection);

        renderMesh(genMesh, view, projection);

        int logicalWidth, logicalHeight;
        glfwGetWindowSize(window, &logicalWidth, &logicalHeight);
        float scale;
        glfwGetWindowContentScale(window, &scale, NULL);

        if (nk_begin(
                nuklear, "Mesh Generator",
                nk_rect(10, 10, 400, logicalHeight - 20),
                NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MINIMIZABLE)) {
            nk_layout_row_dynamic(nuklear, 60, 1);
            if (nk_button_label(nuklear, "Generate Mesh") || autoUpdate) {
                Token sdfParsed[512];
                if (parseExpression(sdfExpression, sdfParsed, 512, errMsg) &&
                    validateExpression(sdfParsed, errMsg)) {
                    errMsg[0] = 0;
                    setGeneratorSize(gen, subdivisions);
                    setGeneratorSDF(gen, sdfParsed);
                    setGeneratorWindow(gen, genWindow);
                    setGeneratorThreshold(gen, threshold);
                    generateMesh(gen, genMesh, invertNormals);
                    updateMeshBuffer(genMesh);
                }
            }
            nk_layout_row_dynamic(nuklear, 30, 1);
            autoUpdate = nk_check_label(
                nuklear, "Auto Update (subdivisions < 64)", autoUpdate);
            const float ratio[] = {0.2, 0.8};
            nk_layout_row(nuklear, NK_DYNAMIC, 120, 2, ratio);
            nk_label(nuklear, "SDF: ", NK_TEXT_RIGHT);
            nk_edit_string_zero_terminated(nuklear, NK_EDIT_BOX, sdfExpression,
                                           512, nk_filter_default);
            if (strlen(errMsg) > 0) {
                nk_layout_row_dynamic(nuklear, 30, 1);
                nk_label_wrap(nuklear, errMsg);
            }
            nk_layout_row_dynamic(nuklear, 30, 1);
            nk_property_float(nuklear, "Threshold", -1000, &threshold, 1000, 1,
                              0.01);
            invertNormals =
                nk_check_label(nuklear, "Invert Normals", invertNormals);
            if (nk_tree_push(nuklear, NK_TREE_TAB, "SDF Window",
                             NK_MAXIMIZED)) {
                nk_layout_row_dynamic(nuklear, 30, 1);
                nk_property_int(nuklear, "Subdivisions", 2, &subdivisions, 128,
                                1, 0.5);
                if (subdivisions > 64) {
                    autoUpdate = false;
                }
                nk_layout_row_dynamic(nuklear, 30, 2);
                nk_property_float(nuklear, "X Min", -1000, &genWindow.min[0],
                                  1000, 1, 0.01);
                nk_property_float(nuklear, "X Max", -1000, &genWindow.max[0],
                                  1000, 1, 0.01);
                nk_property_float(nuklear, "Y Min", -1000, &genWindow.min[1],
                                  1000, 1, 0.01);
                nk_property_float(nuklear, "Y Max", -1000, &genWindow.max[1],
                                  1000, 1, 0.01);
                nk_property_float(nuklear, "Z Min", -1000, &genWindow.min[2],
                                  1000, 1, 0.01);
                nk_property_float(nuklear, "Z Max", -1000, &genWindow.max[2],
                                  1000, 1, 0.01);
                nk_tree_pop(nuklear);
            }
            if (nk_tree_push(nuklear, NK_TREE_TAB, "Export", NK_MAXIMIZED)) {
                nk_layout_row_dynamic(nuklear, 60, 1);
                if (nk_button_label(nuklear, "Export Model")) {
                    FILE *file = fopen(exportFilename, "w");
                    exportMesh(genMesh, file);
                    fclose(file);
                }
                const float ratio[] = {0.2, 0.8};
                nk_layout_row(nuklear, NK_DYNAMIC, 30, 2, ratio);
                nk_label(nuklear, "Filename: ", NK_TEXT_RIGHT);
                nk_edit_string_zero_terminated(nuklear, NK_EDIT_FIELD,
                                               exportFilename, 64,
                                               nk_filter_default);
                nk_tree_pop(nuklear);
            }
        }
        nk_end(nuklear);

        renderGUI(gui, logicalWidth, logicalHeight, scale);

        glfwSwapBuffers(window);
    }

    destroyGenerator(gen);
    destroyMesh(genMesh);
    glDeleteProgram(shaderProgram);

    destroyGUI(gui);

    glfwTerminate();
    return 0;
}
