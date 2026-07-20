# Phase 3 Full-Game Physics Promotion Report

Status: **PASSED_WITH_DECLARED_LIMITATIONS**

Production profile: `chinese_pool_full_game_v1`  
Source revision: `1f9023cc12b140455e35157baff0aa2f20f95fdd`  
Executable SHA-256: `adb9af7faa66bbf8034f22f7050f2c280d1dd380a6c813925527f75eb3fe7d61`

## Hard gates

- candidate_inventory: PASSED
- dual_path_equivalence: PASSED
- full_game_stress: PASSED
- golden_governance: PASSED
- performance_budget: PASSED
- unexplained_regressions: 0

## Stress and performance

- Stress rows: 12
- Repeated breaks represented: 36
- Maximum penetration: 0.07612295626979293 cm
- Maximum residual: 0.00099917615771029714 cm/s
- Mean/p95/p99 step: 0.078449 / 0.083667 / 0.088708 ms

## Evidence boundary

Reality goldens, analytic goldens, and behavior snapshots remain distinct. A
passing engineering release does not erase preserved public-experiment
mismatches or upgrade behavior snapshots to real-world validation.

- cloth transfer and long-horizon spin decay remain incompletely measured
- elevated cue, jump, masse, and measured miscue boundaries remain unavailable
- material transfer and complete spin measurements remain limited
- Chinese Pool rail material transfer and general spin incidence remain limited
- real pocket trajectory and pot-success measurements remain unavailable
- real break-shot and chaotic long-trajectory evidence remain unavailable

## Replay

- `ctest --test-dir build/check --output-on-failure`
- `build/check/BilliardsFullGameStress --write /tmp/full_game_stress.csv`
- `build/check/BilliardsFullGamePerformance --write /tmp/full_game_performance.json`
- `python3 -m unittest discover -s tests/physics_validation -p 'test_*.py'`
