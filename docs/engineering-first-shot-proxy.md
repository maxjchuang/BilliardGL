# Engineering first-shot proxy

`BilliardsModernProductFirstShotEvidenceE2E` is a headless engineering proxy for
the first-shot product flow. It is not a user study, usability result, rendered
capture, or evidence that the Phase 3 `5+3` participant protocol ran. That
protocol remains `NOT_RUN`.

The proxy starts at the real `ModernProductSession` main menu and uses only
`handlePlatformInput` plus public authority stepping. In one hybrid session it
uses the 2x/192-DPI Retina shell hit-test path to enter Single Player, verifies
the approved `wpa_heyball_2025_08_16_v1 / WPA Heyball` profile, selects Expert
AI, and starts Match. It then records observe state, uses Tab and mouse motion
to enter/adjust aim, uses aim-context arrow input for a non-centre tip offset,
uses the mouse wheel for non-default power, and uses left-button release for the
shot. The test never calls `pollAi`, never mutates `GameState`, never invokes a
private shot mutation, and never loads or jumps to a scenario.

Pass requires all of the following inside a 30-second wall-time budget:

- every submitted platform event has a public handled/ok/error result;
- pre-shot and pre-release `ShotControlModel` values are recorded;
- the authority journal contains exactly one `shoot` action;
- cue contact is publicly reported as applied;
- fixed-step authority reaches stationary post-shot state and publishes
  `shot_ended`;
- Help is never opened, AI starts no planning job, and product Assist retains
  its no-auto-aim/no-auto-shoot contract.

The CTest writes machine-readable evidence to the build tree at
`test-output/first-shot-proxy/engineering-first-shot-proxy-v1.json`. The JSON
contains a monotonic sequence/elapsed-time timeline, platform actions and public
results, screen and authority tick, shot-control snapshots, retained semantic
actions/runtime events, wall time, explicit failure text, and the
`phase3_five_plus_three_user_study: NOT_RUN` boundary. It is generated test
output and must not be committed as a participant record.

Coverage is deliberately scoped to one headless hybrid session: Retina pointer
shell routing plus keyboard/mouse gameplay routing. Native Cocoa event synthesis
and a second keyboard-only complete session are not claimed.
