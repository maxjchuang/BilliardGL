# Phase 5 / M4 规则决策 UI 与 Action 实现规格

日期：2026-07-28  
规则集：`wpa_heyball_2025_08_16_v1`  
状态：**PENDING / profile incomplete / product_exposed=false**

## 交付结论

本包把官方完整性 audit 中 M4 所需的六项待实现能力拆成七条可直接交给 C++ 的交互流，固定
capability、flow、state、action ID，以及中英文标题、正文与按钮。它只定义表现和客户端/规则权威
之间的合同，不修改规则语义、C++ runtime 或三项冻结 rules-copy，也不能用来晋升 capability。

主合同是 `phase5-rules-decision-ui-actions-v1.pending.json`；schema 把包、能力和流程固定在 pending、
不可产品暴露状态。`phase5-rules-decision-ui-actions-flat-proof-v1.svg` 仅用于平面文案、层级和密度检查，
不是 GPU capture、runtime 证据或无障碍人工测试。

## M4 最小交互流

| Capability ID | Flow ID | 精确 Action ID |
| --- | --- | --- |
| `break_foul_opponent_choices` | `break_foul_opponent_choice` | `rules.break_foul.accept_table`; `rules.break_foul.rerack_opponent_breaks`; `rules.break_foul.rerack_original_breaker` |
| `eight_on_break_spot_or_rerack` | `eight_on_break_spot_or_rerack` | `rules.eight_on_break.spot_continue`; `rules.eight_on_break.spot_ball_in_hand_head_string`; `rules.eight_on_break.rerack_original_breaker`; `rules.eight_on_break.rerack_opponent_breaks` |
| `free_ball_placement` | `ball_in_hand_full_table` | `rules.ball_in_hand.preview_position`; `rules.ball_in_hand.confirm_full_table_placement`; `rules.ball_in_hand.reset_preview` |
| `free_ball_placement` | `ball_in_hand_behind_head_string` | `rules.ball_in_hand.preview_position`; `rules.ball_in_hand.confirm_behind_head_string`; `rules.ball_in_hand.request_nearest_legal_respot`; `rules.ball_in_hand.choose_equidistant_respot_ball`; `rules.ball_in_hand.reset_preview` |
| `simultaneous_contact_and_group_choice` | `simultaneous_group_choice` | `rules.group_choice.choose_solids`; `rules.group_choice.choose_stripes` |
| `ball_respot_and_rerack_executor` | `respot_rerack_progress_error` | `rules.execution.retry_status`; `rules.execution.return_to_decision` |
| `stalemate_decision` | `stalemate_proposal` | `rules.stalemate.accept_rerack`; `rules.stalemate.refuse_continue` |

每条 flow 都拥有独立命名空间内的 exact state ID、非空 preconditions/postconditions、authority command、
ACK 要求和 fail-closed failure policy。C++ 接入时不得重命名 ID、从按钮文案反推动作、在 ACK 前更改
回合/球组/球位/rack，或在超时后代替玩家选择。

## 交互与恢复合同

层级从高到低为：比赛结果 modal、阻断错误、阻断进度、规则决策 modal、自由球放置 overlay、
犯规 banner、自由球 banner、换手 toast、信息 toast。规则 modal 可以保留 HUD 摘要，但 HUD 不得成为
权威来源；任何阻断流程都必须屏蔽击球输入。

- 初始焦点落在 authority offer 中第一个可用 action，modal 捕获焦点并显示 `focus_chevron`；
- Tab / Shift+Tab / 方向键 / Enter / Space / Escape 与 hover / click / placement drag 均有固定语义；
- 必选决策的 Escape 只朗读“需要决策”并保持打开；放置 Escape 只清本地 preview，不清 entitlement；
- transaction 中 Escape 被忽略并朗读进度；10 秒 ACK 超时进入 authority error，不执行本地 mutation；
- 恢复必须匹配 decision/transaction ID；旧 revision 必须以最新 authority offer 重开；
- 重试复用 idempotency key，重复 key 返回相同结果。

