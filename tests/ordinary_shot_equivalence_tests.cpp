#include "game_runtime.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void appendFloat(std::ostringstream& output, float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    output << std::hex << std::setw(8) << std::setfill('0') << bits;
}

void appendDouble(std::ostringstream& output, double value)
{
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    output << std::hex << std::setw(16) << std::setfill('0') << bits;
}

void appendPoint(std::ostringstream& output, const billiardgl::Point3& point)
{
    appendFloat(output, point.x);
    appendFloat(output, point.y);
    appendFloat(output, point.z);
}

std::string ballBytes(const billiardgl::BallState& ball)
{
    std::ostringstream output;
    appendPoint(output, ball.position);
    appendPoint(output, ball.velocity);
    appendPoint(output, ball.angularVelocity);
    appendFloat(output, ball.speed);
    output << (ball.pocketed ? "01" : "00")
           << std::hex << std::setw(8) << std::setfill('0')
           << static_cast<int>(ball.motionState)
           << std::setw(8) << ball.pocketInteraction.pocketId
           << std::setw(8) << static_cast<int>(ball.pocketInteraction.phase)
           << std::setw(16) << ball.pocketInteraction.captureSequence;
    return output.str();
}

std::string sampleBytes(const billiardgl::PhysicsBallSample& ball)
{
    std::ostringstream output;
    appendPoint(output, ball.position);
    appendPoint(output, ball.velocity);
    appendPoint(output, ball.acceleration);
    appendPoint(output, ball.angularVelocity);
    appendFloat(output, ball.speed);
    appendFloat(output, ball.contactSlipSpeedCmS);
    appendDouble(output, ball.rotationalKineticEnergyJ);
    output << (ball.pocketed ? "01" : "00")
           << std::hex << std::setw(8) << std::setfill('0') << ball.pocketId
           << std::setw(8) << static_cast<int>(ball.pocketPhase)
           << std::setw(16) << ball.pocketCaptureSequence
           << std::setw(8) << static_cast<int>(ball.motionState);
    return output.str();
}

std::string contactBytes(const billiardgl::CueContactResult& contact)
{
    std::ostringstream output;
    appendDouble(output, contact.frictionCoefficient);
    appendDouble(output, contact.normalImpulseNs);
    appendDouble(output, contact.tangentialImpulseNs);
    appendDouble(output, contact.normalRelativeSpeedBeforeMS);
    appendDouble(output, contact.tangentialRelativeSpeedBeforeMS);
    appendDouble(output, contact.inputKineticEnergyJ);
    appendDouble(output, contact.outputKineticEnergyJ);
    const std::array<std::array<double, 3>, 12> vectors{{
        contact.tangentialRelativeVelocityBeforeMS, contact.contactArmM,
        contact.contactNormal, contact.impulseNs,
        contact.cueVelocityBeforeMS, contact.cueVelocityAfterMS,
        contact.ballVelocityBeforeMS, contact.ballVelocityAfterMS,
        contact.ballAngularVelocityBeforeRadS,
        contact.ballAngularVelocityAfterRadS,
        {{0.0, 0.0, 0.0}}, {{0.0, 0.0, 0.0}}
    }};
    for (const auto& vector : vectors) {
        for (double value : vector) appendDouble(output, value);
    }
    return output.str();
}

void appendBallArray(std::ostringstream& output,
    const std::array<billiardgl::BallState, billiardgl::kBallCount>& balls)
{
    output << '[';
    for (int index = 0; index < billiardgl::kBallCount; ++index) {
        if (index) output << ',';
        output << '"' << ballBytes(balls[index]) << '"';
    }
    output << ']';
}

struct Case {
    const char* name;
    double side;
    double vertical;
    const char* chalk;
    double initialVelocityCmS;
};

billiardgl::CueImpactInput inputFor(const Case& value)
{
    billiardgl::CueImpactInput input;
    input.cueBallIndex = 0;
    input.cueSpeedCmS = 200.0;
    input.cueMassKg = 0.5;
    input.direction = {{1.0, 0.0, 0.0}};
    input.tipOffsetRadius = {{value.side, value.vertical}};
    input.tipOffsetCm = {{value.side * billiardgl::kBallRadius,
        value.vertical * billiardgl::kBallRadius}};
    input.chalkState = value.chalk;
    return input;
}

