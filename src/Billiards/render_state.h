#pragma once

#include "game_state.h"
#include "presentation_snapshot.h"
#include "camera/camera_state_machine.h"
#include "shot_control_model.h"

namespace billiardgl {

class GameRuntime;

// Creates the immutable, presentation-only frame consumed by renderer and UI.
// Both inputs are completed authoritative states and are never modified.
PresentationSnapshot buildPresentationSnapshot(
    const GameState& previous, const GameState& current, double alpha);

PresentationSnapshot buildPresentationSnapshot(
    const GameState& previous, const GameState& current, double alpha,
    const ShotControlModel& shotControl,
    const ShotControlCapabilities& shotCapabilities,
    const camera::Frame& cameraFrame);

PresentationSnapshot buildPresentationSnapshot(
    const GameState& previous, const GameState& current, double alpha,
    const ShotControlModel& shotControl,
    const ShotControlCapabilities& shotCapabilities,
    const camera::Frame& cameraFrame,
    const RulesPresentation& rulesPresentation);

RulesPresentation buildRulesPresentation(const GameRuntime& runtime);

}  // namespace billiardgl
