# Phase 5 WPA Heyball 产品规则完整性审计

日期：2026-07-28  
产品规则集：`wpa_heyball_2025_08_16_v1`  
结论：**4 IMPLEMENTED / 12 PARTIAL / 17 MISSING / 7 PROCEDURAL_ONLY；profile 不完整且不得作为完整规则集暴露。**

## 证据边界

规范来源为 WPA 官方 [Rules of Heyball - Rules of Play, Effective 2025-08-16](https://wpapool.com/wp-content/uploads/2025/08/250816-Rules-of-Heyball.pdf)。2026-07-28 从 profile 指向的 URL 取回文件，核对为 21 页，SHA-256：
`2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24`。第 7、9、10、12 页另以页面渲染目检，页眉、条款和页码与文本提取一致。

本审计逐条覆盖 Chapter II 1-24，补入与冻结球直接相关的 Chapter I §5，并把 Chapter III 作为明确的限时赛程序边界。Appendix A 只在 Chapter II 明确依赖重置、自由球、恢复、僵局和犯规程序时作为辅助解释。没有把赛事服装、器材行政和申诉流程误当作自动判罚能力。

审计基于当前工作树中的：

- `ShotFacts`：`src/Billiards/rules/chinese_eight_ball_rules.h:52-66`；
- `ChineseEightBallRules`：`src/Billiards/rules/chinese_eight_ball_rules.cpp:78-205`；
- 事实采集：`src/Billiards/rules/shot_fact_collector.cpp:24-77`；
- runtime 应用：`src/Billiards/game_runtime.cpp:655-701`；
- presentation/HUD：`src/Billiards/rules/rules_presentation.cpp:31-55`、`src/Billiards/product_hud.cpp:53-93`、`271-290`；
- 现有 unit/runtime/HUD 测试。

本任务没有修改 C++，也没有改动三项冻结的 Phase 5 rules-copy 文件。机器可读源为
`docs/rules/phase5-wpa-heyball-completeness-audit.json`；Schema 和 validator 会拒绝把 pending action 暴露为可操作产品能力。

## 状态定义

- `IMPLEMENTED`：该条款被当前产品切片确定性表达，并有相称测试；相关后续选择可能在另一条款单独审计。
- `PARTIAL`：只有事实、判罚、runtime、文案、UI/action 或测试中的一部分存在。
- `MISSING`：缺少足以形成产品规则结果的核心事实或执行路径。
- `PROCEDURAL_ONLY`：主要由裁判/赛事组织执行；若未来宣称 competition mode，必须另行产品化。

## 逐条审计矩阵

“无”表示当前没有对应的机器事实、reason/text key、产品 action 或测试，不表示规则不适用。完整 bindings 和 fail-closed capability ID 见同名 JSON。

| WPA 条款（官方页） | 状态 | ShotFacts → rules/runtime | reason/text key → 产品 UI/action | 测试与主要缺口 |
| --- | --- | --- | --- | --- |
| I §5 冻结球声明（p3） | PROCEDURAL_ONLY | 无冻结关系/声明来源；无判罚 | 无；裁判声明 UI 未接入 | 无测试；缺声明时点和 provenance |
| II §1 选手责任（p6） | PROCEDURAL_ONLY | 不属于击球事实 | 应由规则帮助/赛前内容承担 | 非自动判罚范围 |
| II §2 获胜方法（p6） | PARTIAL | pocket order + remaining count + first contact → `legalEight` → winner/gameOver | `rules.chinese8.win.legal_eight` → Winner HUD | legal win E2E 有；非法 8 号与不完整 foul facts 使闭环不完整 |
| II §4 摆球（p6） | MISSING | 无 rack procedure/action | 无；`ball_respot_and_rerack_executor` pending | 初始布局不等于官方可执行重摆 |
| II §5/5.1/5.2 比球（p7） | MISSING | `currentPlayer` 默认 0，无 lag facts | 无 lag/re-lag UI | 需产品决定是否支持官方比球 |
| II §6(a)-(b) 力度/故意不开球（p7） | MISSING | 无 soft-break、intent、offense count | 无 warning/rack-loss/match-loss reason/action | 需裁判输入或明确数字产品裁定 |
| II §6(c) 四球碰库/进球阈值（p7） | IMPLEMENTED | rail set + pocket order → `IllegalBreak` + decision strings | `illegal_break` / `rules.chinese8.foul.illegal_break` → banner；按钮仍 fail-closed | unit/runtime 覆盖阈值；选择执行见 §6(e) |
| II §6(d) 开球线后放置（p7） | PARTIAL | 只有 `ballInHandBehindHeadString` 结果 flag | 无 reminder/foul key；`free_ball_placement` pending | 无几何、次数与越线测试 |
| II §6(e) 开球犯规选择（p7） | PARTIAL | 已有部分犯规与 choice strings；runtime 未等待选择 | scratch/off-table keys；没有 head-string 放置/两种重摆 action | 仅断言 flags/options；状态会在选择前推进 |
| II §6(f) 开球进 8（p8） | PARTIAL | 检出 8，返回 must-spot/rerack options | 预留 `rules.chinese8.pending.eight_on_break`；无交互 action | 只测描述结果，未测重置/重摆执行 |
| II §6(g) 开球失误未碰球堆（p8） | MISSING | 无 miscue/no-rack-contact fact | 无只选择重新开球方的专用 action | 无测试，且不能与普通 break foul 区分 |
| II §7 重置球（p8） | MISSING | 有 must-spot flag，无位置求解 | 无 re-spot action | 缺受阻 fallback、状态修改与恢复测试 |
| II §8 开球线后自由球（p8） | PARTIAL | 只有结果 flag，无合法区/首碰/最近合法球事实 | 预留 ball-in-hand copy；无放置、越线回碰或等距选择 UI | 仅 flag 测试 |
| II §9 全台自由球（p8） | PARTIAL | `ballInHand`/MatchState 存在 | HUD 只读 “Opponent has ball in hand”；放置/确认 pending | 语义测试有，玩家操作缺失 |
| II §10.1 开放球台（p8-9） | PARTIAL | phase + first contact 支持 8 号首碰犯规 | `wrong_first_contact` banner + OPEN TABLE | 缺开球后某一整组已清空的受限选择分支 |
| II §10.2 非法入袋不取回（p9） | PARTIAL | pocket facts 存在，physics 会保留 pocketed | 无需 reason/action | 行为仅为隐式结果，无规则 invariant/E2E |
| II §10.3 开球后定组（p9） | IMPLEMENTED | first contact + first legal pocket → groups/phase | solids/stripes text keys → group text/shape | unit/runtime/HUD E2E 已覆盖 |
| II §10.4 同时首碰且两组均进（p9） | MISSING | `firstContactBall` 只能存一个球 | 无 group-choice state/UI；`simultaneous_contact_and_group_choice` pending | 缺 simultaneity、双组进球、选择和下一选手约束 |
| II §11 无需报袋（p9） | IMPLEMENTED | called pocket 默认不参与判断 | 无需报袋 UI | legal-8 未报袋测试存在 |
| II §12.1 定组后合法首碰（p9） | IMPLEMENTED | group + remaining count + first contact → target/foul | wrong-contact key → target/group/foul HUD | unit/runtime/HUD 覆盖；同时首碰另见 §13 |
| II §12.2 碰球后吃库（p9） | PARTIAL | `anyRailAfterFirstContact` → NoRailAfterContact | `no_rail_after_contact` → banner/ball-in-hand readback | 8 号被排除在 `pocketedObject` 外；进 8 且未吃库可能误判犯规/负局 |
| II §12.3 与 §17(e) 球离台（p9、p11） | PARTIAL | 手工 facts 有 off-table 字段，collector 不产出；当前 rules 选 §17(e) 判 foul | object/cue off-table reason/HUD 已有；policy pending | **官方原文内部冲突，必须取得签署解释；不得静默宣称完整** |
| II §12.4 非法触球（p9） | MISSING | 无身体/器材触球 fact | 无 reason/key/UI | 需 referee fact 或明确 digital-only 不适用策略 |
| II §12.5-12.6 连击/双击（p9） | MISSING | 无 tip-contact count/duration、近贴球 facts | 无 reason/key/ball-in-hand action | M4 必须补事实、判罚和测试 |
| II §12.7 推杆（p10） | MISSING | 无 contact-duration/push fact | 无 reason/key/ball-in-hand action | M4 必须补 deterministic/referee fact |
| II §12.8 球上库后返回（p10） | MISSING | 无 cushion-top state/contact fact | 无 reason/action | 需决定模拟范围 |
| II §12.9 让球移除（p10） | PROCEDURAL_ONLY | 当前无 handicap variant | 无 | 未支持的赛事变体不得宣传 |
| II §13 合法/非法球同时首碰（p10） | MISSING | collector 按 telemetry 顺序只取第一个 | 无 legal-first presumption | 缺确定性 simultaneity window 和 tie-break 测试 |
| II §14 球自行移动（p10） | PROCEDURAL_ONLY | 无 3 秒 settling/影响结果 fact | 无 restore/replay action | 若 competition mode 支持则需状态快照恢复 |
| II §15.1 母球贴合法目标球（p10） | MISSING | 无 frozen relation、direct move、push fact | 无 reason/key | M4 必须补冻结声明/测量与推杆例外 |
| II §15.2 目标球贴库（p10） | MISSING | 单一 rail bool 无法表达离库后再碰库 | 无 reason/key | M4 必须补有序 leave/re-contact telemetry |
| II §16 跳球（p10-11） | MISSING | 无击点半球、越障、失误 facts | 无 illegal-jump reason/ball-in-hand | cue elevation 不是已晋升的规则事实 |
| II §17 一般犯规（p11） | PARTIAL | 覆盖 no-contact/wrong-contact/scratch/off-table 子集 | 5 个相关 reason/key 有 HUD；自由球 action 仍 pending | 缺静止、双脚、触球、双击、推杆、线后、单手、器材、手机处罚 |
| II §18 故意犯规（p11） | MISSING | 无 intent/offense count | 无 rack/match loss action | 需裁判输入和跨局状态 |
| II §19 球组混淆（p11） | MISSING | 当前只把当杆错碰当普通犯规 | 无“下一杆前发现→停局重摆” action | 缺报告窗口与重摆测试 |
| II §20 判负（p12） | PARTIAL | 支持合法/过早/带犯规进 8 的部分组合；不采集 8 离台 | `illegal_eight_ball` / loss key → HUD/winner | 缺最后一颗目标球同杆进 8、8 离台、8 已不在台面时犯规矩阵 |
| II §21 僵局（p12） | MISSING | 无声明/拒绝/各三杆计数 | 仅预留 stalemate copy；无保持原开球方/顺序的 rerack | M4 必须有完整 choice/state/action/E2E |
| II §22 外界干扰（p12） | PROCEDURAL_ONLY | 无 interference/restore facts | 无 referee restore/replay | competition mode 才需产品化 |
| II §23-24 认输/体育道德（p12-13） | PROCEDURAL_ONLY | 不属于自动物理事实 | 无赛事纪律 action | 由赛事裁判处理 |
| III §1-8 限时赛（p13-15） | PROCEDURAL_ONLY | 当前无 time-limited variant | 无 clocks/shootout/timeout/dispute UI | 明确排除于 base M4；不得标为支持 |

## 关键审计发现

1. **开球与 8 号球目前是“描述性分支”，不是产品 action。** `decisionOptions` 可以传到 presentation，但 `GameAction` 没有提交选择、自由球放置、重置 8 号或重摆动作。继续展示为按钮会产生无权威状态修改，因此必须保持不可交互。
2. **`ShotFacts` 的事实面远小于官方规则面。** 当前只有单一首碰、碰库、入袋及若干 off-table 字段；collector 甚至没有填充 cue/object off-table。它无法表达同时首碰、冻结关系、连击、推杆、跳球、线后合法性和裁判事实。
3. **进 8 后吃库条件存在具体风险。** `pocketedObject` 排除了 8；§12.2 的“有目标球进袋即可”却包含 8 号。这会让未额外吃库的进 8 杆先得到 `no_rail_after_contact`，再被判非法 8 号。
4. **官方 PDF 自身有一处需要规则负责人签署解释的冲突。** §12.3 p9 把非 8 号目标球离台称为合法消失；§17(e) p11 又列为一般犯规。当前代码采用后者，但在取得 WPA/规则负责人解释前只能标 `PARTIAL`。
5. **已有 HUD 是英文只读反馈，不是完整本地化/交互证据。** 当前运行时 `noticeForCode` 和 resolution 为硬编码英文；之前生成的中英文内容合同未接 runtime，本审计不把它算成已产品化。

## M4 必须关闭的最小集合

机器门禁列出 24 个 required rows。为便于实施，可归为 8 个最小工作包：

1. **权威事实层**：扩展并从 telemetry/referee channel 产生同时首碰集合、冻结声明与关系、tip contact count/duration、push、jump、cue/object/8 off-table identity、head-string placement，以及所有 8 号判负谓词。
2. **规则解释层**：关闭 §12.2 进 8 漏洞；实现 §10.4/§13 同时首碰与定组选择、§15 冻结球、§12.5-7 连击/推杆、§16 跳球、§20 全判负矩阵；为新增结果分配稳定 reason code/text key。
3. **规则决策状态机**：判罚产生 `awaiting_decision`，在选择提交前不得推进 turn/group/rack；非法或过期 action 必须 fail-closed。
4. **自由球产品 action**：全台/开球线后合法区域、拖动/键盘放置、确认/取消、首碰线后限制、最近合法球重置以及无合法位置错误处理。
5. **重置/重摆执行器**：官方 foot-spot fallback、遮挡求解、8 号重置、完整重摆、开球方选择和原始击球顺序恢复；所有修改必须是单一权威事务。
6. **开球与僵局 UI**：§6(e)/(f) 和 §21 的 modal、焦点、键鼠输入、双语文案、状态读回、取消/断线恢复；pending capability 晋升前不可暴露。
7. **规则政策签署**：至少关闭 §12.3/§17(e) 离台冲突，并记录 lag、裁判事实、competition-only 与限时赛的产品边界。
8. **验证闭环**：每个新 fact/rule/action 有 unit；选择、放置、重置/重摆有 runtime integration；所有 reason/action 有 HUD/localization；完整合法局、各类判负、断点恢复与僵局有 full-rack E2E。

M4 通过条件不是“24 行有代码”，而是 JSON 中全部 acceptance 同时满足，并经人工规则 review。通过前必须持续：

- `profile_complete=false`；
- `complete_profile_product_exposed=false`；
- 所有 pending capability `product_exposed=false`；
- 不使用文案、mock decision string 或 flat proof 代替真实 runtime action/E2E。

## 离线验证

```bash
python3 scripts/validate_phase5_wpa_heyball_completeness_audit.py \
  docs/rules/phase5-wpa-heyball-completeness-audit.json
python3 -m unittest -v \
  tests.visual.test_phase5_wpa_heyball_completeness_audit
```

validator 固定官方 URL、页数和 PDF SHA，验证 40 行条款清单、状态汇总、六层映射、M4 24 行最小集、pending capability 双向引用及 fail-closed 产品边界，并拒绝静默消除 §12.3/§17(e) 冲突。
