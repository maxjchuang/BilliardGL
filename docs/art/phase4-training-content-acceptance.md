# Phase 4 首发训练内容包：实现与验收

日期：2026-07-27  
内容 schema：`training_scenario_v1`  
Pack：`assets/training/phase4/training_pack_v1.json`

## 交付范围

| 类别 | 数量 | 首发练习 |
| --- | ---: | --- |
| 定 / 跟 / 拉 | 3 | 中杆定球、高杆跟进、低杆拉回 |
| 左右塞吃库 | 3 | 左塞一库、右塞一库、侧塞两库 |
| 角袋 / 中袋 | 2 | 角袋薄球、中袋角度 |
| 连续走位 | 2 | 两球直线路线、三球角袋路线 |
| 开球 / 母球控制 | 2 | 正碰开球控白、切薄开球控白 |

每个练习都包含中英文标题与说明、tags、嵌入式 canonical physics scenario v1、允许控制与 assist 上限、确定性的 success/failure predicate、原子 reset、量化评分和 reference replay 占位绑定规则。

## 物理能力边界

当前 automation 文档明确说明 angular velocity 尚不能代表完整真实旋转动力学，因此内容层不把 UI 击点当作已经晋升的生产能力：

- 跟杆和拉杆要求 `authoritative_longitudinal_tip_offset_v1`。
- 左右塞练习要求 `authoritative_horizontal_tip_offset_v1`。
- capability 缺失时仍可在目录中展示，但状态必须是 `visible_locked_no_score`。
- 所有门控练习的 `production_default_enabled` 均为 `false`。
- `cue_elevation`、`chalk`、自动瞄准和自动击球全部明确禁止。
- manifest 不含 `physics_profile`，因此不会覆盖冻结生产默认。

## 确定性合同

- 初始状态固定 seed 为 0，并使用 canonical JSON SHA-256 绑定。
- 球均为零速度、零角速度；索引唯一、母球存在、球间不重叠且球心位于可玩边界内。
- success 使用闭集 predicate vocabulary，统一在球停止后的 attempt end 评估。
- failure 有稳定优先级：数值失败、母球落袋、超杆数、超 tick。
- 评分只依赖量化误差和杆数，采用 `nearest_integer_half_away_from_zero`。
- 同分排序依次使用分数、attempt ticks、trace SHA-256；禁止时钟、平台 RNG、无序迭代和视觉帧率。
- replay 保持 `placeholder_unbound`；只有 replay/scenario/profile/action-log/trace 哈希齐全且审核通过才允许绑定。

## 静态验证与坏输入

`scripts/validate_phase4_training_pack.py` 校验 schema、文件哈希、12 项分类覆盖、几何、predicate、评分、能力门控和 replay 绑定。五个 committed 坏输入分别锁定以下错误：

| Fixture | 预期错误 |
| --- | --- |
| 缺少 success | `E_REQUIRED_SUCCESS` |
| 球体重叠 | `E_GEOMETRY_OVERLAP` |
| 旋转练习缺少 capability gate | `E_SPIN_CAPABILITY_GATE` |
| replay 已绑定但缺少 trace hash 条件 | `E_REPLAY_BINDING` |
| 非确定性评分 | `E_SCORING_DETERMINISM` |

## 工程接入验收清单

- [ ] 运行时只在声明 `training_scenario_v1` 后加载 pack。
- [ ] 加载使用嵌入式 canonical scenario，并校验 `canonical_sha256`。
- [ ] 未满足 spin capability 时不开始可计分 attempt。
- [ ] reset 原子恢复初始球态、tick 0 与 seed 0，不保留上次 shot/event 状态。
- [ ] success/failure 只从权威 physics/rules event 与 stopped state 求值。
- [ ] HUD 可以显示目标区，但 assist 不可执行自动瞄准或自动击球。
- [ ] 评分按 manifest 的量化、封顶和舍入规则计算。
- [ ] replay 未满足全部哈希和 review 条件时不显示“参考回放可用”。
- [ ] `python3 scripts/validate_phase4_training_pack.py assets/training/phase4/training_pack_v1.json` 通过。
- [ ] `python3 -m unittest tests.training.test_phase4_training_pack` 通过。
- [ ] 全量工程测试无回归，生产 physics profile 与 frozen evidence 无改动。

这些是内容合同和静态可验证结果；真实击球可达性与手感仍需在相应旋转物理 capability 晋升后，用固定 runtime/profile 的参考 replay 与玩家测试完成验证。
