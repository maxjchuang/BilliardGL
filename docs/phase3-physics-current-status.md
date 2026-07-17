# Phase 3 Physics Current Status

Date: 2026-07-17

Engineering status: **COMPLETED**

Promotion status: **NO_PROMOTED_PHASE3_CANDIDATE**

Authorized production profile: `chinese_pool_legacy_v1`

All eight Phase 3 engineering themes are implemented and covered by production-path,
full-game, deterministic replay, performance, and evidence-governance tests. This
completion does not promote a physics candidate: v1 through v5 each have an
immutable rejection record, and none is authorized as the game default.

The file [phase3-physics-promotion-report.md](phase3-physics-promotion-report.md)
is immutable historical v1 evidence. Its `PASSED_WITH_DECLARED_LIMITATIONS` text
records what the original v1 gate emitted; it is not the current release decision.
The current terminal scientific assessment is
[phase3-v5-physics-assessment-report.md](phase3-v5-physics-assessment-report.md),
which rejects v5 after Cross 2016 failed its preregistered gate and correctly leaves
Han 2005 unexecuted.

Rejected candidates remain committed for deterministic evidence replay and future
successor work. Code must select such a candidate explicitly; the production default
must remain the authorized legacy baseline until a successor passes every promotion
gate. The binding policy is
`physics_models/promotion/phase3_production_default.json`.

The current gate validates both the immutable v5 rejection and the actual executable
default:

```bash
python3 scripts/check_phase3_physics_release.py \
  --root . \
  --executable build/check/Billiards
```

The implementation-plan checkboxes are preserved as historical execution scripts.
Their completion status is recorded by archival notices in each plan and by the
committed implementation, test, freeze, confirmation, rejection, and assessment
artifacts.
