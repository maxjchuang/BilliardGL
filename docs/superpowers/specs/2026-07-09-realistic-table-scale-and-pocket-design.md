# Realistic Table Scale and Pocket Design

## Goal

Bring the table, balls, cue, and pocket behavior closer to real-world Chinese billiards dimensions while keeping the implementation scoped to basic physical consistency.

This design targets option B from the discussion: visual scale plus basic physics consistency. It does not implement full rounded pocket shoulder collision, reject behavior, or detailed pocket cut geometry.

## Current Problems

The current project mixes physical dimensions, rendering model dimensions, and legacy constants.

- `kBallRadius` is `5.715`, which matches the real-world ball diameter in centimeters, but the code treats it as a radius. This makes rendered and simulated balls roughly twice the correct size.
- The playfield dimensions are close to a real Chinese billiards table, but they are duplicated between modern constants and legacy macros.
- Pocket behavior is represented by one `kPocketRadius` plus a pocketed test of `distance < kBallRadius / 4`, which is not a meaningful real-world pocket opening.
- Corner pockets and side pockets are not differentiated.
- Table visual geometry comes from `table.obj`, while physics uses hardcoded playfield dimensions. These do not share an explicit source of truth.

## Real-World Scale Targets

Use centimeters as the project unit for table-scale gameplay values.

| Item | Target |
| --- | ---: |
| Ball diameter | `5.715 cm` |
| Ball radius | `2.8575 cm` |
| Playfield length | `254.0 cm` |
| Playfield width | `127.0 cm` |
| Table height | `85.0 cm` preferred, or preserve `87.0 cm` temporarily if visual model alignment requires it |
| Cue length | approximately `145.0 cm` |

Pocket dimensions should be represented as pocket-opening parameters rather than as one generic radius. Corner and side pockets must have separate values because they play differently and have different geometry.

## Architecture

Add a small specification layer, for example `table_specs.h/.cpp`, that centralizes dimensions:

- `BallSpec`
  - `radiusCm`
  - optional derived `diameterCm`
- `TableSpec`
  - `playfieldLengthCm`
  - `playfieldWidthCm`
  - `heightCm`
- `PocketSpec`
  - `cornerMouthWidthCm`
  - `sideMouthWidthCm`
  - `dropZoneDepthCm`
  - future extension fields for shoulder radius and cut angle

Existing constants such as `kBallRadius`, `kTableInWidth`, and `kTableInLength` can remain as compatibility aliases during the transition, but they should derive from the specification layer. Legacy macros in `billiards.cpp` should either be removed or redirected to the same source of truth so values cannot drift.

## Pocket Model

Generate six `PocketOpening` values from the table spec:

- Four corner pockets.
- Two side pockets.
- Each opening has a type, center point, mouth width, and drop-zone depth.

The pocketed check should no longer use `distance < kBallRadius / 4`.

Use a basic two-stage test:

1. Determine whether the ball center is inside the relevant pocket mouth area.
2. Determine whether the ball center has crossed far enough past the playfield boundary into the drop zone.

For this phase, keep cushion collision rectangular, but skip normal cushion reflection when the ball is inside a pocket mouth opening. This prevents balls from bouncing off an invisible straight rail at places where a pocket exists.

This phase explicitly does not simulate:

- Rounded pocket shoulder collisions.
- Pocket cut angle, such as a `72 deg` pocket jaw.
- Rejects caused by brushing the pocket shoulder.
- Tangential bounce against curved pocket geometry.

Those behaviors are left for a later full pocket-geometry phase.

## Rendering Consistency

Balls should render using the corrected real radius. Ball center height should remain `tableHeight + ballRadius`.

Rack spacing must derive from the corrected radius:

- Horizontal adjacent spacing: `2 * ballRadius`.
- Row spacing: `sqrt(3) * ballRadius`.

Cue placement, guide-line start, and any particle emitter bounds must also derive from the corrected ball radius.

The existing `table.obj` should not be regenerated in this phase. Instead, document that the visual model bounding box and the physical playfield dimensions are not yet guaranteed to come from the same source. If visual mismatch remains after the ball and pocket fixes, table model scaling should become a separate task.

## Testing

Add focused tests for the new physical dimensions and pocket behavior:

- Ball radius is `2.8575 cm`.
- Playfield is `254.0 x 127.0 cm`.
- Rack spacing creates touching balls without overlap.
- Ball-wall collision uses the corrected playfield and ball radius.
- Corner pocket and side pocket checks both work.
- A ball inside a pocket mouth is not reflected by the ordinary rectangular rail.
- A ball near a rail but outside a pocket mouth still reflects normally.

Behavioral tests should cover:

- Initial cue ball and rack placement.
- Object balls do not overlap at setup.
- A ball entering a corner pocket becomes pocketed.
- A ball entering a side pocket becomes pocketed.

Visual verification should use the program's screenshot path when available:

- Default table view.
- Aim mode view.
- A pocket-near-ball view if practical.

The expected visual result is that the ball-to-table ratio matches real Chinese billiards more closely and that pocket locations no longer obviously contradict the physics behavior.

## Rollout

Implement in small steps:

1. Introduce the specification layer and compatibility aliases.
2. Correct ball and table dimensions.
3. Update rack placement, cue placement, particles, and physics calculations to use the corrected values.
4. Replace pocketed detection with generated pocket openings.
5. Update tests and screenshot verification.
6. Reassess whether table OBJ scaling is still necessary.

## Out Of Scope

- Full rounded pocket shoulder collision.
- Full table mesh regeneration.
- New ball textures or higher-resolution ball assets.
- Rule changes unrelated to scale and pocket behavior.
- Online play, AI, or gameplay mode changes.
