# Five contact sounds — v3 listening candidate

Status: opt-in candidate; no final sound-content, rights, or shipping approval.

The approved direction was to make cue impact, ball-ball, rail, pocket jaw and
pocket drop less like pitched notifications and closer to dry billiard contact.
The new internal generator uses seeded band-limited noise and short damped,
non-harmonic modes, without a pitch sweep. The modal frequencies are design
parameters, not measured modes of a real ball. Drop includes diminishing,
time-separated cup/net-contact-like pulses. No recordings or downloaded sample
libraries are used or claimed.

New masters are in `assets/audio/phase5-v3-contact-review`. Only those five
masters are resynthesized. UI/foul/match-finish/music/ambience are byte-identical
copies of v2. All v2 files and the existing listening evidence remain intact.
The existing technical gate is unchanged: its SHA is
`c9bdbe929afa868e94e991835f3bd52972d67d5ef9aedb80605c21ec489d1e00`.
All ten v3 files satisfy its measured technical checks; that does not establish
perceptual realism, semantic identification or legal chain-of-title.

## Short old → new comparison

`docs/evidence/20261012-audio-v3-contact-review/old-v2-then-new-v3-five-contacts.wav`
is the actual SDL postmix output, 4.48 seconds, stereo 48 kHz PCM16. Its frozen
SHA is `fd2456409efbcd5272f018a4c86dd7e0f9a05b12bb69b835a2fc54c54a62f70e`.
Each pair uses the same effect-bus gain 0.7. Labels below include trailing silence.

| Pair | v2 old interval (s) | v3 new interval (s) |
| --- | --- | --- |
| Cue / 球杆触球 | 0–0.384000 | 0.384000–0.682667 |
| Ball-ball / 碰球 | 0.682667–1.066667 | 1.066667–1.280000 |
| Rail / 库边 | 1.280000–1.834667 | 1.834667–2.176000 |
| Jaw / 袋口 | 2.176000–2.773333 | 2.773333–3.072000 |
| Drop / 落袋 | 3.072000–3.925333 | 3.925333–4.480000 |

Actual output is from the existing `SdlMixerAudioCueConsumer`, not direct WAV
concatenation: each old/new variant prepares ten SHA-bound masters, queues one
real SDL channel, and records its postmix buffer. Logs bind every segment to
its master SHA. Dummy-device capture makes no speaker sound; Root has separately
presented this frozen clip to the user. Final listening disposition is pending.
The short excerpts are not a five-reviewer blind test or loop acceptance session.

## Explicit product selection

Set `BILLIARDGL_AUDIO_REVIEW_V3_ROOT` to the absolute v3 master directory to use
the candidate through the existing interactive product audio path. V3 takes
precedence if both review-root variables are set. With V3 unset, the existing
V2 opt-in remains available; with neither set, shipping keeps its original
four-WAV policy. Missing or changed masters fail explicitly without alias fallback.

Use the independent CPU audio build's `BilliardsProductAudioCandidateTests
--compare-v3 /absolute/new-comparison.wav` to create another comparison only if
needed. The destination must be new and outside the frozen v2/v3 trees. Both
capture modes now use exclusive creation rather than truncation. The frozen
4.48-second clip should not be regenerated or overwritten.

## Verification and self-review

The original short-contact, selector and SDL-output RED evidence is retained.
Fresh focused verification passes five Python tests (three original plus two
small immutability guards) and the four existing audio CTest targets. No renderer,
shared mainbuild, GPU, CMake target or physics authority change belongs to v3.

Independent source review confirmed the candidate spec, then found an existing
output-truncation risk inherited by the new comparison helper. A temporary
sentinel RED→GREEN test now proves refusal without mutation; `fopen("wbx")`
also prevents a precheck/write race. Another real temporary v2 clone proves the
generator rejects its parent's subtree. The reviewer rechecked only those fixes;
no peer rerun is misrepresented as new suite execution. Source snapshots, both
diffs and all failure logs remain in the evidence directory.

The output guards do not change synthesis. Five current generated sample arrays
were compared with the already-frozen masters and remain exact. The manifest's
generation-source SHA refers to the original generator preserved in
`frozen-after`; `fix-after` preserves the guard-fixed wrapper separately. Masters
and manifest were not rewritten merely to refresh that source hash.

## 2026-10-19 user candidate choice

After being presented with the existing old/new comparison, the user replied
“采用新版 v3 候选” to the question about choosing the five new contact sounds
for the test candidate. The exact disposition and bound hashes are recorded in
`docs/art/m4-audio-v3-candidate-disposition-20261019.json`.
This authorizes the candidate choice; it does not establish five-reviewer blind
identification, both device classes, three-cycle loop review, rights, acceptance
of the unchanged other five cues, or shipping approval. Existing masters,
manifests and default selection are unchanged. Do not ask the user to repeat
this candidate-choice decision.