## 权威 ACK 与 fail-closed

request 必须带 ruleset、capability、flow、decision、action、revision、idempotency key、actor 和 payload；
ACK 必须回显 decision/action/key、前后 revision、effects 和 authoritative state hash。客户端拒绝未知 effect、
部分 transaction effect、未提供的 action 和过期 revision。放置 preview 永远是本地非权威状态。

所有 authoritative mutation 都必须满足：action 在 offer 中、preconditions 成立、发送精确 request、收到
accepted ACK、revision 连续、effects 完整、hash 可接受。任一步失败均停留在阻断 error/progress，不自动
换手、不清自由球权利、不重摆、不继续计分。

## 可访问性与既有 token

所有 title、body 和 button 都提供 `en` / `zh_cn`。决策与错误使用 assertive live region，进度使用
polite；朗读顺序为标题、正文、authority 状态、当前焦点 action、位置或进度细节。自由球坐标必须有
人类可理解的区域描述。

standard 与 color-vision-safe palette 都必须同时使用 shape、标题、正文和 action，禁止仅靠颜色。
规则必要文字不受字幕开关影响；reduced motion 只关闭非必要动画，进度仍保留文字和 shape。
manifest 中引用的 shape token 全部来自现有 HUD / ProductShell shipping token inventory；validator 会拒绝
新造 token。

## 未决政策边界

以下 placeholder 必须维持 `UNRESOLVED`、`assumed_answer=null`、`product_exposed=false`：

- `stalemate_declaration_authority_policy`：不同产品模式下谁有权提出/最终宣告僵局；
- `stalemate_continuation_window_policy`：裁判酌情继续窗口如何表达和版本化；
- `off_table_12_3_vs_17_e_interpretation`：官方 PDF 第 II 章 12.3（第 9 页）与 17(e)（第 11 页）冲突的签署解释。

本包没有、也不得宣称已解决上述政策。尤其不得用实现细节静默选择 12.3 或 17(e)。

## 冻结依赖

下列三项只读输入的 SHA-256 在 validator 中固定，本交付未修改它们：

| 文件 | SHA-256 |
| --- | --- |
| `phase5-rules-shipping-copy-v1.json` | `bbc5bd4199d1c876948aa3641957c0b9bbe1748b24bf517670d36d24b49781aa` |
| `phase5-rules-shipping-copy-v1.schema.json` | `b4c056db14ae00e2fdab6ec5374b2bce0da683a8306988482f4adfe836219d68` |
| `phase5-rules-shipping-copy-flat-proof-v1.svg` | `5d2c214c0885a42b1b4c4542c4eaf3a23bb8ce6329784b0f85e0494a6b18bb38` |

## 验证

```bash
python3 scripts/generate_phase5_rules_decision_ui_actions.py --output-dir docs/art
python3 scripts/validate_phase5_rules_decision_ui_actions.py \
  docs/art/phase5-rules-decision-ui-actions-v1.pending.json
python3 -m unittest -v tests.visual.test_phase5_rules_decision_ui_actions
```

坏输入覆盖：包/flow 伪晋升、capability 暴露、action 缺失或重复、ACK 缺失、optimistic mutation、
中英文缺失、未知 token、层级/焦点/输入/取消/超时/恢复漂移、fail-open、僵局政策猜测、官方冲突
伪解决、冻结绑定漂移，以及 flat proof 冒充 GPU/runtime。

## 后续 C++ 准入门

只有在规则负责人签署对应政策、authority 能生成并验证上述 request/ACK、全部 exact flow/action/state ID
接入、自动化坏输入通过、真实 GPU capture 与键鼠/读屏人工验收通过后，相关 capability 才可逐项申请
从 pending 晋升。该过程不能整体把 profile 标为 complete；仍需遵循官方完整性 audit 的其余缺口。
