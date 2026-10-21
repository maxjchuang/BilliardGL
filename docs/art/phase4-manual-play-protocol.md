# Phase 4 首发 12 练习人工可玩验收协议

> 2026-09-22 范围说明：本文件及 `phase4-manual-play-v1.pending.json` 维护历史 v1 十二课的空白协议/测试模板，不代表当前默认 v2 十三课的验收状态。此次仅通过既有生成器同步模板到当前保留的 v1 课包，所有人工字段仍为 PENDING、汇总仍为 NOT_RUN；没有补写参与者、签名或结果。当前十三课的用户验收确认见 `../m4-training-manual-checklist.md`，不要求为本历史模板重复试玩。

日期：2026-07-27  
状态：**PENDING / NOT_RUN**  
Pack：`assets/training/phase4/training_pack_v1.json`

本协议只定义外部真人 QA 的执行方法与证据契约，不包含任何人工可玩结论。Codex、
sub-agent、自动化程序、模拟用户和项目成员均不得充当执行者；现有自动化成功/失败 fixture
只能作为绑定证据，不能替代人工操作、观察记录或双重确认。

## 1. Manifest 冻结范围

执行器从实际 `training_pack_v1` 和 12 个 scenario 文件提取 ID、分类、文件 SHA、初始场景
SHA、中文标题/说明、推荐 assist、capability 与 seed。当前真实内容必须精确覆盖：

| 分类 | Scenario ID |
| --- | --- |
| 定 / 跟 / 拉 | `launch_stop_shot_center_v1` |
| 定 / 跟 / 拉 | `launch_follow_shot_v1` |
| 定 / 跟 / 拉 | `launch_draw_shot_v1` |
| 左右塞吃库 | `launch_left_english_one_rail_v1` |
| 左右塞吃库 | `launch_right_english_one_rail_v1` |
| 左右塞吃库 | `launch_side_english_two_rail_v1` |
| 角袋 / 中袋 | `launch_corner_pocket_angle_v1` |
| 角袋 / 中袋 | `launch_side_pocket_angle_v1` |
| 连续走位 | `launch_two_ball_lane_route_v1` |
| 连续走位 | `launch_three_ball_corner_route_v1` |
| 开球 / 母球控制 | `launch_head_ball_break_control_v1` |
| 开球 / 母球控制 | `launch_cut_break_control_v1` |

分类数量必须为 3/3/2/2/2。manifest 若增删练习、改 ID、改分类或文件哈希，生成器与
validator 会 fail closed；不得继续沿用旧的 12 项人工记录。

## 2. 执行者、环境与隐私

每个 scenario 必须由一名外部真人 QA 实际执行；同一 QA 可以执行多项，但每项都必须有
独立 COMPLETE record。执行者需满足 `real_external_human=true`、`project_member=false`、
`agent_or_robot=false`，匿名编码格式为 `P4QA-XXXXXXXXXX`。观察员使用
`OBS-XXXXXXXX`，两者不能是同一身份。

不采集姓名、邮箱、电话、地址、生日、员工号、账号、照片、录音或其他 PII。记录中只写
匿名设备码、OS、GPU、键盘/鼠标、逻辑/物理 viewport 与 DPI。开放回答和 notes 出现 PII
时必须停止提交、脱敏并重新完成 PII review。

12 项必须绑定同一 build SHA、asset bundle SHA、executable version、physics profile ID/SHA，
并分别绑定当前 pack、coverage matrix、scenario file 和 initial scenario SHA。任何冻结项变化
都要求开启新一轮矩阵，不能拼接不同 build 的记录。

## 3. 每项练习的人工步骤

观察员只提供练习名称，不提示按钮位置、正确路线、力度或击点答案。外部 QA 依次完成并
为每个检查写 `PASS/FAIL` 与脱敏 notes：

1. 从产品入口找到该练习，确认入口可达。
2. 阅读说明、目标和推荐 assist，分别说明是否清晰。
3. 加载场景；门控练习必须在声明所需 authoritative capability 的验收 build 中真正可玩，
   `visible_locked_no_score` 不能算通过。
