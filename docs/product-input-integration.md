# Product normalized input integration contract

Status: pure C++ product input closure implemented; native event adaptation is
deferred to the platform application.

## Boundary

`ProductInputController` is the only app-facing input seam for the product
bindings. A platform application converts a native event into:

```cpp
NormalizedInputEvent {
    device, control, trigger, value, secondaryValue
}
```

The three identity strings use the same normalized vocabulary persisted in
`ProductSettings`; examples are `keyboard / F10 / press`,
`mouse / MoveX / continuous`, and `mouse / WheelUp / step`. `value` and
`secondaryValue` are app-normalized logical deltas or step counts, not pixels
or Cocoa key codes. The controller has no Cocoa, window, or renderer include.

The caller selects `Play`, `Training`, or `Replay` and reports whether an AI
turn currently owns shot control. Before every resolution, the controller
derives ordered active contexts:

- open settings: `settings`;
- replay: `replay`;
- training: `aim|observe`, then `training`, then `play`;
- normal play: `aim|observe`, then `play`;
- `global` is always the router's explicit fallback.

The platform may close the process only when the result carries
`ProductAppRequest::Quit`. It should display `reasonCode` for handled failures
and may ignore unhandled `input_unbound` events.

## Authority routing

Resolved actions never reproduce physics, shot, camera, training, replay, or
settings mutations in the adapter:

- aim, power, shoot, help, camera orbit/pan/zoom, camera anchor, and return to
  cue ball dispatch public `GameRuntime` semantic actions;
- `GameRuntime` remains the owner of `ShotControlModel` and
  `CameraStateMachine` updates;
- training reset calls `TrainingProductController::resetAttempt`;
- replay pause, step, playback rate, seek, and camera mode call
  `ReplayTransportModel`;
- settings navigation, transaction apply/cancel, and capture call
  `ProductSettingsRuntime` / `SettingsMenuModel`.

`ToggleCameraAnchor` and `ReturnToCueBall` are explicit `ActionType` values, so
the normalized layer does not synthesize legacy keyboard events.

## Settings modal and rebind behavior

Opening settings immediately replaces play contexts with `settings`. An input
that would otherwise be a play action returns `input_settings_modal` and cannot
reach authority. The menu edits a transaction preview; F10 or the global quit
binding cancels while the modal is open. `settings_apply` uses the existing
same-directory atomic save boundary.

After `beginRebind`, the next valid normalized event is captured before normal
routing. On successful apply, `InputActionRouter` observes the updated live
`ProductSettings` object, so the next event uses the new binding with no cache
or restart.

## Stable rejection policy

The controller rejects malformed identity, NaN/infinite payloads, and values
that cannot be represented by the semantic float interface before dispatch.
Player shot-control actions are rejected with `input_ai_turn_active` while the
AI owns the turn and `input_balls_moving` while balls are moving. Missing
optional product controllers return `input_training_unavailable` or
`input_replay_unavailable`. Errors from the authoritative controller are
preserved when they are already stable, such as `replay_shot_not_found`.

Rejected input never appends to the applied semantic action journal. Assist
selection is synchronized only as shot metadata through `setAssistLevel`;
assist never invokes aim or shoot and `assistCanAutoAimOrShoot` remains false.

## Default action coverage

The version-1 settings defaults include 30 actions / 36 device bindings across
global, play, aim, observe, training, settings, and replay. Settings controls
cover focus/category movement, increase/decrease, toggle, and apply. Replay
controls cover pause, single-step, fixed policy playback rates, camera mode,
and shot seek. Rebinding retains the action's context.

## Verification

`BilliardsProductInputControllerTests` is a headless C++ target. It verifies:

- default input and immediate post-apply rebindings;
- ordered dynamic contexts and modal play blocking;
- settings preview, cancel, rebind capture, and atomic persistence;
- aim/power/shoot semantic journal round-trip;
- AI-turn, moving-ball, malformed, non-finite, and unbound rejection without
  journal pollution;
- training reset through the product controller;
- replay pause/rate/camera/step/seek without mutating play authority hash,
  tick, or journal;
- global fallback inside replay.

No renderer, modern application, native event API, or platform audio dependency
is linked into this test.
