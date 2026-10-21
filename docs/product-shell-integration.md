# Product shell and shipping screen integration contract

Status: pure C++ controller, view model, draw-list, shared pointer hit layout,
and opt-in Cocoa product-session input are implemented. GPU submission, native
file-picker presentation, and human playability validation remain integration
work.

## Single application boundary

The platform application owns one `ProductShellController` alongside the
existing authority and product controllers:

```text
native event / file picker result
        |
        v
ProductShellController
        |-- ProductInputController
        |-- ProductSettingsRuntime / SettingsMenuModel
        |-- TrainingProductController
        |-- ReplayTransportModel
        |-- PlayerControl
        v
ProductShellSnapshot
        v
ShippingScreenDrawListBuilder + frozen SDF atlas
        v
ShellDrawList (panels, lines, icons, glyph quads)
```

The application submits normalized play/settings/replay events through
`dispatchProductInput`, semantic shell focus through `navigate`, and a selected
local replay path through `loadReplayPath`. It responds only to explicit
`ProductShellAppRequest` values:

- `Quit`: request normal application shutdown;
- `ChooseReplayFile`: show the native file picker, then pass its result back to
  `loadReplayPath`;
- `None`: no platform action.

There is no Cocoa, renderer, native key code, file-picker, or GPU type in the
shell API.

## Screen state

The controller has nine explicit screens:

- `MainMenu`;
- `SinglePlayerSetup`;
- `Match`;
- `TrainingCatalog`;
- `TrainingSession`;
- `ReplayBrowser`;
- `ReplayPlayback`;
- `Settings`;
- `Help`.

Every snapshot includes a stable screen ID, breadcrumb, title, subtitle,
focusable items, visible focus state, footer help, and explicit error text.
Back transitions are owned by the shell; gameplay mutations are not.

Every screen transition also synchronizes the `ProductInputController` surface:
training screens use `Training`, replay browser/playback use `Replay`, and
main/setup/match use `Play`. Settings and Help preserve their return surface;
the settings modal context temporarily takes priority. Keyboard focus always
lands on and moves between enabled items, skipping informational or disabled
cards.

Pointer navigation uses `ShellScreenLayout`, the same physical-pixel geometry
source consumed by `ShippingScreenDrawListBuilder`; there is no second set of
hard-coded click coordinates. Cocoa converts its bottom-left view coordinates
to canonical top-left logical coordinates, while logical-to-physical viewport
ratios handle 1x and Retina backing sizes. Move updates hover and exact semantic
focus. Left down captures one enabled ID, and left up activates only when it is
still over that same ID. Blank space, disabled rows, right click, and a release
after moving out cannot activate. Activation delegates to the existing
`ShellNavigationCommand::Activate`, including supported Settings row toggles
and adjustments. Match, TrainingSession, and ReplayPlayback bypass shell hit
testing and retain their existing gameplay/replay input paths.

## Approved product entry

`approvedRules()` contains exactly
`wpa_heyball_2025_08_16_v1 / WPA Heyball`. Main menu and single-player setup do
not enumerate legacy or unapproved rules. Rookie, Club, and Expert are the only
AI choices. Starting a match passes the selected configuration to
`PlayerControl::configure` only after resetting `GameRuntime` and explicitly
enabling `ChineseEightBallV1`. This order is deterministic even when the prior
screen held a moving match, active training attempt, or loaded replay. A failed
AI configuration resets authority again and returns to setup with a stable
visible error. Leaving replay playback pauses its isolated transport before the
next screen takes control; the shell does not implement AI logic independently.

## Controller delegation

- Training pack/catalog/session requests use `TrainingProductController`; its
  12 cards and lifecycle snapshot are presented without recalculating score or
  scenario validity. Main-menu availability and card count come from that live
  snapshot, so an unloaded pack is disabled and never advertised as 12 usable
  drills. `abandonAttempt` closes an active training lifecycle before a new
  match takes authority.
- Replay parsing uses the public Replay v1 parser, then delegates validation and
  playback to `ReplayTransportModel`. Pause, fixed-policy rate, camera, step,
  and seek values are read from the transport snapshot.
- Settings values come from `ProductSettingsRuntime::current` and transaction
  preview. Saved and changed values remain visibly distinct. Rebind capture,
  apply, cancel, and stable errors remain owned by the settings/input layers.
- Match controls go through `ProductInputController` and `GameRuntime`; shell
  cards never edit shot, camera, rules, or physics state.

Empty, unreadable, malformed, incompatible, and valid replay paths have
different visible outcomes. A failure remains on `ReplayBrowser`; it cannot be
presented as successful playback.

## Shipping draw-list contract

`ShippingScreenDrawListBuilder` consumes only a snapshot, viewport, and frozen
`SdfFontAtlas`. The viewport specifies logical and physical dimensions and DPI;
the snapshot carries the live or settings-preview UI scale. Supported UI scale
is 85% through 135%; values outside that range fail explicitly.

The output reuses `HudDrawList`, `UiPrimitive`, and `TextRunMetrics`. It contains
only CPU-side render requests:

- background/header/footer and item panels;
- SDF glyph quads and measured/clipped text;
- semantic shape icons for every item;
- four-line focus outlines plus a focus chevron;
- selected-state check shape;
- error text plus an octagonal error/cross icon.

Color is never the sole carrier of focus, selection, disabled state, or errors.
All primitives are clipped to a physical safe rectangle. Grid columns are
chosen from available height so 12 drills and eight audio settings do not
overlap at 1024x768 or 135% UI scale. High-DPI geometry derives from the
physical/logical ratio. No glyph rasterization is permitted during an ordinary
build.

`buildShippingScreenLayout` is independently usable without a font atlas, GPU,
window, or input device. Its stable-ID regions are copied into the draw list,
so CPU tests can prove rendered and hittable rectangles remain identical.

The draw list is not a GPU screenshot and does not prove the renderer submitted
it. A renderer must explicitly consume `ShellDrawList::draw.primitives`, bind
the matching atlas cache key, and report its own failures.

## Verification boundary

`BilliardsProductShellTests` validates:

- all nine screen states and representative back/activate transitions;
- keyboard-only Main-to-match, Main-to-training, Replay-to-Back, and Settings
  Apply/Cancel paths, including disabled-item focus skipping;
- deterministic match restart from moving match, training, and replay states,
  plus clean authority recovery after AI configuration failure;
- exactly one approved WPA Heyball entry and three AI levels;
- 12 training cards and training-session controls;
- settings saved/preview/rebind/error states;
- empty, missing, malformed, and valid Replay v1 paths;
- replay pause/rate/camera/seek snapshot updates without match-authority
  mutation;
- 108 layout combinations: nine screens, four viewport/DPI cases, and three UI
  scales (85%, 100%, 135%);
- safe bounds, non-overlapping item rectangles, visible keyboard focus,
  text/icon/shape redundancy, and zero per-frame rasterization.

The test is headless. It does not claim native event integration, GPU pixels,
screenshots, audio playback, or human playability.

`BilliardsModernProductPointerSessionE2E` additionally validates Main, setup,
Training catalog/session, Replay browser/playback, Help, and Settings pointer
routing at 1280x720, 1920x1080, and 1280x720 logical / 2560x1440 Retina. It also
covers inclusive/exclusive edges, blank safe area, disabled rows, wrong button,
press-then-move-out cancellation, settings preview changes, and gameplay-screen
non-interception. `BilliardsModernApplicationE2E` exercises the same Retina
pointer activation through the macOS product-session compilation boundary.
These are CPU/application-input results; they do not claim that product UI
primitives have been submitted by the GPU renderer.
