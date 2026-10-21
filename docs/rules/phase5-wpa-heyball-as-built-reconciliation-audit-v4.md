# WPA Heyball as-built audit v4

Targeted successor for WPA-II-10-4. The v3 audit remains frozen.

- Status: `IMPLEMENTED`
- Product reachable: `true`
- Product surface: current offered solids/stripes GroupChoice parent action only
- Global Rules Decision UI exposed: `false`
- Foundation actions exposed: `false`
- GPU executed: `false`

Core B stages the simultaneous two-group choice and hardened Core D validates ruleset/capability/flow, exact decision/revision/actor/action, canonical payload, frozen authority hash and idempotency. The shipping session exposes only the two actions in the current GroupChoice parent offer while the global Rules Decision UI and foundation actions remain hidden.
