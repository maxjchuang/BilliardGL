#include "full_game_case_registry.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <set>
#include <sstream>

namespace fullgame {
namespace {

void mix(std::uint64_t& hash, const void* data, std::size_t size)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
}

std::string hashText(std::uint64_t value)
{
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

FullGameCaseResult runLegacyBreakStress(billiardgl::GameRuntime& runtime,
    std::uint32_t seed, const FullGameRunOptions& options)
{
    FullGameCaseResult result;
    result.passed = true;
    result.seed = seed;
    std::uint64_t hash = 1469598103934665603ULL;
    for (int repeat = 0; repeat < options.repeats; ++repeat) {
        runtime.reset();
        if (!runtime.step(3).ok) {
            result.passed = false;
            result.failure = "rack_settle_failed";
            break;
        }
        runtime.setPhysicsTraceEnabled(true);
        billiardgl::CueImpactInput input;
        input.cueBallIndex = 0;
        input.cueSpeedCmS = 90.0 + (seed % 7) * 5.0 + repeat * 4.0;
        input.cueMassKg = runtime.physicsProfile().cue.effectiveMassKg;
        const double angle = (static_cast<int>(seed % 9) - 4) * 0.0125;
        input.direction = {{std::sin(angle), 0.0, std::cos(angle)}};
        input.tipOffsetRadius = {{0.0, 0.0}};
        input.tipOffsetCm = {{0.0, 0.0}};
        input.chalkState = "chalked";
        if (!runtime.applyCueImpact(input).ok) {
            result.passed = false;
            result.failure = "cue_impact_failed";
            break;
        }
        int remaining = options.ticksPerRepeat;
        while (remaining > 0) {
            const int count = std::min(options.cadenceTicks, remaining);
            const billiardgl::ActionResult stepped = runtime.step(count);
            if (!stepped.ok) {
                result.passed = false;
                result.failure = stepped.errorCode;
                break;
            }
            remaining -= count;
            if (options.hostLoad) {
                volatile std::uint64_t load = 0;
                for (int index = 0; index < 2000; ++index)
                    load += index * 2654435761U;
                (void)load;
            }
        }
        if (!result.passed) break;
        for (const billiardgl::PhysicsFrame& frame :
                runtime.physicsTrace().frames()) {
            result.maximumPenetrationCm = std::max(
                result.maximumPenetrationCm, frame.maximumPenetrationCm);
            std::set<std::string> contactKeys;
            for (const billiardgl::PhysicsContactRecord& contact :
                    frame.contacts) {
                if (!contact.velocityImpulseApplied) continue;
                std::ostringstream key;
                key << contact.solverEventId << ':' << contact.solverIslandId
                    << ':' << contact.firstBall << ':' << contact.secondBall
                    << ':' << static_cast<int>(contact.kind);
                if (!contactKeys.insert(key.str()).second)
                    ++result.duplicateContacts;
            }
            for (const billiardgl::SolverEventRecord& event :
                    frame.solverEvents) {
                result.maximumResidualCmS = std::max(
                    result.maximumResidualCmS, event.maximumResidualCmS);
            }
        }
        for (const billiardgl::BallState& ball : runtime.state().balls) {
            const float values[] = {
                ball.position.x, ball.position.y, ball.position.z,
                ball.velocity.x, ball.velocity.y, ball.velocity.z,
                ball.angularVelocity.x, ball.angularVelocity.y,
                ball.angularVelocity.z};
            for (float value : values) {
                if (!std::isfinite(value)) result.passed = false;
                mix(hash, &value, sizeof(value));
            }
            mix(hash, &ball.pocketed, sizeof(ball.pocketed));
        }
    }
    if (result.maximumPenetrationCm > 0.5 + 1e-9 ||
        result.maximumResidualCmS > 0.001 + 1e-9 ||
        result.duplicateContacts != 0) {
        result.passed = false;
        result.failure = "physics_invariant_failed";
    }
    result.deterministicHash = hashText(hash);
    return result;
}

}  // namespace

const std::vector<FullGameCase>& fullGameCases()
{
    static const std::vector<FullGameCase> cases = {
        {"legacy_break_stress", runLegacyBreakStress},
    };
    return cases;
}

std::vector<std::string> fullGameCaseIds()
{
    std::vector<std::string> result;
    for (const FullGameCase& item : fullGameCases()) result.push_back(item.id);
    std::sort(result.begin(), result.end());
    return result;
}

const FullGameCase* findFullGameCase(const std::string& id)
{
    for (const FullGameCase& item : fullGameCases()) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

}  // namespace fullgame
