#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/freeglut.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "renderer.h"

namespace billiardgl {

void renderScene(const GameState& state, const RenderHooks& hooks)
{
    (void)state;

    if (hooks.renderRoom) {
        hooks.renderRoom();
    }
    if (hooks.renderTable) {
        hooks.renderTable();
    }
    if (hooks.renderBall) {
        hooks.renderBall();
    }
    if (hooks.renderCue) {
        hooks.renderCue();
    }
    if (hooks.renderDecoration) {
        hooks.renderDecoration();
    }
}

}  // namespace billiardgl
