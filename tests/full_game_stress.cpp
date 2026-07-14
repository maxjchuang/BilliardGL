#include "game_runtime.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Result {
    int seed = 0;
    int cadence = 0;
    int loaded = 0;
    int repeats = 0;
    bool finite = true;
    double maximumPenetration = 0.0;
    double maximumResidual = 0.0;
    std::uint64_t maximumPenetrationTick = 0;
    std::string maximumPenetrationContacts;
    int duplicateContacts = 0;
    std::uint64_t hash = 1469598103934665603ULL;
};

void mix(Result& result, const void* data, std::size_t size)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) {
        result.hash ^= bytes[index];
        result.hash *= 1099511628211ULL;
    }
}

Result run(int seed, int cadence, bool loaded)
{
    Result result;
    result.seed = seed;
    result.cadence = cadence;
    result.loaded = loaded ? 1 : 0;
    result.repeats = 3;
    for (int repeat = 0; repeat < result.repeats; ++repeat) {
        billiardgl::GameRuntime runtime;
        // A real user cannot strike before the reset rack has observed an idle
        // frame; let zero-time contact projection settle the tight rack first.
        runtime.step(3);
        runtime.setPhysicsTraceEnabled(true);
        billiardgl::CueImpactInput input;
        input.cueBallIndex = 0;
        input.cueSpeedCmS = 90.0 + (seed % 7) * 5.0 + repeat * 4.0;
        input.cueMassKg = runtime.physicsProfile().cue.effectiveMassKg;
        const double angle = (seed % 9 - 4) * 0.0125;
        input.direction = {{std::sin(angle), 0.0, std::cos(angle)}};
        input.tipOffsetRadius = {{0.0, 0.0}};
        input.tipOffsetCm = {{input.tipOffsetRadius[0] * 2.8575,
                              input.tipOffsetRadius[1] * 2.8575}};
        input.chalkState = "chalked";
        if (!runtime.applyCueImpact(input).ok) {
            result.finite = false;
            continue;
        }
        int remaining = 240;
        while (remaining > 0) {
            const int count = std::min(cadence, remaining);
            runtime.step(count);
            remaining -= count;
            if (loaded) {
                volatile std::uint64_t load = 0;
                for (int index = 0; index < 2000; ++index) load += index * 2654435761U;
                (void)load;
            }
        }
        for (const billiardgl::PhysicsFrame& frame : runtime.physicsTrace().frames()) {
            if (frame.maximumPenetrationCm > result.maximumPenetration) {
                result.maximumPenetration = frame.maximumPenetrationCm;
                result.maximumPenetrationTick = frame.tick;
                std::ostringstream contacts;
                for (const billiardgl::PhysicsContactRecord& contact : frame.contacts) {
                    if (contact.penetrationCm > 0.0) {
                        contacts << contact.firstBall << '-' << contact.secondBall
                                 << ':' << contact.penetrationCm
                                 << '@' << contact.solverEventId << ';';
                    }
                }
                result.maximumPenetrationContacts = contacts.str();
            }
            std::set<std::string> keys;
            for (const billiardgl::PhysicsContactRecord& contact : frame.contacts) {
                if (!contact.velocityImpulseApplied) continue;
                std::ostringstream key;
                key << contact.solverEventId << ':' << contact.solverIslandId << ':'
                    << contact.firstBall << ':' << contact.secondBall;
                if (!keys.insert(key.str()).second) ++result.duplicateContacts;
            }
            for (const billiardgl::SolverEventRecord& event : frame.solverEvents) {
                result.maximumResidual = std::max(
                    result.maximumResidual, event.maximumResidualCmS);
            }
        }
        for (const billiardgl::BallState& ball : runtime.state().balls) {
            const float values[] = {ball.position.x, ball.position.y, ball.position.z,
                                    ball.velocity.x, ball.velocity.y, ball.velocity.z,
                                    ball.angularVelocity.x, ball.angularVelocity.y,
                                    ball.angularVelocity.z};
            for (float value : values) {
                result.finite = result.finite && std::isfinite(value);
                mix(result, &value, sizeof(value));
            }
            mix(result, &ball.pocketed, sizeof(ball.pocketed));
        }
    }
    return result;
}

std::string hashText(std::uint64_t value)
{
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

}  // namespace

int main(int argc, char** argv)
{
    std::vector<Result> results;
    for (int seed : {101, 211, 307}) {
        for (int cadence : {1, 4}) {
            for (int loaded : {0, 1}) results.push_back(run(seed, cadence, loaded != 0));
        }
    }
    std::map<int, std::string> hashes;
    for (const Result& result : results) {
        const std::string hash = hashText(result.hash);
        if (!result.finite || result.maximumPenetration > 0.5 + 1e-9 ||
            result.maximumResidual > 0.001 + 1e-9 || result.duplicateContacts != 0) {
            std::cerr << "full-game stress invariant failed for seed " << result.seed
                      << " finite=" << result.finite
                      << " penetration=" << result.maximumPenetration
                      << " tick=" << result.maximumPenetrationTick
                      << " contacts=" << result.maximumPenetrationContacts
                      << " residual=" << result.maximumResidual
                      << " duplicates=" << result.duplicateContacts << '\n';
            return EXIT_FAILURE;
        }
        if (!hashes.insert(std::make_pair(result.seed, hash)).second &&
            hashes[result.seed] != hash) {
            std::cerr << "cadence or load changed replay hash for seed " << result.seed << '\n';
            return EXIT_FAILURE;
        }
    }
    if (argc == 3 && std::string(argv[1]) == "--write") {
        std::ofstream output(argv[2]);
        if (!output) return EXIT_FAILURE;
        output << "seed,cadence_ticks,host_load,repeated_breaks,finite_state,maximum_penetration_cm,maximum_residual_cm_s,duplicate_contacts,replay_hash\n";
        output << std::setprecision(17);
        for (const Result& result : results) {
            output << result.seed << ',' << result.cadence << ',' << result.loaded << ','
                   << result.repeats << ',' << (result.finite ? "true" : "false") << ','
                   << result.maximumPenetration << ',' << result.maximumResidual << ','
                   << result.duplicateContacts << ',' << hashText(result.hash) << '\n';
        }
    }
    return EXIT_SUCCESS;
}
