# Focused Practice HUD Phase-3 Specification

This package is an original product-HUD specification for the BilliardGL
presentation layer. It contains no external font, icon, image, brand, or
competitor UI source. The SVG mockups are generated from repository-owned code
and are byte deterministic.

## Deliverables

- `focused_practice_hud_v1.hud.json`: machine-readable layout, state, input,
  accessibility, safe-area, DPI, debug-isolation and capability-gating contract.
- `focused_practice_hud_shipping_tokens_v1.json`: normalized 24×24 straight-line
  GPU icon geometry, standard/color-vision-safe palettes, SDF typography and
  state-to-token mappings. It closes the prior gap where draw lists emitted
  opaque icon strings without repository-owned geometry.
- `focused_practice_hud_shipping_tokens_flat_proof_v1.svg`: deterministic flat
  proof for default/aiming/post-shot/foul/help and both palettes. It is explicitly
  not a GPU screenshot or user-research artifact.
- `focused_practice_shell_shipping_tokens_v1.json`: standalone ProductShell
  namespace with all 43 icon tokens emitted by the nine shipping screens. It
  includes normalized geometry, standard/CVD palettes, per-screen/state usage,
  strict unknown-token policy and no aliases or external assets.
- `focused_practice_shell_shipping_tokens_v1.manifest.json`: SHA-bound Shell
  asset receipt and source-audit contract. It does not change the three HUD v1
  files or require the HUD and Shell registries to merge.
- `focused_practice_shell_shipping_tokens_flat_proof_v1.svg`: all 43 Shell
  tokens in both palettes. It is an inventory proof, not a GPU capture or user
  study.
- Four wide 1920×1080 SVG mockups.
- Four compact 1024×768 SVG mockups.

The four review states are:

1. `default` — player, group, target, camera and assist context.
2. `aiming` — default information plus segmented power, numeric percent,
   estimated cue speed and numeric/crosshair tip offset.
3. `post_shot_foul` — the post-shot result and foul layers shown together so
   both independent runtime states can be reviewed in one composition.
4. `help` — keyboard, mouse and trackpad quick controls plus the first-shot path.

The runtime manifest keeps `post_shot` and `foul` as separate states, so there
are five runtime states and four mockup compositions.

## Accessibility and authority boundaries

- Solid/stripe groups use text, number, fill/band pattern and shape in addition
  to color.
- Foul feedback uses the word `FOUL`, a warning triangle and destructive color.
- Power uses a bar, tick marks, percentage and `EST.` cue-speed value with unit.
- Tip offset uses a ball outline, crosshair, marker and numeric `X/Y` values in
  ball-radius units.
- Product text contrast is at least 4.5:1; the minimum compact caption is 13 px.
- Shipping vector tokens use only normalized rect/circle/polyline/polygon commands,
  so the existing draw-list path can expand them into solid/line triangles without
  an SVG runtime or a second font system. Power, tip, camera, foul, help and group
  semantics all have original geometry.
- Responsive proof cases cover 1280×720, reference 1920×1080 and 144-DPI
  1920×1080. The validator recomputes physical safe-area containment and
  active-state rectangle intersections for all three.
- Debug telemetry is a separately identified overlay, hidden by default and
  forbidden in release captures.
- Cue elevation and chalk are omitted from all product states until both their
  UI capability and physics-promotion flags exist. There is no disabled or
  decorative fallback control.
- Estimated cue speed is presentation guidance, not authoritative physics.
- A deterministic first-shot automation proxy checks status by 500 ms, aiming
  by 11 s, aiming-control response within 100 ms, one release by 30 s and
  post-shot visibility within 250 ms. It is an engineering proxy, not a claim
  of completed user research.

## Regenerate and validate

```sh
python3 scripts/generate_phase3_hud_package.py \
  --output-dir assets/modern/hud
python3 scripts/validate_phase3_hud_package.py \
  assets/modern/hud/focused_practice_hud_v1.hud.json
python3 -m unittest tests.visual.test_phase3_hud_package

python3 scripts/generate_phase3_shell_token_package.py \
  --output-dir assets/modern/hud
python3 scripts/validate_phase3_shell_token_package.py \
  assets/modern/hud/focused_practice_shell_shipping_tokens_v1.manifest.json
python3 -m unittest tests.visual.test_phase3_shell_token_package
```

See `docs/art/phase3-hud-implementation-acceptance.md` for the implementation
and rendered-automation acceptance checklist.
