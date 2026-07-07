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

#include "ObjLoader.h"
#include "particle.h"
#include "renderer.h"
#include "shot.h"

#include <cmath>

namespace billiardgl {
namespace {

constexpr float kRoomWidth = 1000.0f;
constexpr float kRoomLength = 1000.0f;
constexpr float kRoomHeight = 400.0f;

void drawBall(float radius, unsigned int texture)
{
    GLUquadricObj* ball = gluNewQuadric();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    gluQuadricTexture(ball, GL_TRUE);
    gluSphere(ball, radius, 80, 120);
    gluDeleteQuadric(ball);
}

void renderRoom(const RenderResources& resources)
{
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glColor3f(0.18f, 0.20f, 0.19f);
    glBegin(GL_QUADS);
    glVertex3f(-kRoomWidth / 2.0f, 0.0f, kRoomLength / 2.0f);
    glVertex3f(-kRoomWidth / 2.0f, 0.0f, -kRoomLength / 2.0f);
    glVertex3f(kRoomWidth / 2.0f, 0.0f, -kRoomLength / 2.0f);
    glVertex3f(kRoomWidth / 2.0f, 0.0f, kRoomLength / 2.0f);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, resources.ceilingTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-kRoomWidth / 2.0f, kRoomHeight, kRoomLength / 2.0f);
    glTexCoord2f(0.0f, 5.0f);
    glVertex3f(-kRoomWidth / 2.0f, kRoomHeight, -kRoomLength / 2.0f);
    glTexCoord2f(5.0f, 5.0f);
    glVertex3f(kRoomWidth / 2.0f, kRoomHeight, -kRoomLength / 2.0f);
    glTexCoord2f(5.0f, 0.0f);
    glVertex3f(kRoomWidth / 2.0f, kRoomHeight, kRoomLength / 2.0f);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glColor3f(0.24f, 0.27f, 0.26f);
    glBegin(GL_QUADS);
    glVertex3f(-kRoomWidth / 2.0f, 0.0f, -kRoomLength / 2.0f);
    glVertex3f(-kRoomWidth / 2.0f, kRoomHeight, -kRoomLength / 2.0f);
    glVertex3f(kRoomWidth / 2.0f, kRoomHeight, -kRoomLength / 2.0f);
    glVertex3f(kRoomWidth / 2.0f, 0.0f, -kRoomLength / 2.0f);
    glVertex3f(-kRoomWidth / 2.0f, 0.0f, -kRoomLength / 2.0f);
    glVertex3f(-kRoomWidth / 2.0f, kRoomHeight, -kRoomLength / 2.0f);
    glVertex3f(-kRoomWidth / 2.0f, kRoomHeight, kRoomLength / 2.0f);
    glVertex3f(-kRoomWidth / 2.0f, 0.0f, kRoomLength / 2.0f);
    glVertex3f(-kRoomWidth / 2.0f, 0.0f, kRoomLength / 2.0f);
    glVertex3f(-kRoomWidth / 2.0f, kRoomHeight, kRoomLength / 2.0f);
    glVertex3f(kRoomWidth / 2.0f, kRoomHeight, kRoomLength / 2.0f);
    glVertex3f(kRoomWidth / 2.0f, 0.0f, kRoomLength / 2.0f);
    glVertex3f(kRoomWidth / 2.0f, 0.0f, kRoomLength / 2.0f);
    glVertex3f(kRoomWidth / 2.0f, kRoomHeight, kRoomLength / 2.0f);
    glVertex3f(kRoomWidth / 2.0f, kRoomHeight, -kRoomLength / 2.0f);
    glVertex3f(kRoomWidth / 2.0f, 0.0f, -kRoomLength / 2.0f);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
}

void renderTable(const RenderResources& resources)
{
    if (!resources.tableObj) {
        return;
    }

    ObjLoader& tableObj = *resources.tableObj;
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, resources.tableVertexVBO);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glNormalPointer(GL_FLOAT, 0, reinterpret_cast<void*>(sizeof(GLfloat) * tableObj.vertices.size() * 3));
    glVertexPointer(3, GL_FLOAT, 0, 0);
    glTexCoordPointer(3, GL_FLOAT, 0, reinterpret_cast<void*>((sizeof(GLfloat) * tableObj.vertices.size() * 3) * 2));
    glBindTexture(GL_TEXTURE_2D, resources.tableTextures[tableObj.mtlIndex[1]]);
    glDrawArrays(GL_TRIANGLES, 0, tableObj.mtlIndex[4]);
    glBindTexture(GL_TEXTURE_2D, resources.tableTextures[tableObj.mtlIndex[5]]);
    glDrawArrays(GL_TRIANGLES, tableObj.mtlIndex[4], tableObj.mtlIndex[6] / 2);
    glBindTexture(GL_TEXTURE_2D, resources.tableTextures[tableObj.mtlIndex[1]]);
    glDrawArrays(GL_TRIANGLES, tableObj.mtlIndex[6] / 2, tableObj.vertices.size());
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void renderDecoration(const RenderResources& resources)
{
    if (resources.benchObj) {
        ObjLoader& benchObj = *resources.benchObj;
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, resources.benchVertexVBO);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
        glNormalPointer(GL_FLOAT, 0, reinterpret_cast<void*>(sizeof(GLfloat) * benchObj.vertices.size() * 3));
        glVertexPointer(3, GL_FLOAT, 0, 0);
        glTexCoordPointer(3, GL_FLOAT, 0, reinterpret_cast<void*>((sizeof(GLfloat) * benchObj.vertices.size() * 3) * 2));
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
        glTranslatef(0.0f, 0.0f, -kRoomWidth / 2.0f + 50.0f);
        glBindTexture(GL_TEXTURE_2D, resources.blackTexture);
        glDrawArrays(GL_TRIANGLES, 0, benchObj.vertices.size());
        glPopMatrix();
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    }

