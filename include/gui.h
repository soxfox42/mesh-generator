#ifndef GUI_H
#define GUI_H

#include <cglm/cglm.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "nk_options.h"
#include <nuklear.h>

typedef struct GUIState GUIState;

/// Creates a GUIState struct to manage Nuklear rendering.
GUIState *initGUI(GLFWwindow *window);
/// Retrieves the Nuklear context from a GUIState.
struct nk_context *getGUIContext(GUIState *state);
/// Enables or disables the GUI's input functionality.
void setInputState(GUIState *state, bool enabled);
/// Updates the Nuklear input state.
void inputGUI(GUIState *state);
/// Renders the current state of the GUI.
void renderGUI(GUIState *state, int width, int height, float scale);
/// Destroys a GUIState, cleaning up all memory and GPU objects.
void destroyGUI(GUIState *state);

#endif
