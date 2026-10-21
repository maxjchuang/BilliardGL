#pragma once

#include "game_runtime.h"
#include "training/ball_edit_transaction.h"
#include "training/training_scenario.h"

#include <set>
#include <string>
#include <vector>

namespace billiardgl::training {

class Session {
public:
    explicit Session(GameRuntime& runtime);

    LoadResult load(const std::string& path,
        const std::set<std::string>& capabilities);
    bool active() const { return active_; }
    const Scenario& scenario() const { return scenario_; }
    const LoadResult& lastLoad() const { return lastLoad_; }
    Evaluation score(const AttemptFacts& facts) const;
    EditResult commit(BallEditTransaction& transaction);
    bool reset();
    bool undo();
    void markShotStart();
    void markShotStart(const GameState& state);
    bool retry();
    void abandon();
    bool save(const std::string& path, std::string& error) const;

private:
    void restore(const GameState& state);

    GameRuntime& runtime_;
    bool active_ = false;
    Scenario scenario_;
    LoadResult lastLoad_;
    GameState initialState_;
    GameState retryState_;
    bool hasRetryState_ = false;
    std::vector<GameState> undoStack_;
};

}  // namespace billiardgl::training
