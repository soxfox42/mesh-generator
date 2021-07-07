#include "gui.h"
#include "loaders.h"
#include "user_data.h"

#include <stdlib.h>
#include <cglm/cglm.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

struct GUIState {
    GLuint VAO;
    GLuint VBO, EBO;
    GLuint shaderProgram;
    GLuint fontTex;
    struct nk_draw_null_texture null;
    struct nk_context nuklear;
    struct nk_buffer cmds, vbuf, ebuf;
    GLFWwindow *window;
    AppUserData *userData;
    bool inputEnabled;
};

typedef struct {
    vec2 pos;
    vec2 uv;
    GLubyte color[4];
} GUIVertex;

void addCharacter(GLFWwindow *window, unsigned int codepoint) {
    AppUserData *userData = glfwGetWindowUserPointer(window);
    userData->textBuffer[userData->textLength] = codepoint;
    userData->textLength++;
}

void setupGUIVAO(GLuint VAO, GLuint VBO, GLuint EBO) {
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GUIVertex),
                          (void *)offsetof(GUIVertex, pos));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GUIVertex),
                          (void *)offsetof(GUIVertex, uv));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GUIVertex),
                          (void *)offsetof(GUIVertex, color));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
}

GUIState *initGUI(GLFWwindow *window) {
    glfwSetCharCallback(window, addCharacter);

    GUIState *state = malloc(sizeof(GUIState));

    glGenVertexArrays(1, &state->VAO);
    glGenBuffers(1, &state->VBO);
    glGenBuffers(1, &state->EBO);

    setupGUIVAO(state->VAO, state->VBO, state->EBO);

    state->shaderProgram =
        loadShaderProgram("data/shaders/gui.vert", "data/shaders/gui.frag");
    glUseProgram(state->shaderProgram);
    glUniform1i(glGetUniformLocation(state->shaderProgram, "tex"), 0);

    // Generate a font texture for the default Nuklear font, and transfer it to
    // the GPU.
    struct nk_font_atlas atlas;
    nk_font_atlas_init_default(&atlas);
    nk_font_atlas_begin(&atlas);
    int w, h;
    const void *fontImage =
        nk_font_atlas_bake(&atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    glGenTextures(1, &state->fontTex);
    glBindTexture(GL_TEXTURE_2D, state->fontTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 fontImage);
    nk_font_atlas_end(&atlas, nk_handle_id(state->fontTex), &state->null);

    nk_init_default(&state->nuklear, &atlas.default_font->handle);

    nk_buffer_init_default(&state->cmds);
    nk_buffer_init_default(&state->vbuf);
    nk_buffer_init_default(&state->ebuf);

    state->window = window;
    state->userData = glfwGetWindowUserPointer(window);

    state->inputEnabled = true;

    return state;
}

struct nk_context *getGUIContext(GUIState *state) {
    return &state->nuklear;
}

void setInputState(GUIState *state, bool enabled) {
    state->inputEnabled = enabled;
}

void inputKey(GUIState *state, enum nk_keys nuklearKey, int glfwKey) {
    nk_input_key(&state->nuklear, nuklearKey,
                 glfwGetKey(state->window, glfwKey) == GLFW_PRESS);
}

void inputGUI(GUIState *state) {
    if (!state->inputEnabled) {
        state->userData->textLength = 0;
        return;
    }

    nk_input_begin(&state->nuklear);

    double mouseX, mouseY;
    glfwGetCursorPos(state->window, &mouseX, &mouseY);
    bool mousePressed =
        glfwGetMouseButton(state->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    nk_input_motion(&state->nuklear, (int)mouseX, (int)mouseY);
    nk_input_button(&state->nuklear, NK_BUTTON_LEFT, (int)mouseX, (int)mouseY,
                    mousePressed);

    // Input all characters in the buffer, then reset it to the start.
    for (int i = 0; i < state->userData->textLength; i++) {
        nk_input_unicode(&state->nuklear, state->userData->textBuffer[i]);
    }
    state->userData->textLength = 0;

    inputKey(state, NK_KEY_BACKSPACE, GLFW_KEY_BACKSPACE);
    inputKey(state, NK_KEY_LEFT, GLFW_KEY_LEFT);
    inputKey(state, NK_KEY_RIGHT, GLFW_KEY_RIGHT);
    inputKey(state, NK_KEY_UP, GLFW_KEY_UP);
    inputKey(state, NK_KEY_DOWN, GLFW_KEY_DOWN);
    inputKey(state, NK_KEY_ENTER, GLFW_KEY_ENTER);

    nk_input_end(&state->nuklear);
}

void renderGUI(GUIState *state, int width, int height, float scale) {
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    glUseProgram(state->shaderProgram);

    mat4 projection;
    glm_ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f, projection);
    glUniformMatrix4fv(glGetUniformLocation(state->shaderProgram, "projection"),
                       1, GL_FALSE, (float *)projection);

    static const struct nk_draw_vertex_layout_element vertex_layout[] = {
        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(GUIVertex, pos)},
        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(GUIVertex, uv)},
        {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, offsetof(GUIVertex, color)},
        {NK_VERTEX_LAYOUT_END}};

    struct nk_convert_config config;
    config.global_alpha = 1.0f;
    config.line_AA = NK_ANTI_ALIASING_OFF;
    config.shape_AA = NK_ANTI_ALIASING_OFF;
    config.circle_segment_count = 16;
    config.arc_segment_count = 16;
    config.curve_segment_count = 16;
    config.null = state->null;
    config.vertex_layout = vertex_layout;
    config.vertex_size = sizeof(GUIVertex);
    config.vertex_alignment = NK_ALIGNOF(GUIVertex);

    nk_convert(&state->nuklear, &state->cmds, &state->vbuf, &state->ebuf,
               &config);

    glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->EBO);
    glBufferData(GL_ARRAY_BUFFER, nk_buffer_total(&state->vbuf),
                 nk_buffer_memory(&state->vbuf), GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, nk_buffer_total(&state->ebuf),
                 nk_buffer_memory(&state->ebuf), GL_STREAM_DRAW);

    glBindVertexArray(state->VAO);
    const struct nk_draw_command *cmd;
    nk_draw_index *offset = NULL;
    nk_draw_foreach(cmd, &state->nuklear, &state->cmds) {
        if (!cmd->elem_count) continue;
        glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
        glScissor((cmd->clip_rect.x - 1) * scale,
                  (height - cmd->clip_rect.y - cmd->clip_rect.h) * scale,
                  (cmd->clip_rect.w + 2) * scale, cmd->clip_rect.h * scale);
        glDrawElements(GL_TRIANGLES, cmd->elem_count, GL_UNSIGNED_SHORT,
                       offset);
        offset += cmd->elem_count;
    }
    nk_buffer_clear(&state->cmds);
    nk_buffer_clear(&state->vbuf);
    nk_buffer_clear(&state->ebuf);
    nk_clear(&state->nuklear);
}

void destroyGUI(GUIState *state) {
    glDeleteVertexArrays(1, &state->VAO);
    glDeleteBuffers(1, &state->VBO);
    glDeleteBuffers(1, &state->EBO);

    glDeleteTextures(1, &state->fontTex);

    nk_free(&state->nuklear);

    free(state);
}
