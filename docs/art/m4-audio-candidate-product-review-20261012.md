# M4 audio candidate product review — 2026-10-12

Status: **OPT-IN REVIEW IMPLEMENTED / NOT CONTENT ACCEPTED**. Shipping defaults and Beta acceptance are unchanged.

## What changed

The interactive modern product can explicitly select the existing Phase-5 v2
candidate masters through `BILLIARDGL_AUDIO_REVIEW_V2_ROOT`. The value must be
the absolute directory of `assets/audio/phase5-v2-technical-correction`.
No environment variable means the existing shipping four-WAV alias policy.
Legacy wrappers also keep their existing default policy.

The candidate policy maps all ten semantics one-to-one to their frozen masters:
cue impact, ball-ball, rail, pocket jaw, pocket drop, UI feedback, foul, match
finish, music loop, and venue ambience. Candidate status explicitly says
`candidate_review_not_content_accepted`; it is not represented as accepted
shipping content. Missing resources, a relative/empty review root, or a master
SHA mismatch produce explicit non-fatal asset errors. There is no alias fallback
or alternate-directory search for this pack.

The same `ProductAudioController` receives read-only runtime facts and current
settings; the same SDL consumer performs actual WAV decoding and queueing.
Impulse gain, per-tick aggregation, sequence deduplication, unsupported pitch
reporting, and physics authority ownership remain intact. The product owner
explicitly starts persistent loops only for the review pack after preparation.
Music and ambience have separate decoded content and separate bus gain/mute.
An initially muted loop can start when unmuted without restarting the game.

The candidate device requests stereo 48 kHz. SDL converts the 48 kHz PCM24
masters to its actual signed PCM16 runtime format; the runtime derivative is
bound to each master SHA rather than being mislabelled a PCM24 master. Candidate
loops use a 120 ms fade-in. Bus changes during fade retire/requeue the loop with
its new gain so SDL's fade envelope cannot overwrite the control. Explicit
audition exit supports bounded fade-out; ordinary destruction still halts audio
and releases SDL resources. No audio input can modify physics state.

## Source and rights disposition

The reused files and generators are exactly those in
`docs/art/m4-audio-release-handoff-20260923.md`. The v1 generator uses parameterized
oscillators/envelopes and seeded integer noise, without loading external audio.
The v2 generator changes only the two loop PCM gains; eight one-shots are
byte-identical to v1. Runtime verifies all ten frozen file SHA values before
decoding. Manifest SHA:
`ca2267ac9ad2d020f0bbb1cbe935ed544c4f6b0875ffd53e101108bffaadcc10`.

This is technical source traceability, **not** a signed authorship, license, or
chain-of-title certification. No rights holder, human reviewer, device-listening
session, or content acceptance has been invented.

Original plan §13.3 requires impulse mapping, audible semantic distinction,
same-tick overload prevention, independently disableable venue ambience, and
one-way consumption of physical facts. It does not specify five reviewers.
The existing `phase5-audio-content-remediation-design-gate-v1.json` separately
records a five-reviewer randomized blind-test protocol, two playback classes,
three-cycle loop listening, and rights disposition. Those records are preserved
as existing design requirements, not silently added as new user hard gates.
Root must determine the content acceptance arrangement before shipping/Beta
promotion. A measured candidate or decoded PCM is not proof it sounds right.

## Listen or capture without opening a graphics window

`build/m4-audio-review-20261012/BilliardsProductAudioCandidateTests --audition`
uses the actual selected SDL audio device and plays eight labelled one-shots,
followed by four-second music-only and ambience-only excerpts. Run it only when
the reviewer wants audible output. It is not a randomized blind test or a
three-consecutive-cycle loop assessment.

`BilliardsProductAudioCandidateTests --capture /absolute/review.wav` uses SDL's
dummy output device and saves the actual SDL postmix PCM for the same sequence.
It generates no speaker sound. The resulting playable WAV can be reviewed later;
it is a runtime preview, not a replacement master or a content acceptance claim.

The product application review opt-in is scoped to its ordinary interactive
audio consumer. Automation/capture/windowless paths still do not construct that
consumer. Candidate assets are not added to default shipping packaging. To test
an installed/package build, provide the explicit candidate root; absence is an
honest missing-resource error rather than an implicit promotion.

## Verification boundary

Fresh CPU tests cover actual SDL dummy-device decoding/queueing of all ten
SHA-bound masters, independent loop content and controls, fade-in/out, initially
muted-loop recovery, gain changes during fade, physical-contact impulse gain,
duplicate-tick non-replay, explicit missing/SHA errors, and unchanged authority
hash. Existing foundation/controller/session tests still use their original
assertions; session consumption remains covered with real runtime telemetry and
an injected fake consumer. The modern application source is syntax-checked,
not GPU-executed for this audio change.

Evidence is in `docs/evidence/20261012-audio-candidate-product-v1`. Initial test
RED is retained: it incorrectly demanded a fade state for a zero-volume muted
loop. The corrected fade test unmutes both loops before checking the audible
transition and additionally covers mute/recovery during fade. No sound-quality
or clipping threshold was relaxed.
