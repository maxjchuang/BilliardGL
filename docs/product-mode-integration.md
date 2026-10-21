# Product mode integration gate

`--product-mode` is the opt-in macOS lifecycle and input path. It selects the
modern renderer and creates one shared `GameRuntime` for rendering,
`ProductInputController`, `ProductShellController`, training, replay, and the AI
player. `--settings-path <file>` can override the persistent settings location
for test or managed deployments.

The default remains the legacy interactive lifecycle. Product mode deliberately
rejects automation, standalone screenshot capture, explicit legacy rendering,
and the legacy `--ai-opponent` shortcut. Rendered/headless automation and Phase2
capture therefore do not instantiate the product session or inspect Cocoa input.

Example:

```sh
./Billiards --product-mode
./Billiards --product-mode --settings-path /absolute/path/product-settings.json
```

## Default-switch acceptance gate

Product mode may become the interactive default only after all of the following
are recorded in the release verification matrix:

1. The GPU product-shell draw list is connected to `ModernRenderer` and Main,
   setup, Settings, Help, training, and replay screens have approved screenshots.
2. Native-window manual evidence proves keyboard, scroll, pointer/button,
   modifier, and Retina coordinate delivery; pure adapter, shared-layout
   pointer-session, and macOS app-session E2E remain passing.
3. Single-player playability, AI-turn exclusion, moving-ball exclusion, back,
   quit, settings preview/cancel/apply, and persisted rebind receive manual QA
   sign-off.
4. Phase2 rendered automation, standalone capture, and the 600-frame modern
   benchmark remain unchanged and passing.

The product session now closes CPU/application pointer navigation for all shell
screens, including same-ID left-down/up capture and Settings row activation,
while gameplay screens preserve their prior mouse paths. This milestone does
not claim that the product shell is GPU-rendered or manually playability-signed.
