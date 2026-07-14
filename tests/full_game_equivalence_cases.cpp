#include "full_game_equivalence_cases.h"

#include "automation_json.h"
#include "automation_protocol.h"
#include "full_game_invariants.h"
#include "full_game_prng.h"

#include <algorithm>
#include <string>
#include <vector>

namespace fullgame {
namespace {

struct Replay {
    bool passed = false;
    std::string failure;
    std::vector<billiardgl::PhysicsFrame> frames;
    std::vector<billiardgl::RuntimeEvent> events;
    std::size_t droppedFrames = 0;
};

billiardgl::ActionResult control(billiardgl::GameRuntime& runtime,
    billiardgl::ActionType type, float value = 0.0f)
{
    billiardgl::GameAction action;
    action.type = type;
    action.first = value;
    return runtime.dispatch(action);
}

Replay replay(std::uint32_t seed, int cadence, bool hostLoad, int ticks)
{
    Replay replayResult;
    billiardgl::GameRuntime runtime;
    if (!runtime.step(3).ok) {
        replayResult.failure = "rack_settle_failed";
        return replayResult;
    }
    runtime.setPhysicsTraceEnabled(true);
    XorShift32 random(seed);
    const float yaw = billiardgl::kPi * 0.5f +
        (random.unit() - 0.5f) * 0.015f;
    const float power = 90.0f + random.unit() * 10.0f;
    billiardgl::ActionResult action = control(
        runtime, billiardgl::ActionType::SetAimYaw, yaw);
    if (action.ok) action = control(
        runtime, billiardgl::ActionType::SetShotPower, power);
    if (action.ok) action = control(runtime, billiardgl::ActionType::Shoot);
    if (!action.ok) {
        replayResult.failure = action.errorCode;
        return replayResult;
    }
    int remaining = ticks;
    while (remaining > 0) {
        const int batch = std::min(cadence, remaining);
        const billiardgl::ActionResult stepped = runtime.step(batch);
        if (!stepped.ok) {
            replayResult.failure = stepped.errorCode;
            return replayResult;
        }
        remaining -= batch;
        if (hostLoad) {
            volatile std::uint32_t accumulator = seed;
            for (std::uint32_t index = 1; index <= 10000; ++index)
                accumulator = accumulator * 1664525u + index + 1013904223u;
            (void)accumulator;
        }
    }
    replayResult.frames.assign(runtime.physicsTrace().frames().begin(),
        runtime.physicsTrace().frames().end());
    replayResult.events = runtime.events();
    replayResult.droppedFrames = runtime.physicsTrace().droppedFrames();
    replayResult.passed = true;
    return replayResult;
}

std::string frameState(const billiardgl::PhysicsFrame& frame)
{
    return billiardgl::json::stringify(
        billiardgl::serializePhysicsFrame(frame));
}

std::string eventState(const billiardgl::RuntimeEvent& event)
{
    return std::to_string(event.tick) + ":" +
        std::to_string(event.sequence) + ":" + event.name;
}

FullGameCaseResult compare(const std::string& caseId, std::uint32_t seed,
    const Replay& first, const Replay& second)
{
    FullGameCaseResult result;
    result.caseId = caseId;
    result.seed = seed;
    if (!first.passed || !second.passed) {
        result.failure = first.passed ? second.failure : first.failure;
        return result;
    }
    result.frames = first.frames;
    result.events = first.events;
    result.droppedTraceFrames = first.droppedFrames;
    if (first.frames.size() != second.frames.size()) {
        ++result.stateMismatches;
    } else {
        for (std::size_t index = 0; index < first.frames.size(); ++index) {
            if (first.frames[index].tick != second.frames[index].tick ||
                frameState(first.frames[index]) != frameState(second.frames[index]))
                ++result.stateMismatches;
        }
    }
    if (first.events.size() != second.events.size()) {
        ++result.eventMismatches;
    } else {
        for (std::size_t index = 0; index < first.events.size(); ++index) {
            if (eventState(first.events[index]) != eventState(second.events[index]))
                ++result.eventMismatches;
        }
    }
    const FullGameInvariantResult invariants = evaluateFullGameInvariants(
        result.frames, result.events, result.droppedTraceFrames, 0.0, 0);
    result.maximumPenetrationCm = invariants.maximumPenetrationCm;
    result.maximumResidualCmS = invariants.maximumResidualCmS;
    result.duplicateContacts = invariants.duplicateContacts;
    result.stepFailures = invariants.stepFailures;
    result.deterministicHash = invariants.deterministicHash;
    result.passed = invariants.passed && result.stateMismatches == 0 &&
        result.eventMismatches == 0;
    if (!result.passed) {
        if (!invariants.failures.empty()) result.failure = invariants.failures.front();
        else result.failure = "common_tick_equivalence_failed";
    }
    return result;
}

}  // namespace

FullGameCaseResult runCadenceEquivalence(billiardgl::GameRuntime&,
    std::uint32_t seed, const FullGameRunOptions& options)
{
    return compare("cadence_equivalence", seed,
        replay(seed, 1, false, options.ticksPerRepeat),
        replay(seed, 4, false, options.ticksPerRepeat));
}

FullGameCaseResult runHostLoadEquivalence(billiardgl::GameRuntime&,
    std::uint32_t seed, const FullGameRunOptions& options)
{
    return compare("host_load_equivalence", seed,
        replay(seed, 1, false, options.ticksPerRepeat),
        replay(seed, 1, true, options.ticksPerRepeat));
}

}  // namespace fullgame
