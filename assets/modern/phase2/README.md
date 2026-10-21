# Focused Practice Hall Phase-2 Modular Candidate

This directory contains an original, procedural, byte-deterministic glTF 2.0
production candidate. It is a compact loader-facing package, not a final high
mesh and not a replacement for the frozen simulation geometry.

No competitor asset, legacy OBJ/BMP, photograph, downloaded model, font, logo,
or third-party texture is used. The committed PNG and SVG are generated from
the same in-repository procedural source.

## Package

- `focused_practice_hall_modular_v1.gltf`: embedded geometry buffer, modular
  table/ball/cue/lamp LOD0/1/2, simple room shell, UV0, normals and tangents.
- `focused_practice_hall_modular_v1.asset.json`: loader mapping, LOD roots,
  ball material variants, frozen alignment binding, content hashes and budgets.
- `focused_practice_hall_ball_identity_atlas_v1.png`: original 4096×512 RGBA8
  8×2 atlas for cue ball and balls 1–15. Each 512×256 tile preserves the 2:1
  equirectangular sphere aspect and uses a 24 px safe inset.
- `focused_practice_hall_ball_identity_atlas_v1.svg`: inspectable procedural
  vector source for the atlas.
- `focused_practice_hall_ball_identity_closeup_1_8_11_15_v2.png`: review-only
  native-resolution evidence strip for the four difficult identities.
- `focused_practice_hall_pbr_v1.params.json`: fixed seeds, resolutions, color
  spaces, UV repeats and mip-generation policy for six procedural PBR sets.
- `focused_practice_hall_lighting_rig_v1.json`: rectangular key light,
  analytic environment, manual exposure and low/medium/high soft-shadow tiers.
- `focused_practice_hall_pbr_review_v1.png`: review-only material-sphere and
  repeated-UV contact sheet; top row felt/walnut/rubber, bottom row
  gunmetal/leather/cue.
- `focused_practice_hall_venue_v1.gltf` and `.asset.json`: modular 8×10×4 m
  venue shell LOD0/1/2 with floor, walls, ceiling, lamp rack/cables, acoustic
  panels and restrained wall trim. It contains no furniture, brand or text.
- `focused_practice_hall_venue_four_camera_v1.png`: review-only composition
  sheet. Its manifest mapping is top-left `player_shot`, top-right `low_aim`,
  bottom-left `tactical_overhead`, bottom-right `broadcast_oblique`.

The table candidate separates teal felt, cushion rubber, walnut body,
gunmetal trim, pocket leather, dark pocket liner and visual-only pocket void
slots. The cue uses
shaft and tip slots. The rectangular lamp uses housing and diffuser slots.
All three sphere LODs share the same 5.715 cm diameter and can select any of
the 16 atlas-backed ball material entries listed in the asset manifest.

`KHR_materials_clearcoat` and `KHR_texture_transform` are optional in
`extensionsUsed`; neither appears in `extensionsRequired`. Every clearcoat
material retains a core glTF metallic-roughness fallback. A loader that has not
implemented these extensions can therefore render a valid, reduced-fidelity
fallback and can use the manifest's explicit atlas tile mapping if needed.

The runtime PBR maps are `baseColor` sRGB, OpenGL `+Y` tangent-space normal,
and linear ORM (`R=AO`, `G=roughness`, `B=metallic`). Felt, walnut and cue use
512² maps; rubber, gunmetal and leather use 256² maps. All procedural sampling
and normal finite differences wrap at tile boundaries. Runtime mip generation
must filter baseColor in sRGB, ORM in linear space, and renormalize filtered
normal vectors.

## Frozen compatibility boundary

The candidate keeps the approved M1/M2 binding:

- `chinese_pool_interactive_120hz_v5` / `legacy_opening_band`;
- play surface `0.86483976 m`, cushion nose `1.4R` above it;
- mouth-center and nominal mouth-endpoint markers only;
- marker tolerance `0.05 cm`, visual-solid clearance `0.10 cm`;
- simulation-driven capture presentation with no render detach.

