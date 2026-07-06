#pragma once

#include <array>

namespace billiardgl {

constexpr int kBallCount = 16;
constexpr float kWindowWidth = 1024.0f;
constexpr float kWindowHeight = 768.0f;
constexpr float kTableInWidth = 124.5f;
constexpr float kTableInLength = 252.0f;
constexpr float kTableHeight = 87.0f;
constexpr float kPocketRadius = 8.5f;
constexpr float kBallRadius = 5.715f;
constexpr float kPi = 3.1415926f;
constexpr float kDefaultTimeStep = 0.1f;
constexpr float kDefaultFrictionAcceleration = -4.0f;

struct Point3 {
    Point3() = default;
    Point3(float xValue, float yValue, float zValue) : x(xValue), y(yValue), z(zValue) {}

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct BallState {
    Point3 position;
    Point3 velocity;
    Point3 rotationAxis;
    float speed = 0.0f;
    float rotationAngle = 0.0f;
    bool pocketed = false;
    unsigned int texture = 0;
};

struct CameraState {
    float target[3] = {0.0f, kTableHeight + kBallRadius, -kTableInLength / 4.0f};
    float eye[3] = {0.0f, 200.0f, -kTableInLength / 4.0f};
    float zoom = 120.0f;
    float angleX = -kPi / 2.0f;
    float angleY = kPi / 3.0f;
};

struct PlayerState {
    int assignedBallType[2] = {-1, -1};
    bool firstPocketedObjectBall = true;
    int currentPlayer = 0;
    int nextPlayer = 0;
    bool illegalShot = false;
    bool updatedAfterShot = false;
    bool shotTaken = false;
};

struct InputState {
    int mouseX = 0;
    int mouseY = 0;
    bool leftMouseDown = false;
    bool rightMouseDown = false;
    bool trackpadOrbit = false;
    bool waitingForHit = false;
    bool hitRequested = false;
    float shotPower = 0.0f;
};

struct HudState {
    bool showHelp = false;
};

struct RuntimeConfig {
    bool windowedMode = true;
    int width = 1024;
    int height = 768;
};

struct GameState {
    std::array<BallState, kBallCount> balls;
    CameraState camera;
    PlayerState players;
    InputState input;
    HudState hud;
    RuntimeConfig config;
    int pocketedBallCount = 0;
    bool ballsMoving = false;
    bool transitionPerspective = false;
    bool perspectiveRecorded = false;
    bool gameOver = false;
};

void initializeBalls(GameState& state);
void updateCameraFromCueBall(GameState& state);

}  // namespace billiardgl
