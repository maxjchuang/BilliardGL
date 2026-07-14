# Reference Physics Validation Report

Build: sha256:6ff041120e032ee8ea18c03c92ca82352e98bdff6e9569f883f4d70d1e558fbc

## CALIBRATION

| Point | Case | Incident | Domain | Metric | Prediction | Experiment | Interval | Status |
|---|---|---:|---|---|---:|---:|---|---|
| - | - | - | - | - | - | - | - | - |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| - | 0 | - | - | - |

## HOLDOUT

| Point | Case | Incident | Domain | Metric | Prediction | Experiment | Interval | Status |
|---|---|---:|---|---|---:|---:|---|---|
| chain_passivity_right | chain_right | - | - | solver_energy_growth_j | 0.0 | 0.0 | [-1e-09, 1e-09] | PASSED |
| line_toi_right | line_toi_right | - | - | solver_first_toi_seconds | 0.0428499984741211 | 0.04285 | [0.042849, 0.042851] | PASSED |
| overlap_limit_right | overlap_right | - | - | solver_penetration_limit_count | 2 | 2.0 | [2.0, 2.0] | PASSED |
| symmetric_residual_right | symmetric_right | - | - | solver_max_residual_cm_s | 0.0002899169921875 | 0.0 | [-0.001, 0.001] | PASSED |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| chain_right | 1 | 0.0 | 0.0 | 1.0 |
| line_toi_right | 1 | 1.5258789023975261e-09 | 1.5258789023975261e-09 | 1.0 |
| overlap_right | 1 | 0.0 | 0.0 | 1.0 |
| symmetric_right | 1 | 0.0002899169921875 | 0.0002899169921875 | 1.0 |

## Failure accounting

- Known model mismatches: none
- New model mismatches: none
- Missing model mismatches: none
- Known reference limitations: none
- New reference limitations: none
- Missing reference limitations: none
- Unallowlistable failures: none

## Reference limitation details

None.
