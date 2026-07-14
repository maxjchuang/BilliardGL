#include "automation_json.h"
#include "full_game_artifacts.h"
#include "full_game_invariants.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

std::string read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

}  // namespace

int main()
{
    try {
        expect(fullgame::sha256Hex("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "SHA-256 empty vector");
        expect(fullgame::sha256Hex("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "SHA-256 abc vector");
        fullgame::FullGameCaseResult fixture;
        fixture.passed = true;
        fixture.caseId = "fixture";
        fixture.seed = 42;
        billiardgl::PhysicsFrame frame;
        frame.tick = 1;
        frame.timeSeconds = 0.1;
        frame.deltaSeconds = 0.1f;
        frame.physicsProfileId = "fixture_profile";
        fixture.frames.push_back(frame);
        fixture.events.emplace_back(1, 1, "shot_ended");
        const fullgame::FullGameInvariantResult invariants =
            fullgame::evaluateFullGameInvariants(
                fixture.frames, fixture.events, 0, 0.01, 1024);
        expect(invariants.passed, "fixture invariants pass");
        fixture.deterministicHash = invariants.deterministicHash;

        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            "billiardgl-full-game-artifact-test";
        std::filesystem::remove_all(directory);
        fullgame::writeFullGameArtifacts(fixture, directory);
        const billiardgl::json::ParseResult summary =
            billiardgl::json::parse(read(directory / "summary.json"));
        const billiardgl::json::ParseResult trace =
            billiardgl::json::parse(read(directory / "trace.json"));
        expect(summary.ok && trace.ok, "artifacts are valid JSON");
        expect(summary.value.at("frame_count").asInt() ==
            static_cast<int>(trace.value.at("frames").asArray().size()),
            "summary binds all frames");
        expect(summary.value.at("dropped_trace_frames").asInt() == 0,
            "no dropped frames");
        expect(summary.value.at("step_failures").asInt() == 0,
            "no failed steps");
        std::string traceBytes = read(directory / "trace.json");
        if (!traceBytes.empty() && traceBytes.back() == '\n') traceBytes.pop_back();
        expect(summary.value.at("deterministic_hash").asString() ==
            fullgame::sha256Hex(traceBytes),
            "summary hash binds canonical state");
        expect(trace.value.at("frames").asArray()[0].at("balls").asArray()[0]
            .has("pocket_phase"), "trace contains pocket state");
        std::filesystem::remove_all(directory);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