    if (resources.wardObj) {
        ObjLoader& wardObj = *resources.wardObj;
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, resources.wardVertexVBO);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
        glNormalPointer(GL_FLOAT, 0, reinterpret_cast<void*>(sizeof(GLfloat) * wardObj.vertices.size() * 3));
        glVertexPointer(3, GL_FLOAT, 0, 0);
        glTexCoordPointer(3, GL_FLOAT, 0, reinterpret_cast<void*>((sizeof(GLfloat) * wardObj.vertices.size() * 3) * 2));
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
        glTranslatef(0.0f, 0.0f, -kRoomWidth / 2.0f + 30.0f);
        glBindTexture(GL_TEXTURE_2D, resources.wardTexture);
        glDrawArrays(GL_TRIANGLES, 0, wardObj.vertices.size());
        glPopMatrix();
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    }
}

void renderBalls(const GameState& state, const RenderResources& resources)
{
    GLfloat shadowMatrix[16] = {};
    for (int i = 0; i < kBallCount; ++i) {
        const BallState& ball = state.balls[i];
        if (!ball.pocketed) {
            for (int j = 0; j < 15; ++j) {
                shadowMatrix[j] = 0.0f;
            }
            shadowMatrix[0] = shadowMatrix[5] = shadowMatrix[10] = 1.0f;
            shadowMatrix[7] = -1.0f / 405.0f;
            glPushMatrix();
            glTranslatef(ball.position.x, ball.position.y, ball.position.z);
            glPushMatrix();
            glTranslatef(0.0f, 400.0f, 0.0f);
            glMultMatrixf(shadowMatrix);
            glTranslatef(0.0f, -400.0f, 0.0f);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glColor4f(0.3f, 0.3f, 0.3f, 0.5f);
            glBlendFunc(GL_SRC_ALPHA, GL_DST_COLOR);
            drawBall(kBallRadius, resources.blackTexture);
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glPopMatrix();
            glRotatef(ball.rotationAngle, ball.rotationAxis.x, ball.rotationAxis.y, ball.rotationAxis.z);
            drawBall(kBallRadius, ball.texture);
            glPopMatrix();
        } else {
            glPushMatrix();
            glTranslatef(ball.position.x, ball.position.y, ball.position.z);
            drawBall(kBallRadius, ball.texture);
            glPopMatrix();
        }
    }
}