std::string corpus(bool enableFrozenRouting)
{
    const Case cases[] = {
        {"centered", 0.0, 0.0, "CHALKED", 0.0},
        {"side_spin", 0.2, 0.0, "CHALKED", 0.0},
        {"top_spin", 0.0, 0.2, "CHALKED", 0.0},
        {"bottom_spin", 0.0, -0.2, "CHALKED", 0.0},
        {"moving_ball", 0.1, 0.0, "CHALKED", 20.0},
        {"unchalked", 0.3, 0.0, "UNCHALKED", 0.0},
        {"miscue", 0.81, 0.0, "CHALKED", 0.0},
    };
    std::ostringstream output;
    output << "{\"schema_version\":1,\"cases\":[";
    bool firstCase = true;
    for (const Case& value : cases) {
        if (!firstCase) output << ',';
        firstCase = false;
        billiardgl::GameState state;
        billiardgl::initializeBalls(state);
        for (billiardgl::BallState& ball : state.balls) ball.pocketed = true;
        state.balls[0].pocketed = false;
        state.balls[0].position = billiardgl::Point3{};
        state.balls[0].velocity.x = static_cast<float>(value.initialVelocityCmS);
        state.balls[0].speed = static_cast<float>(value.initialVelocityCmS);
        state.balls[1].pocketed = false;
        state.balls[1].position.x = 2.0f * billiardgl::kBallRadius + 10.0f;
        const billiardgl::GameState pre = state;
        billiardgl::PhysicsProfile profile =
            billiardgl::defaultChinesePoolPhysicsProfile();
        profile.frozenCueContact.enabled = enableFrozenRouting;
        billiardgl::GameRuntime runtime;
        if (!runtime.replaceStateForScenario(state, profile).ok) return {};
        runtime.setPhysicsTraceEnabled(true);
        const billiardgl::ActionResult action =
            runtime.applyCueImpact(inputFor(value));
        const billiardgl::GameState post = runtime.state();
        if (!runtime.step(8).ok) return {};

        output << "{\"name\":\"" << value.name << "\",\"action_ok\":"
               << (action.ok ? "true" : "false")
               << ",\"action_error\":\"" << action.errorCode
               << "\",\"pre_ball_bytes\":";
        appendBallArray(output, pre.balls);
        output << ",\"post_ball_bytes\":";
        appendBallArray(output, post.balls);
        const billiardgl::CueContactResult& contact = runtime.cueContactResult();
        if (!contact.microsteps.empty()) return {};
        output << ",\"contact\":{\"regime\":"
               << static_cast<int>(contact.regime)
               << ",\"applied\":" << (contact.applied ? "true" : "false")
               << ",\"error\":\"" << contact.error
               << "\",\"numeric_bytes\":\"" << contactBytes(contact)
               << "\"},\"rule_flags\":{\"current_player\":"
               << post.players.currentPlayer << ",\"next_player\":"
               << post.players.nextPlayer << ",\"illegal_shot\":"
               << (post.players.illegalShot ? "true" : "false")
               << ",\"shot_taken\":"
               << (post.players.shotTaken ? "true" : "false")
               << ",\"updated_after_shot\":"
               << (post.players.updatedAfterShot ? "true" : "false")
               << "},\"frames\":[";
        bool firstFrame = true;
        for (const billiardgl::PhysicsFrame& frame :
             runtime.physicsTrace().frames()) {
            if (!firstFrame) output << ',';
            firstFrame = false;
            output << "{\"tick\":" << frame.tick << ",\"balls\":[";
            for (int index = 0; index < billiardgl::kBallCount; ++index) {
                if (index) output << ',';
                output << '"' << sampleBytes(frame.balls[index]) << '"';
            }
            output << "],\"contacts\":" << frame.contacts.size()
                   << ",\"solver_events\":" << frame.solverEvents.size()
                   << ",\"step_status\":"
                   << static_cast<int>(frame.stepStatus)
                   << ",\"failure_code\":"
                   << static_cast<int>(frame.failureCode) << '}';
        }
        output << "],\"events\":[";
        bool firstEvent = true;
        for (const billiardgl::RuntimeEvent& event : runtime.events()) {
            if (!firstEvent) output << ',';
            firstEvent = false;
            output << "{\"sequence\":" << event.sequence
                   << ",\"tick\":" << event.tick
                   << ",\"name\":\"" << event.name << "\"}";
        }
        output << "]}";
    }
    output << "]}\n";
    return output.str();
}

std::string readFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

}  // namespace

int main(int argc, char** argv)
{
    const std::string baselinePath = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/physics_models/regression/phase3_v4_ordinary_shot_baseline.json";
    if (argc == 2 && std::string(argv[1]) == "--write-v4") {
        std::ofstream output(baselinePath, std::ios::binary);
        output << corpus(false);
        return output ? 0 : 1;
    }
    const std::string expected = readFile(baselinePath);
    const std::string actual = corpus(true);
    if (expected.empty() || actual != expected) {
        std::cerr << "v5 ordinary-shot corpus differs from the frozen v4 baseline\n";
        return 1;
    }
    return 0;
}
