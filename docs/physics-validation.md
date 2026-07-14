# BilliardGL 物理验证

物理验证系统以 WPA 美式 Pool 为器材基准，通过固定物理 tick 记录生产引擎状态。当前阶段验证解析不变量和已知数值缺陷；论文数据接入、参数标定和旋转模型将在后续独立阶段完成。

## 运行

完整仓库检查会构建游戏、运行非 rendered 测试，并把持久报告写入构建目录：

```bash
./scripts/check.sh
```

单独运行物理验证：

```bash
python3 -m tools.physics_validation.run \
  --executable build/check/Billiards \
  --scenarios tests/physics_validation/scenarios \
  --known-failures tests/physics_validation/known_failures.json \
  --output build/check/physics-validation-report
```

重放单个场景时，`--scenarios` 也可以直接指向一个 JSON 文件；known-failure 清单会自动限定到该场景：

```bash
python3 -m tools.physics_validation.run \
  --executable build/check/Billiards \
  --scenarios tests/physics_validation/scenarios/high_speed_tunneling_v1.json \
  --known-failures tests/physics_validation/known_failures.json \
  --output build/check/physics-validation-report/high-speed-tunneling
```

该命令的原始数据写入 `build/check/physics-validation-report/high-speed-tunneling/traces/high_speed_tunneling_v1.json`。

输出包括：

- `report.json`：机器可读场景结果、构建 SHA-256、场景来源、expectation/容差、原始 trace 路径、指标、失败分类和 known-failure 对账。
- `report.md`：按场景名排序的人工摘要。
- `traces/<scenario-id>.json`：第一次确定性运行的完整逐 tick 原始数据。

每个场景会在两个全新 headless 进程中运行。相同构建的序列化 trace 必须完全相同；跳 tick、丢帧、协议错误或进程退出会标记为 `INTEGRATION_MISMATCH`。

## 数据与单位

trace 由 `GameRuntime` 在每次生产 `updatePhysics` 完成后记录，不使用渲染帧时钟。字段单位如下：

| 数据 | 单位 |
|---|---|
| 位置、穿透深度 | cm |
| 线速度 | cm/s |
| 线加速度 | cm/s² |
| 角速度 | rad/s |
| 线动量 | kg·m/s |
| 冲量 | N·s |
| 平动动能 | J |

当前模型没有真实旋转动力学，`angular_velocity_rad_s` 是权威预留状态而非视觉旋转反推值。未由场景设置时保持为零。

runtime 最多保留 10000 帧。协议通过 `get_physics_trace {after_tick,limit}` 分页查询，每页最多 1000 帧。任何 `dropped_frames > 0` 都使验收失败。

## 场景 schema v1

规范场景位于 `tests/physics_validation/scenarios/`，必须声明：

- `schema_version: 1`；
- 唯一 `id` 和描述；
- `evidence.grade` 为 `A`、`B` 或 `C`；
- `evidence.equipment` 必须为 `WPA_POOL`；
- 固定 `time_step_seconds: 0.1` 和 1–1000000 个 tick；
- 活跃球的位置、线速度、三维角速度和落袋状态；
- 使用 `eq`、`lte` 或 `gte` 的验收指标。

未列出的球被原子地设置为静止和落袋。解析失败不会部分修改游戏状态。底层 C++ 测试和进程 E2E 使用相同 JSON 文件。

场景 `id` 必须是安全且唯一的文件名组成部分。涉及球间距的 fixture 使用 WPA Pool 的 `5.715 cm` 球直径和 `2.8575 cm` 半径；修改 `table_specs` 中的 WPA 几何后，必须同步重新生成或更新这些 literal fixture，并在 review 中核对差异。

`permutation_invariance` 会从 `value.index_map` 生成第二套索引置换场景。两套布局各运行两次，先验证各自确定性，再按物理身份映射比较最终速度；报告同时保存原始和置换 trace。

## Cue-impact schema v2

