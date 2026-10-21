# Phase 4 training and replay system

## Training content binding

`training_scenario_v1` verifies the SHA-256 of the original embedded
`initial_scenario.scenario` before parsing or applying it. Canonical bytes use
UTF-8, sorted object keys, compact separators, Python-compatible shortest float
spelling, and a trailing LF, matching the Phase 4 content generator.

The content pack names its equipment `CHINESE_EIGHT_BALL`. The legacy physics
scenario v1 parser predates that product-facing name and accepts `WPA_POOL`.
After the original content hash passes, the loader changes the equipment name
only in a temporary parser-adapter copy. The bound content, its hash, and the
committed training asset are never changed by this adapter.

Missing runtime capabilities do not silently enable scoring. The scenario loads
as visible and locked with `score_enabled=false` and lists each missing
capability.

## Runtime isolation

Free-ball placement stages changes in a `BallEditTransaction`. It validates the
cue ball, finite positions, ball overlap, table bounds, and pocket interiors,
then commits the complete candidate state atomically. Training reset, undo, and
retry restore complete state snapshots through `GameRuntime`.

Training trajectory assistance uses `PreviewRuntime`, an isolated state/profile
clone with explicit tick, wall-time, and active-ball budgets. Preview events are
discarded inside the sandbox. The authoritative state, tick, event stream, and
audio path are not exposed to the preview simulation.

## Replay v1

Replay v1 binds format, executable and content versions, content SHA-256,
physics profile ID and SHA-256, initial state, deterministic seed, tick-sorted
semantic actions, and the final state summary/hash. Binding mismatches fail
closed. Numeric fields reject negative, fractional, nonfinite, and out-of-range
values before conversion.

Playback supports pause, 1/4x, 1/2x, 1x, 2x, fixed-tick stepping, and seek to a
shot. Playback rate changes only scheduler cadence; `GameRuntime` always uses
the profile's fixed physics step.

`ReplayTransportModel` is the UI-facing transport boundary. It also carries the
free/broadcast camera choice, the isolated replay cue trajectory, cue endpoint,
and observed rail-event count. The replay player owns a separate `GameRuntime`;
transport, seek, camera, and overlay work cannot mutate training authority.

The authoritative semantic-action journal is a fixed-capacity deque. A recorder
that consumes periodically can preserve streams longer than the journal, while
a lagging recorder fails with `replay_action_journal_overrun`. It never silently
serializes a replay after required actions have been evicted. A runtime journal
generation change likewise fails with `replay_action_journal_reset`.

## Product controller and trajectory overlay

`TrainingProductController` validates `training_pack_v1`, confines scenario
paths to the pack directory, verifies every manifest SHA-256, and parses every
scenario before exposing cards to UI. Its lifecycle covers start, score,
reset/retry, and atomic Replay v1 save. Each reset/retry begins a fresh recording
from the restored authoritative state.

`TrajectoryOverlayModel` presents predicted and replay-actual cue paths with
non-color-only encoding: amber dashed endpoint-ring for prediction and cyan
solid tick-marked line for actual replay, plus explicit legend labels. Both
tracks expose endpoint and rail-event count. Advanced details are collapsed by
default.

Automated acceptance covers all 12 launch scenarios with constructed success
and failure facts, pack/hash rejection, lifecycle invalid states, all four replay
rates, fixed-tick scheduling, seek/camera controls, overlay encoding, and
authority hash/tick/event isolation. These checks establish deterministic
product-layer behavior; they do not claim human playability or final rendered
quality, which require the later UI/renderer hookup and human visual review.

## Automation commands

The headless and rendered controllers advertise versioned training load/state,
score, reset, undo, retry, save, edit transaction, and preview commands. Replay
commands cover load/state, pause, speed, fixed-tick step, and seek-to-shot.
