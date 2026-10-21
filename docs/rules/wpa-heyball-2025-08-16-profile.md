# WPA Heyball 2025-08-16 rules profile

Product ruleset ID: `wpa_heyball_2025_08_16_v1`

Normative source: [WPA Rules of Heyball, effective 2025-08-16](https://wpapool.com/wp-content/uploads/2025/08/250816-Rules-of-Heyball.pdf).

The first implementation slice deliberately separates facts reported by physics from
rule interpretation. `ShotFacts` contains first contact, cushion and pocket facts;
`ChineseEightBallRules` returns machine-readable reason codes, HUD text keys, turn,
ball-in-hand, group and rack outcome.

Covered in the first scenario table:

- legal and illegal break thresholds (four object balls to cushions or one pocketed),
- break-foul head-string restriction and explicit opponent decision options,
- 8-ball on the break as spot-or-rerack rather than a win or loss,
- open-table group assignment only after the break,
- wrong first contact, no cushion after contact, scratch and balls off table,
- legal and illegal 8-ball outcomes,
- no called pocket requirement for this profile.

Still required before the profile is exposed as a complete product ruleset:

- shot-fact collection from authoritative contact telemetry,
- simultaneous legal/illegal first contact and simultaneous group-choice interaction,
- frozen-ball, double-hit, push, jump and player/referee procedural facts,
- break and stalemate choice UI plus re-spot/re-rack execution,
- end-to-end rack tests and HUD localization strings.
