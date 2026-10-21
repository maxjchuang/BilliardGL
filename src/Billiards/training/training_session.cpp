#include "training/training_session.h"

#include "automation_json.h"
#include "deterministic_hash.h"

#include <cstdio>
#include <fstream>

namespace billiardgl::training {

Session::Session(GameRuntime& runtime) : runtime_(runtime) {}

LoadResult Session::load(const std::string& path,
    const std::set<std::string>& capabilities)
{
    lastLoad_ = loadScenarioFile(path, capabilities);
    if (!lastLoad_.ok) return lastLoad_;
    const ActionResult applied = applyPhysicsScenario(
        runtime_, lastLoad_.scenario.initialScenario);
    if (!applied.ok) {
        lastLoad_.ok = false;
        lastLoad_.errorCode = applied.errorCode;
        lastLoad_.errorMessage = "initial scenario rejected";
        return lastLoad_;
    }
    scenario_ = lastLoad_.scenario;
    initialState_ = runtime_.state();
    retryState_ = initialState_;
    hasRetryState_ = true;
    undoStack_.clear();
    active_ = true;
    return lastLoad_;
}

Evaluation Session::score(const AttemptFacts& facts) const
{
    if (!active_) return Evaluation{false, true, false, 0, "training_session_inactive"};
    return evaluate(scenario_, facts);
}

EditResult Session::commit(BallEditTransaction& transaction)
{
    if (!active_) return EditResult{false, "training_session_inactive"};
    GameState candidate;
    const EditResult result = transaction.commit(candidate);
    if (!result.ok) return result;
    undoStack_.push_back(runtime_.state());
    restore(candidate);
    return EditResult{true, ""};
}

void Session::restore(const GameState& state)
{
    runtime_.replaceStateForScenario(state, runtime_.physicsProfile(), nullptr,
        false, runtime_.boundaryMode());
}

bool Session::reset()
{
    if (!active_) return false;
    undoStack_.push_back(runtime_.state());
    restore(initialState_);
    retryState_ = initialState_;
    hasRetryState_ = true;
    return true;
}

bool Session::undo()
{
    if (!active_ || undoStack_.empty()) return false;
    const GameState previous = undoStack_.back();
    undoStack_.pop_back();
    restore(previous);
    return true;
}

void Session::markShotStart()
{
    markShotStart(runtime_.state());
}

void Session::markShotStart(const GameState& state)
{
    if (!active_) return;
    retryState_ = state;
    hasRetryState_ = true;
}

bool Session::retry()
{
    if (!active_ || !hasRetryState_) return false;
    undoStack_.push_back(runtime_.state());
    restore(retryState_);
    return true;
}

void Session::abandon()
{
    active_ = false;
    scenario_ = Scenario{};
    lastLoad_ = LoadResult{};
    initialState_ = GameState{};
    retryState_ = GameState{};
    hasRetryState_ = false;
    undoStack_.clear();
}

bool Session::save(const std::string& path, std::string& error) const
{
    if (!active_) {
        error = "training_session_inactive";
        return false;
    }
    json::Value root = json::Value::object();
    root["content_version"] = json::Value(1);
    root["physics_profile_id"] = json::Value(runtime_.physicsProfile().id);
    root["schema"] = json::Value("training_saved_scenario_v1");
    root["schema_version"] = json::Value(1);
    root["source_scenario_id"] = json::Value(scenario_.id);
    root["state"] = json::parse(canonicalGameState(runtime_.state())).value;
    root["state_sha256"] = json::Value(gameStateSha256(runtime_.state()));
    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "training_save_open_failed";
        return false;
    }
    output << json::stringify(root) << '\n';
    output.close();
    if (!output || std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        error = "training_save_commit_failed";
        return false;
    }
    return true;
}

}  // namespace billiardgl::training
