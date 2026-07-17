#include "full_game_case_registry.h"
#include "full_game_cli.h"
#include "full_game_artifacts.h"
#include "full_game_invariants.h"
#include "full_game_test_profile.h"
#include "automation_json.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <sys/resource.h>
#include <vector>

namespace {

std::uint64_t peakRssBytes()
{
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#if defined(__APPLE__)
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open matrix file");
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

void atomicWrite(const std::filesystem::path& path, const std::string& bytes)
{
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot open matrix output");
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) throw std::runtime_error("cannot flush matrix output");
    }
    std::filesystem::rename(temporary, path);
}

fullgame::FullGameCaseResult runCase(const fullgame::FullGameCase& selected,
    const std::string& caseId, std::uint32_t seed)
{
    billiardgl::GameRuntime runtime = fullgame::phase3V5CandidateRuntime();
    const auto started = std::chrono::steady_clock::now();
    fullgame::FullGameCaseResult result = selected.run(
        runtime, seed, fullgame::FullGameRunOptions{});
    result.caseId = caseId;
    result.wallSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    result.peakRssBytes = peakRssBytes();
    const fullgame::FullGameInvariantResult invariants =
        fullgame::evaluateFullGameInvariants(result.frames, result.events,
            result.droppedTraceFrames, result.wallSeconds,
            result.peakRssBytes);
    result.maximumPenetrationCm = invariants.maximumPenetrationCm;
    result.maximumResidualCmS = invariants.maximumResidualCmS;
    result.duplicateContacts = invariants.duplicateContacts;
    result.stepFailures = invariants.stepFailures;
    result.deterministicHash = invariants.deterministicHash;
    if (!invariants.passed) {
        result.passed = false;
        if (result.failure.empty() && !invariants.failures.empty())
            result.failure = invariants.failures.front();
    }
    return result;
}

struct MatrixEntry {
    std::string id;
    std::uint32_t seed = 0;
    const fullgame::FullGameCase* selected = nullptr;
};

std::vector<MatrixEntry> parseMatrix(const std::filesystem::path& path)
{
    const billiardgl::json::ParseResult parsed =
        billiardgl::json::parse(readFile(path));
    if (!parsed.ok || !parsed.value.isObject() ||
        !parsed.value.has("schema_version") ||
        parsed.value.at("schema_version").asInt() != 2 ||
        !parsed.value.has("cases") || !parsed.value.at("cases").isArray())
        throw std::runtime_error("matrix must use schema version 2");
    std::vector<MatrixEntry> entries;
    std::set<std::string> ids;
    for (const billiardgl::json::Value& item :
            parsed.value.at("cases").asArray()) {
        if (!item.isObject() || !item.has("id") || !item.has("seed") ||
            !item.at("id").isString() || !item.at("seed").isNumber())
            throw std::runtime_error("matrix case requires id and seed");
        MatrixEntry entry;
        entry.id = item.at("id").asString();
        const double seed = item.at("seed").asNumber();
        if (seed < 0.0 || seed > std::numeric_limits<std::uint32_t>::max() ||
            std::floor(seed) != seed)
            throw std::runtime_error("matrix seed must be uint32");
        entry.seed = static_cast<std::uint32_t>(seed);
        if (!ids.insert(entry.id).second)
            throw std::runtime_error("matrix case IDs must be unique");
        entry.selected = fullgame::findFullGameCase(entry.id);
        if (entry.selected == nullptr || entry.selected->run == nullptr)
            throw std::runtime_error("matrix contains unknown case: " + entry.id);
        entries.push_back(entry);
    }
    if (entries.empty()) throw std::runtime_error("matrix has no cases");
    return entries;
}

int executeMatrix(const fullgame::FullGameCommand& command)
{
    std::vector<MatrixEntry> entries;
    try {
        entries = parseMatrix(command.matrixPath);
        std::filesystem::create_directories(command.writeDirectory);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    billiardgl::json::Value cases = billiardgl::json::Value::array();
    std::string index = "case_id,seed,passed,frame_count,wall_seconds,peak_rss_bytes,deterministic_hash\n";
    bool passed = true;
    std::string failure;
    for (const MatrixEntry& entry : entries) {
        fullgame::FullGameCaseResult result =
            runCase(*entry.selected, entry.id, entry.seed);
        try {
            fullgame::writeFullGameArtifacts(
                result, command.writeDirectory / entry.id);
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return EXIT_FAILURE;
        }
        billiardgl::json::Value item = billiardgl::json::Value::object();
        item["case_id"] = billiardgl::json::Value(result.caseId);
        item["deterministic_hash"] =
            billiardgl::json::Value(result.deterministicHash);
        item["failure"] = billiardgl::json::Value(result.failure);
        item["frame_count"] = billiardgl::json::Value(
            static_cast<double>(result.frames.size()));
        item["passed"] = billiardgl::json::Value(result.passed);
        item["peak_rss_bytes"] = billiardgl::json::Value(
            static_cast<double>(result.peakRssBytes));
        item["seed"] = billiardgl::json::Value(
            static_cast<double>(result.seed));
        item["wall_seconds"] = billiardgl::json::Value(result.wallSeconds);
        cases.asArray().push_back(item);
        index += result.caseId + "," + std::to_string(result.seed) + "," +
            (result.passed ? "true" : "false") + "," +
            std::to_string(result.frames.size()) + "," +
            std::to_string(result.wallSeconds) + "," +
            std::to_string(result.peakRssBytes) + "," +
            result.deterministicHash + "\n";
        if (!result.passed) {
            passed = false;
            failure = result.caseId + ": " + result.failure;
            break;
        }
    }
    billiardgl::json::Value summary = billiardgl::json::Value::object();
    summary["cases"] = cases;
    summary["matrix_path"] =
        billiardgl::json::Value(command.matrixPath.string());
    summary["passed"] = billiardgl::json::Value(passed);
    summary["schema_version"] = billiardgl::json::Value(2);
    try {
        atomicWrite(command.writeDirectory / "matrix_summary.json",
            billiardgl::json::stringify(summary) + "\n");
        atomicWrite(command.writeDirectory / "index.csv", index);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    if (!passed) {
        std::cerr << failure << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv)
{
    fullgame::FullGameCommand command;
    try {
        command = fullgame::parseFullGameCommand(argc,
            const_cast<const char* const*>(argv));
    } catch (const std::invalid_argument& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    if (command.mode == fullgame::FullGameMode::ListCases) {
        for (const std::string& id : fullgame::fullGameCaseIds())
            std::cout << id << '\n';
        return EXIT_SUCCESS;
    }
    if (command.mode == fullgame::FullGameMode::Matrix) {
        return executeMatrix(command);
    }
    const fullgame::FullGameCase* selected =
        fullgame::findFullGameCase(command.caseId);
    if (selected == nullptr || selected->run == nullptr) return EXIT_FAILURE;
    fullgame::FullGameCaseResult result =
        runCase(*selected, command.caseId, command.seed);
    if (!command.writeDirectory.empty()) {
        try {
            fullgame::writeFullGameArtifacts(result, command.writeDirectory);
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    if (!result.passed) {
        std::cerr << result.failure << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
