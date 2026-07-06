# BilliardGL Aim and Camera Decoupling Design

## Context

BilliardGL currently derives cue direction from the camera. The renderer draws the cue line from `cameraEye - cameraTarget`, and the shot code sets cue ball velocity from `cameraTarget - cameraEye`. This makes the camera both an observation tool and the source of shot intent.

That coupling was simple for the original GLUT game, but it now conflicts with modern play expectations. A player should be able to inspect the table from different angles without accidentally changing the shot line. This matters especially on a MacBook trackpad, where camera movement and fine aiming need different interaction semantics.

## Goal

Separate observation from shot intent.

The camera should control what the player sees. A new aim state should control where the cue ball will go. The camera may assist the aiming experience, but it must not be the authoritative source for cue direction.

## Chosen Interaction Model

Use the recommended hybrid model:

- Default mode is observation.
- Press `Tab` to toggle aim mode.
- In observation mode, mouse and trackpad movement control the camera only.
- In aim mode, horizontal mouse or trackpad movement adjusts the cue direction.
- The shot line, cue model, and cue ball velocity all read the same aim direction.
- Camera soft-follow can be added later, but aim direction remains authoritative.

This gives the player a clear mental model:

- Move the camera to understand the table.
- Enter aim mode to change the shot.
- Charge and release the shot using the current aim direction.

## Non-Goals

- Do not add full collision prediction in this first pass.
- Do not redesign the full HUD.
- Do not change the pool rule system.
- Do not replace GLUT or fixed-pipeline OpenGL.
- Do not require screenshots or visual artifacts to be committed.

## State Design

Add an explicit aim model to `GameState`.

```cpp
enum class AimMode {
    Observe,
    Aim
};

struct AimState {
    AimMode mode = AimMode::Observe;
    float yaw = -kPi / 2.0f;
    float sensitivity = 0.01f;
};
```

`yaw` represents the horizontal table-plane direction of the cue ball's outgoing path. It is independent from `CameraState::angleX` and `CameraState::angleY`.

The initial aim direction should match the current default camera-facing shot direction closely enough that startup behavior feels familiar. After that, camera movement must not mutate `AimState::yaw`.

## Runtime Data Flow

Target data flow:

```text
input
  -> observation mode: update camera
  -> aim mode: update aim yaw

render cue line
  -> read aim yaw

render cue model
  -> read aim yaw

shot release
  -> convert aim yaw to cue ball velocity

camera
  -> may read aim yaw for optional follow behavior
  -> must not write aim yaw implicitly
```

The cue direction vector should be derived from `AimState::yaw` through a small helper, for example:

```cpp
Point3 aimDirectionOnTable(float yaw)
{
    return Point3{std::cos(yaw), 0.0f, std::sin(yaw)};
}
```

The exact sign convention should be chosen to preserve the current visual direction at startup, then used consistently by rendering and physics.

## Input Behavior

### Observation Mode

Observation mode is the default.

- Existing trackpad orbit and right mouse drag behavior remain camera controls.
- Arrow keys continue to orbit the camera unless later reassigned.
- Camera movement does not change the cue line.
- Left mouse shot charging uses the existing shot flow, but the outgoing shot direction comes from `AimState::yaw`.

### Aim Mode

Press `Tab` to toggle aim mode. `A` is intentionally not used because the current game already maps `A / D` to camera pan, and preserving that control avoids a regression for existing players.

- Horizontal mouse or trackpad movement changes `AimState::yaw`.
- Vertical movement is ignored for aim in the first pass.
- Camera orbit is suspended while aim mode consumes pointer movement.
- Pressing `Tab` again returns to observation mode.
- `Esc` may return to observation mode if the current input architecture already has a suitable key path.

The first implementation should avoid combining camera orbit and aim yaw changes in the same drag gesture. That separation is the main UX improvement.

### Shot Charging

Shot charging should remain recognizable:

- Holding left mouse charges shot power.
- Releasing left mouse requests a hit.
- The hit uses `AimState::yaw`, not camera eye/target.

If left-drag currently starts camera behavior in some path, aim mode should take priority over camera behavior to avoid accidental view changes while aiming.

## Rendering Behavior

The cue line should be stable under camera movement.

Renderer responsibilities:

- Convert `AimState::yaw` to a horizontal direction.
- Draw the cue line from the cue ball along that direction.
- Place and rotate the cue model from the same direction.
- Keep cue power offset behavior visually similar to the current implementation.

The renderer should not recompute shot direction from `RenderResources::cameraEye` or `RenderResources::cameraTarget`.

## HUD Behavior

Keep the HUD lightweight.

Always show the existing help prompt without interfering with play. Add a compact mode/status hint such as:

```text
Mode: Observe | Tab Aim
Mode: Aim | Tab Observe
```

The exact wording can be adjusted to fit the current HUD layout, but the user must be able to tell whether pointer movement will rotate the camera or adjust the shot line.

## Testing

Add focused unit tests around the new behavior.

Recommended coverage:

- Initial `AimState` has a deterministic yaw.
- Camera orbit input changes camera angles but does not change aim yaw in observation mode.
- Aim mode pointer movement changes aim yaw but not camera angles.
- Shot velocity is derived from aim yaw, not camera eye/target.
- Cue direction helper returns normalized horizontal directions for representative yaws.

Existing input, game state, physics, rendering resource, and screenshot tests should continue to pass.

## Visual Verification

Use the existing in-process screenshot flow as a manual verification aid:

- Capture a default frame.
- Move/orbit the camera and capture another frame.
- Confirm the cue line is still pointing in the same table-plane direction.
- Toggle aim mode, adjust aim yaw, and capture a frame showing the cue line changed.

Screenshots should stay under `/tmp` unless explicitly requested otherwise.

## Implementation Plan Shape

Implement in small commits:

1. Add `AimState`, aim helpers, and tests.
2. Route input through observation/aim mode behavior.
3. Change shot velocity calculation to use aim direction.
4. Change cue line and cue model rendering to use aim direction.
5. Update HUD/help text and run build, unit tests, and screenshot verification.

This order keeps the behavior testable before touching the OpenGL rendering path.

## Acceptance Criteria

- Camera movement no longer changes the cue line or shot direction.
- `Tab` toggles between observation mode and aim mode.
- Aim mode pointer movement changes the cue line.
- Shot velocity, cue line, and cue model all use the same aim direction.
- HUD clearly communicates the active mode.
- Build and unit tests pass.
- Manual or automated screenshots demonstrate stable cue direction across camera movement.

## Open Questions

- Should arrow keys remain camera-only in aim mode, or should left/right arrows fine-tune aim yaw?
- Should aim mode automatically exit after a shot, or stay active for the next turn?
- Should camera soft-follow be implemented immediately after the basic decoupling, or deferred until the first pass has been tested in play?