void renderCue(const GameState& state, const RenderResources& resources)
{
    if (!resources.cueObj) {
        return;
    }

    const BallState& cueBall = state.balls[0];
    const Point3 lineStart = cueLineStartFromAim(state.aim.yaw);
    const Point3 lineEnd = cueLineEndFromAim(state.aim.yaw, 150.0f);
    const Point3 cuePosition = cueStickPositionFromAim(cueBall.position, state.aim.yaw, resources.shotPower);

    glPushMatrix();
    glTranslatef(cueBall.position.x, cueBall.position.y, cueBall.position.z);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glColor3f(1.0f, 0.88f, 0.1f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex3f(lineStart.x, lineStart.y, lineStart.z);
    glVertex3f(lineEnd.x, lineEnd.y, lineEnd.z);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glPopMatrix();

    ObjLoader& cueObj = *resources.cueObj;
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, resources.cueVertexVBO);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glNormalPointer(GL_FLOAT, 0, reinterpret_cast<void*>(sizeof(GLfloat) * cueObj.vertices.size() * 3));
    glVertexPointer(3, GL_FLOAT, 0, 0);
    glTexCoordPointer(3, GL_FLOAT, 0, reinterpret_cast<void*>((sizeof(GLfloat) * cueObj.vertices.size() * 3) * 2));
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(cuePosition.x, cuePosition.y, cuePosition.z);
    glRotatef(cueStickRotationDegreesFromAim(state.aim.yaw), 0.0f, 1.0f, 0.0f);
    glBindTexture(GL_TEXTURE_2D, resources.cueTextures[0]);
    glDrawArrays(GL_TRIANGLES, cueObj.mtlIndex[4], cueObj.vertices.size());
    glBindTexture(GL_TEXTURE_2D, resources.cueTextures[1]);
    glDrawArrays(GL_TRIANGLES, cueObj.mtlIndex[2], cueObj.mtlIndex[4]);
    glBindTexture(GL_TEXTURE_2D, cueBall.texture);
    glDrawArrays(GL_TRIANGLES, 0, cueObj.mtlIndex[2]);
    glPopMatrix();
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void renderPowerMeter(const RenderResources& resources)
{
    if (!resources.showPowerMeter) {
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, resources.viewportWidth, 0, resources.viewportHeight);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPushMatrix();
    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glColor4f(1.0f, 0.82f, 0.12f, 0.72f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const GLfloat meterLeft = 18.0f;
    const GLfloat meterBottom = static_cast<GLfloat>(resources.viewportHeight) - 260.0f;
    const GLfloat meterTop = meterBottom + resources.shotPower;
    glRectf(meterLeft, meterBottom, meterLeft + 36.0f, meterTop);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void renderParticles(const GameState& state, RenderResources& resources)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
    for (int i = 0; i < kBallCount; ++i) {
        if ((resources.fired[i] || resources.allFired) && resources.emitters[i]) {
            const BallState& ball = state.balls[i];
            resources.emitters[i]->emit(
                -kBallRadius + ball.position.x,
                kBallRadius + ball.position.x,
                ball.position.y,
                ball.position.y,
                ball.position.z,
                ball.position.z);
            resources.emitters[i]->show();
        }
    }
    glDisable(GL_BLEND);
}

}  // namespace

void setupCameraFromGameState(const GameState& state)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(
        60.0f,
        static_cast<GLdouble>(state.config.width) / static_cast<GLdouble>(state.config.height),
        10.0,
        10000.0);
    glTranslatef(state.camera.panX, state.camera.panY, state.camera.panZ);
    glMatrixMode(GL_TEXTURE);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        state.camera.eye[0],
        state.camera.eye[1],
        state.camera.eye[2],
        state.camera.target[0],
        state.camera.target[1],
        state.camera.target[2],
        0.0,
        1.0,
        0.0);
}

void setupLights()
{
    GLfloat lightAmbient[] = {0.65f, 0.65f, 0.65f, 1.0f};
    GLfloat lightDiffuse[] = {0.9f, 0.9f, 0.9f, 1.0f};
    GLfloat lightSpecular[] = {0.55f, 0.55f, 0.55f, 1.0f};
    GLfloat lightPosition[] = {0.0f, 460.0f, 0.0f, 1.0f};
    GLfloat lightDirection[] = {0.0f, -1.0f, 0.0f};

    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, lightDirection);
    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 180.0f);
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 0.0f);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat matAmbient[] = {0.35f, 0.35f, 0.35f, 1.0f};
    GLfloat matDiffuse[] = {0.9f, 0.9f, 0.9f, 1.0f};
    GLfloat matSpecular[] = {0.55f, 0.55f, 0.55f, 1.0f};
    GLfloat matShininess[] = {45.0f};
    glMaterialfv(GL_FRONT, GL_AMBIENT, matAmbient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, matDiffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matSpecular);
    glMaterialfv(GL_FRONT, GL_SHININESS, matShininess);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
}

void renderScene(const GameState& state, RenderResources& resources)
{
    renderRoom(resources);
    renderTable(resources);
    renderBalls(state, resources);
    if (resources.showCue) {
        renderCue(state, resources);
    }
    renderDecoration(resources);
    renderParticles(state, resources);
    renderPowerMeter(resources);
}

}  // namespace billiardgl