4. 使用键盘和鼠标完成至少一次合法操作。
5. 实际触发一次 success，并观察明确成功状态。
6. reset 后实际触发一次 failure，并观察明确失败状态。
7. 观察计分结果，确认分数与本次 success/failure 状态一致。
8. 分别操作 reset 与 retry，确认恢复到该 scenario 的 seed 0 初始状态且没有上次 attempt 污染。
9. 完成一次可保存的 attempt，保存 Replay v1 文件。

自动化 fixture 可以帮助构造预期 success/failure，但执行者必须亲手完成产品路径并观察
渲染结果。若真实玩法无法稳定达到 success，该项就是 FAIL，不能用构造的 `AttemptFacts`
替代。

## 4. Replay v1 与 fail-closed 检查

对每项保存的同一个 Replay 文件记录文件 SHA，并检查：

- `format=billiardgl_replay_v1`、`format_version=1`；
- executable version、scenario content SHA、physics profile ID/SHA 与 seed 均和本次记录一致；
- 声明的 final state SHA 有效；
- 同一文件从初始状态完整重放两次，两次 final state SHA 都等于声明值且彼此相同；
- 错误 content hash 明确返回 `replay_content_hash_mismatch`；
- 错误 profile ID 明确返回 `replay_profile_id_mismatch`；
- 错误 profile SHA 明确返回 `replay_profile_hash_mismatch`。

泛化错误提示、静默兼容、只播放一次或改用另一个文件重放均为失败。

## 5. 轨迹图例与 authority 隔离

在同一项练习中开启预测轨迹并查看实际 Replay 轨迹。执行者需人工确认：

- 预测图例 `training.trajectory.predicted` 可读，虚线与 endpoint ring 可识别；
- 实际图例 `training.trajectory.replay_actual` 可读，实线、tick marker 可识别；
- 不依赖颜色也能区分预测与实际，二者 endpoint 都可读。

调用 preview 前后分别采集 authoritative state SHA。两者必须完全一致，并据此填写
`preview_authority_unchanged`；不得凭观察员主观判断覆盖哈希结果。

## 6. 自动化绑定、签认与退出门

每项记录都绑定同一轮成功/失败自动化产物 SHA，并确认 success/failure fixture 通过、
`substitutes_for_manual_play=false`。每项完成后执行者和观察员分别接受
`phase4_manual_play_attestation_v1`，再由观察员完成 `PII=CLEAR` 复核。

validator 只在 12 个 exact scenario record 齐全后计算 aggregate。PASS 要求：

- 12/12 为 COMPLETE，且全部 12 个人工检查均 PASS；
- 12/12 Replay 绑定、双重重放与三种错误绑定拒绝均 PASS；
- 12/12 轨迹图例可读、无需颜色可区分、preview 不改 authority；
- 12/12 自动化绑定有效但不宣称替代人工；
- 环境、匿名资格、双签与 PII review 全部有效。

任一项失败时 aggregate 必须为 FAIL。不得删掉失败记录、换 ID、用自动化补人工项、提前
填写 aggregate，或把 `PENDING` 模板称为验收通过。

## 7. 生成与校验

```bash
python3 scripts/generate_phase4_manual_play_templates.py --output-dir docs/art
python3 scripts/validate_phase4_manual_play.py \
  docs/art/phase4-manual-play-v1.pending.json --pending-template
python3 -m unittest -v tests.training.test_phase4_manual_play
```

PENDING 校验成功固定输出 `records=0/12 result=NOT_RUN`。真实人工记录去掉
`--pending-template`；完整但未达门槛的矩阵返回 `FAIL`，不会把格式正确混同为可玩验收通过。

## 8. 冻结 PENDING 产物

- Matrix：`docs/art/phase4-manual-play-v1.pending.json`  
  SHA-256：`2bfcb267e4dea98073c9e76b08270ab0a697a33f58f8aa5875bb9834c68dcb03`
- Matrix schema：`docs/art/phase4-manual-play-v1.schema.json`  
  SHA-256：`2138ccfcc093add8307b32df11f26b96b3c809bb077cb5247c52ccdb45e4641d`
- Record schema：`docs/art/phase4-manual-play-record-v1.schema.json`  
  SHA-256：`46b25eb7d5d9191b6a38955343cdf81a8a87727a1b7b4650d8008214fcda6e38`

这些哈希只冻结模板和数据契约；它们不是人工验收记录，也不表示 12 项练习已经可玩验收通过。
