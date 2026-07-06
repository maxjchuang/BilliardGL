#pragma once

#include <array>
#include <string>

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
    float previousAngleX = 0.0f;
    float previousAngleY = 0.0f;
    float previousTargetX = 0.0f;
    float previousTargetY = 0.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    float panZ = 0.0f;
    float recordedZoom = 0.0f;
    float recordedTarget[3] = {0.0f, 0.0f, 0.0f};
};

enum class AimMode {
    Observe,
    Aim
};

struct AimState {
    AimMode mode = AimMode::Observe;
    float yaw = kPi / 2.0f;
    float sensitivity = 0.01f;
};

struct PlayerState {
    int assignedBallType[2] = {-1, -1};
    bool firstPocketedObjectBall = true;
    int currentPlayer = 0;
    int nextPlayer = 0;
    bool illegalShot = false;
    bool updatedAfterShot = false;
    bool shotTaken = false;
    bool aimingAtCueBall = false;
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

enum class ScreenshotScene {
    Default,
    Help,
    Aim,
    AfterShot
};

struct RuntimeConfig {
    bool windowedMode = true;
    int width = 1024;
    int height = 768;
    std::string screenshotPath;
    ScreenshotScene screenshotScene = ScreenshotScene::Default;
};

struct GameplayEvents {
    bool ballCollision = false;
    bool railCollision = false;
    bool ballPocketed = false;
    bool cueBallPocketed = false;
    bool eightBallPocketed = false;
    bool shotEnded = false;
};

struct LegacyBallAdapter {
    Point3 position;
    Point3 velocity;
    Point3 rotationAxis;
    float speed = 0.0f;
    float rotationAngle = 0.0f;
    bool pocketed = false;
    unsigned int texture = 0;
};

struct GameState {
    std::array<BallState, kBallCount> balls;
    CameraState camera;
    AimState aim;
    PlayerState players;
    InputState input;
    HudState hud;
    RuntimeConfig config;
    GameplayEvents events;
    int pocketedBallCount = 0;
    bool ballsMoving = false;
    bool transitionPerspective = false;
    bool perspectiveRecorded = false;
    bool gameOver = false;
};

void initializeBalls(GameState& state);
void updateCameraFromCueBall(GameState& state);
void resetBallMotion(BallState& ball);
void setBallVelocity(BallState& ball, float x, float y, float z);
bool anyBallMoving(const GameState& state);
void clearGameplayEvents(GameState& state);
void copyBallStateToLegacy(const GameState& state, std::array<LegacyBallAdapter, kBallCount>& legacyBalls);
void copyLegacyTexturesToState(const std::array<LegacyBallAdapter, kBallCount>& legacyBalls, GameState& state);

}  // namespace billiardgl