schema v2 保留 v1 的球、仿真与 expectation 字段，并增加必填 `cue_impact` 输入契约。解析器验证 cue ball 唯一且活跃、cue 速度/质量有限且在范围内、方向为单位向量、仰角合法、tip offset 位于球面内，并以生产球半径 `2.8575 cm` 验证两套 offset 表示一致。解析失败不改变 runtime。

v2 当前只解决实验输入的无损表达与 state/trace 追踪，不添加 cue-contact 公式。没有独立机械证据时，物理 cue 速度不能拟合成 shot power；竖直 tip offset 也不能静默降级为中心击球。此类点必须报告 `REFERENCE_LIMITATION`。当将来输入可执行而引擎给出有限但错误的速度或自旋时，才报告 `MODEL_MISMATCH`。

证据等级含义：

- A：包含可复现实验条件、测量精度和逐点数据，可直接验收轨迹或速度。
- B：模型经过真实实验对照但原始数据不完整，用于派生指标和趋势。
- C：解析约束、WPA 几何或定性证据，不能独立证明数值符合真实实验。

## 失败分类

- `NUMERICAL_FAILURE`：非有限状态、漏碰、错误冲量、穿透或能量异常。
- `MODEL_MISMATCH`：稳定结果超出公开实验容差。
- `NON_DETERMINISTIC`：相同输入不复现，或结果依赖球编号/遍历顺序。
- `INTEGRATION_MISMATCH`：runtime 与进程接口、tick 或 trace 不一致。
- `REFERENCE_LIMITATION`：缺少足以验收该指标的公开证据；不计为通过。

## 已知失败规则

`tests/physics_validation/known_failures.json` 不是跳过列表。报告始终把其中场景显示为 `FAILED (KNOWN)`。CI 要求实际 `(scenario_id, code, metric)` 集合与清单完全一致：

- 新失败会导致 CI 失败；
- 已知失败意外消失也会导致 CI 失败，要求先确认修复有效；
- 不允许通过扩大容差让失败消失。

修复一个缺陷时：

1. 单独重放对应场景并保存修复前报告。
2. 按 TDD 修改物理模型，让场景从 `FAILED (KNOWN)` 变为真实通过。
3. 运行完整 `./scripts/check.sh`，此时 CI 应因“missing known failure”而失败。
4. 人工确认指标、trace 和相关回归后，删除清单中的对应项。
5. 再次运行完整检查并提交修复、测试和清单删除。

当前三个明确缺陷是：分离中的重叠球仍收到冲量、高速球离散穿过目标球，以及多球碰撞依赖球编号遍历顺序。

## 论文数据接入边界

公开实验数据进入仓库前，必须记录来源、DOI/链接、图表或页码、器材、许可、单位转换、测量误差和数字化误差，并预先划分标定集与盲测集。没有这些元数据的 A/B 场景必须报告 `REFERENCE_LIMITATION`，不得降低为通过。
# Multi-contact solver v1 evidence boundary

The production multi-contact solver is frozen against a repository-authored
Grade-C analytic contract for swept-sphere TOI, simultaneous-contact symmetry,
impact passivity, and explicit numerical limits, plus a committed deterministic
stress matrix. This establishes reproducible engineering invariants only. No
real break-shot experiment or sufficiently long chaotic trajectory dataset is
currently available, so the project does not claim experimental validation of
full-rack dispersion or long-horizon trajectory accuracy.

The frozen HOLDOUT was executed exactly once on 2026-07-14: 4/4 points passed
with no accounted failure. All 12 validation files (four traces, four provenance
records, point table, JSON/Markdown reports, and receipt) are committed. The
immutable hashes are freeze
`360c85919e3167c3a86351068cdb147b02974ff336b019a3ffa18ac82eaf4969`,
report `86c2dc17b5e504b0068bae57e3a95a8bfbc88b212715a86661ed97afbbce2eb1`,
and receipt
`0e7bea39012d5963f00c9d0e9f72ade573b7c58cf16a6adcd719b7b7334c8732`.
The committed HOLDOUT directory is evidence, not a replay target; future model
work must create a new candidate/version and a newly preregistered split.
