# M4 audio release handoff — 2026-09-23

Status: NOT CONTENT ACCEPTED; shipping defaults unchanged.

## Verified technical provenance

The inspected v1 generator synthesizes eight one-shots from parameterized sine
oscillators, envelopes and seeded integer noise, and two stereo loops from
explicit oscillator sums. Its synthesis path does not load external audio.
This source inspection is not a legal ownership or chain-of-title certification.
The v2 correction transforms only the two loop PCM data chunks with constant
gain; the eight one-shots must remain byte-identical to v1.

Frozen SHA-256 references:

| Artifact | SHA-256 |
| --- | --- |
| `scripts/generate_phase5_audio_content_candidate.py` | `7c2b7afa98cb070e4a5232342c7107f2d0216b3a020a8c4f4e2761aa959ae9ce` |
| `scripts/generate_phase5_audio_v2_technical_correction.py` | `15c0aa5d87e48af7069e62dfbe2425481566987e49b019bca29a75e904b30b4b` |
| `assets/audio/phase5-v2-technical-correction/phase5-audio-v2-technical-correction-candidate-manifest-v1.json` | `ca2267ac9ad2d020f0bbb1cbe935ed544c4f6b0875ffd53e101108bffaadcc10` |
| `docs/art/phase5-audio-v2-technical-correction-machine-measurement-v1/report.json` | `b4153d5b31f5e359428aa68c9ef3fa6233e1aea0d2bfc3a6b3484803ac255f11` |

The machine report states PASS_TECHNICAL_THRESHOLDS_ONLY. Device playback,
human listening, rights verification and content acceptance remain false.
Existing Release AudioFoundation, AudioController and AudioSession tests were
rerun: 3/3 passed, 0.06 seconds. These were existing binaries, not a fresh build,
and the session test uses an injected fake audio consumer, not audible output.

## Remaining acceptance, preserving the existing design gate

1. Review authorship/source and obtain the required ownership disposition.
2. Play the exact candidate masters on reference headphones and representative
   integrated speakers. Record device, candidate hashes, reviewer and result.
3. Independently listen to three consecutive cycles of both loops; assess seam,
   distortion and perceptual quality rather than just sample continuity.
4. Perform the existing five-reviewer randomized semantic blind test: three
   repetitions per cue per reviewer, overall identification >=80%, each cue
   >=70%, pairwise confusion <=25%. Preserve raw responses and randomization.
5. Record content disposition for all ten cues. Do not infer acceptance from
   different WAV hashes or technical loudness measurements.
6. After acceptance, update AudioAssetPolicy, packaging and the Beta contract
   together, then verify actual playback, independent music/ambience controls,
   impact gain, missing-resource behavior and no authority-state mutation.

Until then BallBall/Rail/PocketJaw still alias `hit.wav` in the shipping policy;
MusicLoop/VenueAmbience still alias `background.wav`. The plan's audible
distinction requirement is not closed. This handoff adds no new acceptance
thresholds and does not mark any participant or listening session completed.
