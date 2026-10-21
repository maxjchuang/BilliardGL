#pragma once

#include "game_state.h"
#include "physics_telemetry.h"
#include "rules/chinese_eight_ball_rules.h"

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace billiardgl::rules {

struct BehindHeadStringFirstContactContext {
    bool active = false;
    int actorPlayerId = -1;
    std::string placementDecisionId;
    std::uint64_t placementAckRevision = 0;
    double headStringWorldZCm = 0.0;
    std::string observationId;
};

// Per-shot adapter from authoritative physics facts to rules facts. It does
// not decide legality and never mutates either GameState.
class ShotFactCollector {
public:
    void begin(const GameState& state, int shooter, bool breakShot,
        const BehindHeadStringFirstContactContext* headStringContext = nullptr);
    void consume(const GameState& before, const GameState& after,
                 const PhysicsStepTelemetry& telemetry,
                 std::uint64_t physicsTick);
    bool bindCanonicalDoubleHitClassification(
        const CanonicalDoubleHitClassification& classification);
    bool bindCanonicalPushClassification(
        const CanonicalPushClassification& classification);
    bool bindCanonicalFrozenCushionClassification(
        const CanonicalFrozenCushionClassification& classification);
    ShotFacts finish();
    bool active() const noexcept { return active_; }

private:
    bool active_ = false;
    bool firstContactObserved_ = false;
    ShotFacts facts_;
    std::set<int> objectBallsToRail_;
    std::vector<BreakObjectCushionContact> breakObjectCushionContacts_;
    std::vector<CueBallPocketCaptureEvidence> cueBallPocketCaptures_;
    std::array<bool, kBallCount> pocketRecorded_{};
    std::uint64_t lastSucceededPhysicsTick_ = 0;
    bool breakEvidenceComplete_ = false;
    bool cueBallScratchEvidenceComplete_ = false;
    BehindHeadStringFirstContactContext headStringContext_;
    std::vector<CueCenterPlaneCrossingEvidence> crossingEvents_;
    std::vector<FirstCueObjectContactEvidence> firstContactEvidence_;
    bool headStringEvidenceComplete_ = false;
    std::map<std::uint64_t, std::string> consumedTickDigests_;
};

}  // namespace billiardgl::rules
