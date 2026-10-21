# Product audio consumption contract

Status: the event-to-consumer path is implemented in pure C++, and the opt-in
modern product application now owns a lazy SDL_mixer consumer beside its shared
authority. Automation, screenshot, windowless, and non-product paths do not
construct that consumer.
No claim is made that the current four WAV files provide a shipping-quality or
fully differentiated sound set.

## Ownership and call order

`ProductAudioController` is the only orchestration layer:

```text
RuntimeEvent + PhysicsStepTelemetry (read-only facts)
        -> AudioEventMapper
        -> AudioMixerModel (current four bus settings)
        -> AudioCueConsumer
        -> explicit ProductAudioResult / ProductAudioSnapshot
```

The owner supplies one `ProductAudioStep` per fixed simulation tick. It must
increment `generation` whenever the source RuntimeEvent sequence is reset. A
retained event journal may be submitted every time: the controller accepts only
new sequence values for the current tick. Numeric ticks may repeat only in a
newer generation. Duplicate ticks, failed-step retries, repeated multi-step
batches, and delayed older generations cannot replay cues.

For a frame that advances several fixed steps, retain each step's telemetry and
call `consumeSteps` in tick order. Passing only the final telemetry discards
physical distinctions and is outside this contract. Consumer failures return a
stable error/status, advance the dedup cursor, and never throw or stop gameplay.

Settings are applied before each consumption call. All four buses are pushed to
the consumer even on a duplicate tick, so volume and mute changes affect active
channels immediately without permitting event replay.

## SDL_mixer behavior

`SdlMixerAudioCueConsumer` lives in `platform_audio.*`. Construction and bus
updates are device-free. The first non-suppressed cue lazily initializes SDL
audio, opens SDL_mixer, and allocates channels. Loaded WAV chunks are cached by
path. Each active channel retains its semantic bus and source gain, allowing a
later bus-volume or mute update to change it immediately.

`AudioConsumerStatus` distinguishes `dormant`, `ready`, `degraded`,
`device_failed`, and `asset_failed`. Device initialization, asset loading, and
channel failures are non-throwing. The game may continue and inspect or log the
status. The legacy wrappers `playBackgroundLoop`, `playHit`, `playBallIn`, and
`playGameOver` remain source-compatible and delegate to the same consumer.

SDL_mixer does not provide native per-channel pitch shifting in this path.
Mapped pitch remains intent metadata only; the consumer does not apply it and
counts non-unity requests as ignored. It never reports pitch as played.

## Honest four-WAV mapping

| Semantic cues | Repository WAV | Status |
|---|---|---|
| cue, ball-ball, rail, pocket jaw | `hit.wav` | `degraded_asset_alias` |
| pocket drop | `ballin.wav` | `distinct_asset` |
| UI, foul, match finish | `GameOver.wav` | `degraded_asset_alias` |
| music, venue ambience | `background.wav` | `degraded_asset_alias` |

Aliases are deliberate fallback, not independent sounds. In particular, the
current UI/foul fallback is not evidence of an authored UI sound family, and
venue ambience is not independently authored.

## Modern product application integration

`GameRuntime` retains a bounded, read-only journal of successful and failed
physics-step telemetry. Its explicit event-journal generation increments before
`reset()` or scenario replacement reuses numeric ticks and event sequences.
This metadata does not alter the solver, state transitions, or authority hash.

`ModernProductSession` optionally accepts an `AudioCueConsumer`. After each
actual published authority step, the modern app pumps every new successful
record in order through mapper, mixer, and consumer. Generation, attempt,
tick, and event-sequence cursors prevent replay after repeated pumps,
multi-step calls, reset, scenario replacement, or AI-owned shots. Isolated
preview steps are retained diagnostically but are not consumed as product audio.

Settings preview, cancel, and apply immediately push the effective four-bus
volume/mute state without waiting for another physics tick. Consumer device or
asset failures remain in `ModernProductSessionSnapshot::audio` and
`audioError`; gameplay and fixed-step simulation continue.

The integration must not infer collisions from rendered positions, synthesize
missing multi-step telemetry, mutate `GameRuntime`, or claim a cue was heard
from mapper/mixer output alone.

## Additional WAV admission gate

No additional WAV was generated in this implementation. A future repository-
original procedural candidate should be treated as non-shipping until technical
art review. Before admission it needs a documented generator/seed and license,
44.1 kHz PCM WAV output, bounded peak/no clipping, intentional head/tail fades,
semantic filename, duration/channel-format checks, deterministic hash tests,
and technical-art approval for timbre, loudness, repetition, and alias removal.

## Verification

### Explicit non-shipping candidate review

The interactive modern product now accepts `BILLIARDGL_AUDIO_REVIEW_V2_ROOT`
as an explicit absolute master-directory opt-in. It selects ten distinct
SHA-bound v2 candidate files and starts independent music/ambience loops via
the existing controller; unset means the shipping four-WAV policy. Missing or
changed candidate masters are rejected without alias fallback. Candidate
content remains unaccepted, and default packaging/Beta dispositions do not
change. The candidate runtime requests 48 kHz and supports bounded loop fades.
See `docs/art/m4-audio-candidate-product-review-20261012.md` for source/rights
limitations, fresh actual-SDL dummy tests, and explicit audition/capture modes.

`BilliardsProductAudioControllerTests` uses only a fake consumer. It verifies
dispatch, generation/tick/sequence deduplication, failed-step and consumer-error
non-replay, multi-step batches, immediate volume/mute changes, explicit device
and asset error states, authority read-only behavior, and the exact alias
policy. The test binary does not link SDL, Cocoa, OpenGL, or open an audio
device. `BilliardsProductAudioFoundationTests` continues to verify mapper and
mixer determinism, storm aggregation, bounds, and settings compatibility.
`BilliardsModernProductAudioSessionE2E` also uses only a fake consumer and does
not link SDL. It verifies real runtime telemetry/event consumption, immediate
settings updates, authority-hash isolation, reset/scenario generation reuse,
multi-step and repeated-pump deduplication, retained failed telemetry, and
non-fatal device failure reporting. The existing rendered modern-application
E2E verifies that automation/capture still completes without product audio
device initialization.
