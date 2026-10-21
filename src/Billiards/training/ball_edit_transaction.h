#pragma once

#include "game_state.h"
#include "physics_profile.h"

#include <string>

namespace billiardgl::training {

struct EditResult {
    bool ok = false;
    std::string errorCode;
};

class BallEditTransaction {
public:
    BallEditTransaction(const GameState& base, const PhysicsProfile& profile);

    EditResult setBall(int index, const Point3& position, bool pocketed = false);
    EditResult validate() const;
    EditResult commit(GameState& target);
    const GameState& candidate() const { return candidate_; }
    bool committed() const { return committed_; }

private:
    GameState candidate_;
    PhysicsProfile profile_;
    bool committed_ = false;
};

}  // namespace billiardgl::training
