# Product audio event and mixer foundation

Status: implemented foundation; platform playback integration and production
audio asset selection are intentionally deferred.

## Decision

Shipping audio is split into two deterministic, headless-safe stages:

1. `AudioEventMapper` reads a tick's immutable `PhysicsStepTelemetry` and
   `RuntimeEvent` values and emits typed `AudioCue` requests.
2. `AudioMixerModel` applies the current product settings to the music,
   ambience, effects, and UI buses and emits `MixedAudioCue` requests.

Neither stage owns or mutates `GameRuntime`, `GameState`, physics telemetry, or
runtime events. Neither stage opens an audio device, loads a WAV, schedules a
channel, or claims playback. Every emitted cue is explicitly
`AudioDeliveryStatus::CueOnly`. `AudioCueConsumer` is the seam for a future
platform player; receipt and actual playback status must be reported by that
future layer rather than inferred by the model.

This replaces no legacy SDL path yet and does not modify the modern application
or renderer. No external audio asset is downloaded or introduced by this
foundation.

## Physical event mapping

The mapper distinguishes:

- ball-to-ball contact;
- straight rail contact;
- pocket-jaw contact;
- pocket capture/drop;
- cue impact runtime events;
- foul and UI feedback runtime events.

For physical contacts, finite non-negative normal impulse is mapped through a
bounded square-root curve to gain and pitch. Finite extreme values clamp at the
configured maximum. Negative, NaN, and infinite impulses are ignored. Pocket
capture and payload-free runtime events use a documented bounded fallback
impulse because their current source records contain no collision impulse.

The result is deterministic for the same facts even if contact input order
changes. Events from other ticks are ignored, and failed physics steps do not
emit contact cues.

## Same-tick protection

Contacts aggregate by cue kind within one tick. The strongest valid impulse
sets gain and pitch; impulses are never summed. `aggregateCount` preserves
diagnostic visibility into duplicate/contact-storm input. A pocket-jaw fact
consumes the matching same-ball/same-time rail fact so one jaw impact does not
produce two sounds. Runtime collision and pocket events are fallbacks when a
more precise physics fact already exists.

The mapper emits at most seven categories with the default configuration, so a
single solver tick cannot create an unbounded playback request burst.

## Product settings and buses

`ProductSettings` now persists four volumes and four independent mute flags:

| Bus | Volume | Mute |
| --- | --- | --- |
| Music | `musicVolume` | `musicMuted` |
| Venue ambience | `ambienceVolume` | `ambienceMuted` |
| Physical/game effects | `effectsVolume` | `effectsMuted` |
| UI/foul feedback | `uiVolume` | `uiMuted` |

Older version-1 settings files without the UI or mute keys remain valid and
receive safe defaults. Invalid/non-finite volumes are rejected by settings
validation; the mixer additionally clamps unvalidated runtime inputs. Calling
`applySettings` changes the next `mix` result immediately and has no device
restart side effect.

`persistentCues` exposes separate music and venue-ambience loop requests so a
future player can stop or attenuate either bus independently. It still does not
claim that a loop exists or is playing.

## Verification

`BilliardsProductAudioFoundationTests` is a CPU-only target with no SDL or audio
device dependency. It verifies:

- deterministic event output and stable ordering;
- strongest-impulse aggregation for a 500-contact same-tick storm;
- ball, rail, pocket-jaw, pocket-drop, cue-impact, foul, and UI distinction;
- bounded extreme handling and non-finite rejection;
- jaw/rail de-duplication and runtime/physics fallback de-duplication;
- independent music, ambience, effects, and UI volume/mute application;
- immediate settings changes and backwards-compatible version-1 parsing;
- cue-only delivery state;
- unchanged authoritative game-state hash, runtime tick, and event count.

Build and run:

```sh
cmake --build build --target BilliardsProductAudioFoundationTests
ctest --test-dir build --output-on-failure -R BilliardsProductAudioFoundationTests
```

## Future platform integration contract

A platform player should consume `MixedAudioCue` values after simulation and
must never feed timing, availability, channel count, or playback completion
back into physics or rules. Missing assets and unavailable devices should
produce explicit backend diagnostics while leaving the cue request observable;
they must not be reported as successful playback.
