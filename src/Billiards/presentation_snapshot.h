#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl {

constexpr int kPresentationBallCount = 16;

struct PresentationPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct PresentationBall {
    int identity = 0;
    PresentationPoint position;
    PresentationPoint rotationAxis;
    float rotationAngleDegrees = 0.0f;
    unsigned int legacyTexture = 0;
    bool visible = false;
    bool castsShadow = false;
    bool pocketed = false;
};

struct CuePresentation {
    PresentationPoint referencePosition;
    PresentationPoint direction;
    PresentationPoint guideStart;
    PresentationPoint guideEnd;
    float rotationDegrees = 0.0f;
    float modelOriginBackoffCm = 0.0f;
    float shotPower = 0.0f;
    float tipOffsetX = 0.0f;
    float tipOffsetY = 0.0f;
    float elevationDegrees = 0.0f;
    bool visible = false;
    bool showGuideLine = false;
};

struct CameraPresentation {
    PresentationPoint eye;
    PresentationPoint target;
    PresentationPoint projectionPan;
    float zoom = 0.0f;
    float fieldOfViewDegrees = 48.0f;
    std::uint32_t semanticState = 0;
    std::uint32_t preset = 0;
    bool transitioning = false;
    int viewportWidth = 1;
    int viewportHeight = 1;
    bool followsCueBall = false;
};

struct HudViewModel {
    int currentPlayerNumber = 1;
    float shotPower = 0.0f;
    float tipOffsetX = 0.0f;
    float tipOffsetY = 0.0f;
    std::uint32_t assistLevel = 1;
    bool showCueElevation = false;
    bool showChalkState = false;
    int viewportWidth = 1;
    int viewportHeight = 1;
    bool aimMode = false;
    bool followCamera = false;
    bool showHelp = false;
    bool showPowerMeter = false;
};

struct ShotControlPresentation {
    std::uint32_t schemaVersion = 1;
    std::uint32_t powerMode = 0;
    std::uint32_t assistLevel = 1;
    float yawRadians = 1.5707963f;
    float tipOffsetX = 0.0f;
    float tipOffsetY = 0.0f;
    float requestedPower = 60.0f;
    float strokeSpeedCmS = 0.0f;
    float cueElevationDegrees = 0.0f;
    std::uint32_t chalkIntent = 0;
};

struct TransientVisualEvents {
    bool ballCollision = false;
    bool railCollision = false;
    bool ballPocketed = false;
    bool cueBallPocketed = false;
    bool eightBallPocketed = false;
    bool shotEnded = false;
};

struct RulesPresentation {
    bool enabled = false;
    std::string rulesetId = "legacy";
    std::string phase = "break";
    int currentPlayer = 0;
    std::array<std::string, 2> groups{{"open", "open"}};
    bool ballInHand = false;
    bool hasAdjudication = false;
    bool foul = false;
    bool continueTurn = false;
    bool gameOver = false;
    int winner = -1;
    // Rack-only adjudication must not manufacture a match/game result.
    bool rackEnded = false;
    int rackWinner = -1;
    std::vector<std::string> reasonCodes;
    std::vector<std::string> textKeys;
    std::vector<std::string> decisionOptions;
};

struct PresentationSnapshot {
    std::array<PresentationBall, kPresentationBallCount> balls;
    CuePresentation cue;
    ShotControlPresentation shotControl;
    CameraPresentation camera;
    HudViewModel hud;
    RulesPresentation rules;
    TransientVisualEvents events;
    bool ballsMoving = false;
    bool gameOver = false;
};

}  // namespace billiardgl
