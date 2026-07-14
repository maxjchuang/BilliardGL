#include "full_game_artifacts.h"

#include "automation_json.h"
#include "full_game_invariants.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fullgame {
namespace {

void atomicWrite(const std::filesystem::path& path, const std::string& bytes)
{
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot open artifact temporary file");
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) throw std::runtime_error("cannot flush artifact temporary file");
    }
    std::filesystem::rename(temporary, path);
}

}  // namespace

void writeFullGameArtifacts(
    const FullGameCaseResult& result,
    const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    const std::string canonical = canonicalFullGameState(result.frames, result.events);
    const std::string digest = sha256Hex(canonical);

    billiardgl::json::Value summary = billiardgl::json::Value::object();
    summary["case_id"] = billiardgl::json::Value(result.caseId);
    summary["ball_collisions"] = billiardgl::json::Value(result.ballCollisions);
    summary["cue_ball_captures"] = billiardgl::json::Value(result.cueBallCaptures);
    summary["cue_contact_applied"] =
        billiardgl::json::Value(result.cueContactApplied);
    summary["cue_contact_miscue"] =
        billiardgl::json::Value(result.cueContactMiscue);
    summary["completed_shots"] = billiardgl::json::Value(result.completedShots);
    summary["declared_shots"] = billiardgl::json::Value(result.declaredShots);
    summary["deterministic_hash"] = billiardgl::json::Value(digest);
    summary["dropped_trace_frames"] = billiardgl::json::Value(
        static_cast<double>(result.droppedTraceFrames));
    summary["duplicate_contacts"] = billiardgl::json::Value(result.duplicateContacts);
    summary["failure"] = billiardgl::json::Value(result.failure);
    summary["foul_events"] = billiardgl::json::Value(result.foulEvents);
    summary["frame_count"] = billiardgl::json::Value(
        static_cast<double>(result.frames.size()));
    summary["maximum_penetration_cm"] =
        billiardgl::json::Value(result.maximumPenetrationCm);
    summary["maximum_residual_cm_s"] =
        billiardgl::json::Value(result.maximumResidualCmS);
    summary["game_over"] = billiardgl::json::Value(result.gameOver);
    summary["illegal_action_attempts"] =
        billiardgl::json::Value(result.illegalActionAttempts);
    summary["object_ball_captures"] =
        billiardgl::json::Value(result.objectBallCaptures);
    summary["passed"] = billiardgl::json::Value(result.passed);
    summary["peak_rss_bytes"] = billiardgl::json::Value(
        static_cast<double>(result.peakRssBytes));
    summary["rail_collisions"] = billiardgl::json::Value(result.railCollisions);
    summary["score_events"] = billiardgl::json::Value(result.scoreEvents);
    summary["schema_version"] = billiardgl::json::Value(2);
    summary["seed"] = billiardgl::json::Value(static_cast<double>(result.seed));
    summary["step_failures"] = billiardgl::json::Value(result.stepFailures);
    summary["surface_transitions"] =
        billiardgl::json::Value(result.surfaceTransitions);
    summary["turn_transfers"] = billiardgl::json::Value(result.turnTransfers);
    summary["wall_seconds"] = billiardgl::json::Value(result.wallSeconds);

    atomicWrite(directory / "trace.json", canonical + "\n");
    atomicWrite(directory / "summary.json", billiardgl::json::stringify(summary) + "\n");
    atomicWrite(directory / "index.csv",
        "case_id,seed,passed,frame_count,deterministic_hash\n" +
        result.caseId + "," + std::to_string(result.seed) + "," +
        (result.passed ? "true" : "false") + "," +
        std::to_string(result.frames.size()) + "," + digest + "\n");
}

}  // namespace fullgame
