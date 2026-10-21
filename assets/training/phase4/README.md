# BilliardGL Phase 4 首发训练包

本目录是原创的 `training_scenario_v1` 内容合同，不包含竞品或第三方资产。12 关都显式绑定发布用 `chinese_pool_interactive_120hz_v5`、`production_table` 与 120 Hz 固定步长。

- `training_pack_v1.json`：12 个练习的总清单、文件哈希、确定性合同和坏输入索引。
- `scenarios/`：每个练习一个独立 manifest。
- `coverage_matrix.json`：分类、难度、能力门控和 success predicate 覆盖。
- `bad_inputs/`：5 个必须失败的固定样例，每个声明稳定错误码。
- `replays/`：12 个由真实固定步长运行和 Replay Recorder 两遍生成、自动校验成功条件的参考回放。
- `reference_generation.json`：生成回放所用发布构建、编译器、编译参数与二进制 SHA-256；终点哈希只承诺在匹配该构建配置时复验。

重新生成及验证：

```bash
python3 scripts/generate_phase4_reference_replays.py \
  --binary <fresh-Billiards> \
  --audit-binary <fresh-BilliardsPhase4ProductControllerTests> \
  --build-type Release \
  --compiler /usr/bin/c++ \
  --compiler-flags '-O3 -DNDEBUG'
python3 scripts/validate_phase4_training_pack.py assets/training/phase4/training_pack_v1.json
python3 -m unittest tests.training.test_phase4_training_pack
```

跟杆、拉杆和左右塞仍遵守 capability gate；发布产品已提供对应 authoritative tip-offset capability，缺少能力的调用端保持 `visible_locked_no_score`。`cue_elevation` 与 `chalk` 在全部首发练习中明确禁止。参考回放的 `automated_verified` 只声明真实物理与成功谓词已自动复验，不替代 12 关人工试玩记录；人工状态继续保持 `NOT_RUN`。