Every LOD references the identical 23-node marker list. No jaw, throat or
capture marker exists. Pocket void disks are explicitly visual-only; cushions
and leather guards are checked offline against the mouth clearance corridor.

The v10b visual pocket frame is `+X=physics tangent`, `+Y=world up`, and
`+Z=cavity inward`, with the signed physics marker mapping
`Z_visual=-Z_marker`. Its black void stations are exactly 1.00D / 1.60D /
2.20D for a 57.15 mm ball, yielding a 1.20D authored span. The validator also
enforces the 4.425 mm side margin, 5.715 mm Z margin, 7 mm liner band,
minimum 0.32 black depth/width ratio, visible-walnut outward half-space, and
the rubber → facing → dark liner → black void material sequence.

## Current budgets

| Item | Result |
| --- | ---: |
| Unique triangles, all generated LOD meshes | 6,448 |
| Table LOD0 / 1 / 2 | 2,120 / 1,240 / 408 |
| Shared ball LOD0 / 1 / 2 | 1,536 / 560 / 192 |
| Cue LOD0 / 1 / 2 | 96 / 48 / 32 |
| Lamp LOD0 / 1 / 2 | 120 / 24 / 12 |
| Materials | 28 total: 12 functional/base + 16 ball variants |
| Textures | 19: 1 ball atlas + 18 PBR maps |
| Embedded geometry buffer | 392,880 bytes |
| Atlas disk / RGBA8 full-mip GPU estimate | 21,766 / 11,184,828 bytes |
| Six PBR sets disk / RGBA8 full-mip GPU estimate | 1,103,613 / 15,728,616 bytes |
| Total runtime textures, full-mip GPU estimate | 26,913,444 bytes |

The unique-triangle total includes every authored LOD and shared mesh once; it
is not a per-frame rendered triangle count.

## Venue candidate budgets and visibility

The venue reuses the committed rubber, gunmetal and walnut texture sets. It
adds no texture asset and has zero incremental texture residency when the core
PBR package is loaded. Standalone residency for the nine shared references is
6,291,444 bytes with complete RGBA8 mip chains.

| Venue item | LOD0 | LOD1 | LOD2 |
| --- | ---: | ---: | ---: |
| Rendered triangles | 2,376 | 408 | 168 |
| Unbatched draw calls | 34 | 34 | 14 |

The three LODs contain 636 unique authored triangles in total, six material
slots and a 46,056-byte embedded geometry buffer. All venue materials declare
a background target between 2 and 3 EV below the table.

The venue manifest stores eight protected table/lamp/cue anchors. The offline
validator checks rays from all four camera sockets against every venue solid.
For `tactical_overhead`, only nodes tagged `hide_tactical_overhead`—ceiling and
lamp support geometry—are hidden; light emission remains active. This preserves
the approved full-playfield orthographic view without changing simulation.

## Regenerate and validate

```sh
python3 scripts/generate_phase2_modular_gltf.py \
  --output-dir assets/modern/phase2
python3 scripts/validate_phase2_modular_gltf.py \
  assets/modern/phase2/focused_practice_hall_modular_v1.gltf
python3 scripts/validate_phase2_pbr_package.py assets/modern/phase2
python3 scripts/generate_phase2_venue_gltf.py \
  --output-dir assets/modern/phase2
python3 scripts/validate_phase2_venue_gltf.py \
  assets/modern/phase2/focused_practice_hall_venue_v1.gltf
python3 -m unittest \
  tests.visual.test_phase2_modular_gltf \
  tests.visual.test_phase2_pbr_package \
  tests.visual.test_phase2_venue_gltf
```

Fresh generation must be byte-identical for all four candidate files. The
validator rejects external image/buffer substitutions, invalid vertex layout,
LOD marker drift, unfrozen pocket markers, material fallback regressions,
manifest/hash drift and `NO_VISUAL_SOLID` intrusion.
