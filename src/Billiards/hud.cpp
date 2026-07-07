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

#include "hud.h"

#include <string>

namespace billiardgl {

namespace {

void drawStringAt(float x, float y, void* font, const char* text)
{
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; ++c) {
        glutBitmapCharacter(font, *c);
    }
}

void drawScreenRect(float left, float bottom, float right, float top, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(left, bottom);
    glVertex2f(right, bottom);
    glVertex2f(right, top);
    glVertex2f(left, top);
    glEnd();
}

const char* aimModeText(const GameState& state)
{
    return state.aim.mode == AimMode::Aim ? "Mode: Aim | Tab Observe" : "Mode: Observe | Tab Aim";
}

}  // namespace

void drawHelpPrompt(const GameState& state)
{
    const char* prompt = state.hud.showHelp ? "Press H to close help" : "Press H for help";
    drawStringAt(18.0f, static_cast<float>(state.config.height) - 120.0f, GLUT_BITMAP_HELVETICA_18, prompt);
}

void drawHelpOverlay(const GameState& state)
{
    if (!state.hud.showHelp) {
        return;
    }

    const float panelWidth = 520.0f;
    const float panelHeight = 430.0f;
    const float left = (static_cast<float>(state.config.width) - panelWidth) * 0.5f;
    const float bottom = (static_cast<float>(state.config.height) - panelHeight) * 0.5f;
    const float top = bottom + panelHeight;
    const float textLeft = left + 34.0f;
    float y = top - 44.0f;

    drawScreenRect(left, bottom, left + panelWidth, top, 0.04f, 0.06f, 0.05f, 0.82f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    drawStringAt(textLeft, y, GLUT_BITMAP_TIMES_ROMAN_24, "BilliardGL Help");
    y -= 42.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Camera");
    y -= 28.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "W / S                 Zoom in / out");
    y -= 24.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "A / D                 Pan left / right");
    y -= 24.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Arrow keys            Orbit view");
    y -= 24.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Left mouse drag       Orbit view");
    y -= 24.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Mouse wheel           Zoom in / out");
    y -= 24.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Tab                   Toggle aim mode");
    y -= 24.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Aim mode horizontal   Adjust shot direction");
    y -= 38.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Play");
    y -= 28.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Aim wheel / +/-       Adjust shot power");
    y -= 24.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Aim mode left click   Hit cue ball");
    y -= 24.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "H                     Toggle help");
    y -= 24.0f;
    drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Esc                   Quit");
}

void drawHud(const GameState& state)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, state.config.width, 0.0, state.config.height);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    const std::string playerText = "Current Player: Player " + std::to_string(state.players.currentPlayer + 1);
    drawStringAt(18.0f, static_cast<float>(state.config.height) - 68.0f, GLUT_BITMAP_TIMES_ROMAN_24, playerText.c_str());
    drawStringAt(18.0f, static_cast<float>(state.config.height) - 94.0f, GLUT_BITMAP_HELVETICA_18, aimModeText(state));
    drawHelpPrompt(state);
    drawHelpOverlay(state);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

}  // namespace billiardgl
