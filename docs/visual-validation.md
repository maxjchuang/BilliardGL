# Visual validation

This document defines the Phase 0 visual evidence path. It is intentionally
renderer-neutral: scenes are configured through public rendered-automation
commands and images come directly from `glReadPixels`, never from a desktop
screen capture.

## Baseline scenes

The versioned registry in `tests/visual/visual_scene_registry.py` contains six
scenes:

| ID | Coverage |
| --- | --- |
| `break` | Complete rack, cue ball, table composition, and room framing |
| `low_aim` | Cue, aiming guide, power HUD, and player-height framing |
| `ball_closeup` | Ball texture, silhouette, highlight, and contact shadow |
| `pocket_closeup` | Corner pocket, jaws, rail, cloth, and local occlusion |
| `tactical_overhead` | Full playfield and tactical readability |
| `post_shot_observe` | Deterministic frame after the first break collision |

Every recipe starts in its own newly launched rendered process, pins the
runtime viewport to 1024x768, and applies only named automation commands. The
process boundary is currently important: legacy resource initialization stores
ball texture handles in `GameState`, while `reset_game` clears those handles.
Using a fresh initialized process provides a clean deterministic state without
discarding the presentation resources. This coupling should disappear when
renderer resources move out of simulation state.

The moving scene advances fixed simulation ticks using the existing
interactive production profile; it does not alter any profile parameter.
Registry version, profile identity, reserved random seed, camera state, state
hash, image hash, executable hash, Git revision, pixel format, and broad
interest-region measurements are written to `manifest.json`.

## Local capture

Build the game, then run:

```sh
python3 scripts/capture_visual_baselines.py \
  --executable build/check/Billiards \
  --output /tmp/billiardgl-visual-baselines \
  --width 1920 --height 1080 \
  --verify-repeatability
```

Validated capture profiles are 1024x768 and 1920x1080. The output contains six
RGB8 PPM files and a manifest. `--verify-repeatability`
launches a clean rendered process twice per scene and requires byte-identical
pixels and authoritative state on that machine. This strict check is intended
for local/reference-host evidence. Cross-GPU CI should eventually use broad
regions plus a tolerant image metric, because legacy OpenGL rasterization can
differ between drivers.

Artifacts must remain in `/tmp`, a build directory, or another ignored output
directory. Ordinary screenshots are ignored by Git and must not be committed.
Reviewed goldens will require a separate governance change before the first
files are added under `tests/visual/baselines/`.

## Tests

The registry has a standard-library-only unit test:

```sh
python3 -m unittest tests.visual.test_visual_scene_registry
```

The rendered end-to-end check accepts the built game executable:

```sh
python3 tests/e2e/test_visual_baselines.py build/check/Billiards
```

The end-to-end check runs both validated resolutions. It verifies all six files,
PPM dimensions and exact `width * height * 3` payload length, visible pixels,
interest regions, scene/state uniqueness, declared physics profile, and
same-host repeatability. It also runs standalone screenshot mode twice at each
resolution and requires byte-identical output.

## Offscreen capture and current limits

Screenshot rendering uses a texture-backed framebuffer object with a depth
renderbuffer. The FBO is bound before camera setup and clearing, its viewport is
the requested output size, and pixels are read from `GL_COLOR_ATTACHMENT0`.
The previous framebuffer, renderbuffer, texture, read/draw buffers, and viewport
are restored after capture. This makes rendered-automation `resize` independent
of the native GLUT drawable and lets standalone capture use a small context
window even when the requested image is larger than the current monitor.

The capture tool accepts only the two profiles exercised by E2E: 1024x768 and
1920x1080. Additional profiles should be admitted only with the same header,
payload, visible-region, and repeatability evidence. The remaining Phase 0
performance work is to record stable CPU frame time, GPU frame time, draw calls,
triangle count, and estimated video memory on the named reference machine.

The legacy renderer also does not explicitly define an sRGB framebuffer or
linear-light workflow. For that reason the manifest calls the color space
`legacy framebuffer (unmanaged; Phase 0 baseline)` rather than claiming sRGB.
This label must change only after the modern renderer color pipeline is
validated.
