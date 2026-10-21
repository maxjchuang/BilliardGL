# Product HUD runtime boundary

`ProductHudViewModel` and `HudStateReducer` are presentation-only consumers of
`PresentationSnapshot`. They never read or mutate `GameRuntime`, retain a prior
shot, or emit gameplay input. The reducer produces five independent states:
`default`, `aiming`, `post_shot`, `foul`, and `help`.

Every player-facing field has a neutral fallback. This includes player, group,
target ball/pocket, power percentage, estimated cue speed with `m/s`, tip X/Y,
camera, assist, foul reason/resolution, and result. Foul and post-shot data are
only populated for the current transient shot-ended/game-over snapshot, so a
later default frame cannot inherit stale adjudication text.

Chinese eight-ball reason codes map to readable English plus separate icon and
shape tokens. Groups likewise use labels and filled/banded shapes. Product
meaning never relies on color alone.

## Manifest and responsive layout

The runtime loads
`assets/modern/hud/focused_practice_hud_v1.hud.json` and fails closed on an
unknown schema, manifest ID, scale contract, profile geometry, or capability
gate contract. `ResponsiveHudLayoutSolver` applies the manifest's compact/wide
breakpoint, 96-DPI reference, 85–135% user scale range, safe areas and minimum
font sizes.

Automated coverage checks all five active states at:

- 1024×768 compact;
- 1280×720 minimum wide;
- 1920×1080 reference;
- 1920×1080 at 144 DPI;
- 85%, 100%, and 135% UI scale for every resolution/DPI case.

Each result is checked for physical safe-area containment, pairwise non-overlap,
and a minimum 13-pixel physical font. Debug telemetry is a separate overlay,
hidden by default and always suppressed in release capture.

Cue elevation and chalk nodes require both their UI capability and promoted
physics capability, in addition to presence in the snapshot. Missing capability
omits the node; no decorative or disabled control is emitted.

## First-shot proxy and rules E2E

`FirstShotAutomationProxy` observes the manifest's ordered event sequence and
deadlines: default HUD, user aim toggle, power hold, at least three monotonic
power samples, exactly one user shot release, and post-shot HUD within 30
seconds. It rejects automatic aim or shot sources and has no API that can issue
gameplay commands. This is an engineering automation proxy, not human research
or a human playability claim.

The product-state E2E covers legal break, open table, solid-group assignment,
balls 1–7 cleared one at a time, legal 8-ball win, and a wrong-first-contact plus
no-rail foul. Rendered integration and screenshot review remain a later
modern-renderer task.
