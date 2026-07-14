#pragma once

#include "game_runtime.h"

#include <cstdint>
#include <string>
#include <vector>

namespace fullgame {

struct FullGameRunOptions {
    int cadenceTicks = 1;
    bool hostLoad = false;
    int repeats = 3;
    int ticksPerRepeat = 240;
};

struct FullGameCaseResult {
    bool passed = false;
    std::string failure;
    std::uint32_t seed = 0;
    double maximumPenetrationCm = 0.0;
    double maximumResidualCmS = 0.0;
    int duplicateContacts = 0;
    std::string deterministicHash;
    std::string caseId;
    std::vector<billiardgl::PhysicsFrame> frames;
    std::vector<billiardgl::RuntimeEvent> events;
    std::size_t droppedTraceFrames = 0;
    int stepFailures = 0;
    double wallSeconds = 0.0;
    std::uint64_t peakRssBytes = 0;
    int surfaceTransitions = 0;
    int ballCollisions = 0;
    int railCollisions = 0;
    int objectBallCaptures = 0;
    int cueBallCaptures = 0;
    bool cueContactApplied = false;
    bool cueContactMiscue = false;
    int scoreEvents = 0;
    int foulEvents = 0;
    int turnTransfers = 0;
    int illegalActionAttempts = 0;
    int completedShots = 0;
    int declaredShots = 0;
    bool gameOver = false;
    int stateMismatches = 0;
    int eventMismatches = 0;
};

typedef FullGameCaseResult (*FullGameCaseRun)(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);

struct FullGameCase {
    std::string id;
    FullGameCaseRun run = nullptr;
};

const std::vector<FullGameCase>& fullGameCases();
std::vector<std::string> fullGameCaseIds();
const FullGameCase* findFullGameCase(const std::string& id);

}  // namespace fullgame
