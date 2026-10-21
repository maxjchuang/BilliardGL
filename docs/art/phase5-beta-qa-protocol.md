# Phase 5 / M4 产品 Beta 人工验收协议

日期：2026-07-27  
状态：**PENDING / NOT_RUN**  
默认产品模式切换：**UNAUTHORIZED**

本协议定义真实产品 Beta 的人工执行和证据格式，不包含对局或验收结果。Codex、sub-agent、
模型测试、自动化脚本、项目成员与 scenario/state injection 均不得冒充外部真人产品对局。
自动化只可提供绑定证据，不能替代真实输入、可读性判断、内容决策或双重签认。

## 1. 当前代码合同与前置风险

矩阵生成器直接绑定以下实际源码 SHA、HUD/Shell token SHA 和 Phase 3 capture matrix SHA：

- WPA Heyball：`wpa_heyball_2025_08_16_v1`；
- AI：`rookie`、`club`、`expert`；
- ProductShell、Settings、Input 与 Audio policy；
- HUD/Shell shipping tokens 和实际 GPU capture 证据。

`docs/rules/wpa-heyball-2025-08-16-profile.md` 仍明确列出完整产品规则集尚缺的程序性事项，
包括冻结球、连击、推杆、跳球、裁判事实及重摆执行等。因此本协议不会仅凭已有规则单测
宣称完整 WPA 规则已经 Beta-ready；若实际产品路径无法完成本协议要求，结果必须 BLOCK/FAIL，
默认模式不得切换。

## 2. 十二份必需记录

| Record ID | 人工验收范围 |
| --- | --- |
| `full_match_product` | 从产品入口开新局，由开球开始，以真实物理操作完成合法胜负 |
| `rules_hud_reason_coverage` | 规则、HUD、reason code、自由球、换手和胜负可读性 |
| `ai_rookie_product_challenge` | Rookie 产品入口挑战与主线程响应 |
| `ai_club_product_challenge` | Club 产品入口挑战与主线程响应 |
| `ai_expert_product_challenge` | Expert 产品入口挑战与主线程响应 |
| `settings_lifecycle` | 设置加载、事务、保存、持久化及重绑定回滚 |
| `accessibility_and_controls` | 缩放、CVD、字幕、assist、四 bus、动态效果 |
| `audio_alias_impact` | `hit.wav` impact 共享别名内容决策 |
| `audio_alias_notifications` | `GameOver.wav` UI/foul/finish 共享别名内容决策 |
| `audio_alias_music` | `background.wav` 音乐循环共享别名内容决策 |
| `audio_alias_ambience` | 同一 `background.wav` 场馆氛围共享别名内容决策 |
| `actual_gpu_shell_hud` | Shell/HUD 实际 GPU capture、opaque/debug 门 |

缺任一 record、重复 ID、未 COMPLETE 或提前 aggregate 都不能 PASS。

## 3. 外部真人、环境与隐私

每份记录必须由外部真人 QA 实际执行，满足 `real_external_human=true`、
`project_member=false`、`agent_or_robot=false`。执行者匿名 ID 为 `P5QA-XXXXXXXXXX`，
观察员为 `OBS-XXXXXXXX`，两者不能自签。

所有记录绑定同一 build/content、WPA rules 源码、physics profile ID/SHA、HUD/Shell token、
实际 GPU capture manifest/manual review 和 audio policy SHA。记录匿名设备码、OS、GPU、键鼠、
逻辑/物理 viewport 与 DPI。禁止姓名、邮箱、电话、地址、账号、录音、照片等 PII；notes 与
内容评审意见必须完成 `PII=CLEAR` 复核。

## 4. 完整真实产品对局

从 Main → Single Player Setup 进入产品 Match，初始 phase 必须是 `break`。执行者只能通过
产品公开键鼠控制完成每一杆；禁止 training/physics scenario、automation state replacement、
直接构造 `ShotFacts`、跳 tick 或隐藏 solver 接口。记录真实 physical shot count、完整 match
trace SHA、Replay SHA、最终 `complete` phase、winner 和玩家可读结果。

该记录必须从开球走到合法 8 号球胜负。测试 fixture 中“构造成功事实”的做法不能作为此项
Beta 对局证据。

## 5. WPA Heyball 规则与 HUD

人工覆盖以下实际产品事件，每项绑定 GPU screenshot SHA，并核对玩家可读 HUD、reason code、
text key、自由球和换手：

- 合法开球；非法开球 `illegal_break`；
- 球组确定，显示 `rules.chinese8.group.solids` 或 `rules.chinese8.group.stripes`；
- 空杆 `no_object_ball_contact`；
- 错球 `wrong_first_contact`；
- 碰球后无库 `no_rail_after_contact`；
- 母球落袋 `cue_ball_scratch`；
- 自由球/ball-in-hand 与下一位玩家；
- 普通换手；
- 合法 8 号球 `rules.chinese8.win.legal_eight`；
- 非法 8 号球 `illegal_eight_ball` / `rules.chinese8.loss.illegal_eight`。

源码中另外存在 `cue_ball_off_table` 与 `object_ball_off_table`，仍冻结在 reason-code contract，
不能从规则实现中静默删除。reason code 不得用泛化文案或观察员猜测替代。

## 6. 三档 AI

Rookie、Club、Expert 各从产品 Single Player Setup 选择并开始/完成一次挑战。分别绑定实际
4/8/12 ms planning budget 与 8/24/64 candidate 上限。验收需确认：

- 三档均真实可选，不是禁用或装饰项；
- AI 只提交语义 shot control，不读取或调用隐藏 physics 接口；
- planning 在异步 worker 执行；主线程单次观测阻塞必须 ≤16.7 ms；
- 自动化 trace SHA 只能支持测量，不能代替真人观察或产品入口操作。

## 7. Settings、Input 与可访问性

人工依次验证：启动加载；坏 JSON 回退 defaults 并显示 `E_SETTINGS_FORMAT`；preview 不改 live；
apply 生效；cancel 恢复；原子保存；重启后文件 SHA 一致；按当前上下文重绑定；冲突返回
`E_BINDING_CONFLICT` 且原绑定回滚。

从产品 Settings 确认 85% 与 135% UI scale、五种 CVD、Off/Essential/All 字幕、
Off/Basic/Guided assist、Music/Ambience/Effects/UI 四 bus，以及 bloom、depth of field、
dynamic camera、camera shake 均可达。任何 assist 档都不得自动瞄准或自动击球。

## 8. 音频退化资产披露

当前代码只有四个 WAV，`ballin.wav` 是唯一 distinct transient；其余共享关系不得包装成独立
声音。Beta 内容评审必须对以下四个退化用途逐项写
`ACCEPTED_DEGRADED` 或 `BLOCKED`：

1. `hit.wav`：cue impact、ball-ball、rail、pocket jaw；
2. `GameOver.wav`：UI feedback、foul、match finish；
3. `background.wav`：music loop；
4. 同一 `background.wav`：venue ambience。

四项均必须报告 `degraded_asset_alias`、`claimed_distinct_asset=false`、
`claimed_pitch_supported=false`。SDL_mixer 当前路径不支持 pitch；意图元数据不能写成已播放。
任一项 `BLOCKED` 都是有效但不通过的内容结论，默认模式继续保持未授权。

## 9. 实际 GPU capture

必须绑定通过 Phase 3 validator 的实际 GPU capture manifest 和人工 review，不能绑定 PENDING
matrix、flat proof 或桌面截图。Shell 与 HUD case 均须存在，且最终聚合
`opaque_icon_count=0`、`unexpanded_icon_count=0`、`debug_node_count=0`。capture manifest、
manual review、token 和本次 build/content 必须属于同一冻结证据集。

## 10. 退出门与默认模式授权

validator 只在 12/12 COMPLETE 后重算 aggregate。PASS 必须同时满足：12 项人工 check、领域
证据、自动化绑定、环境 SHA、外部真人资格、双签和 PII 全部有效；四项音频均被内容评审明确
接受；实际 GPU 门通过。只有此时
`product_mode_default_switch_authorized=true`。任何 FAIL、BLOCKED、缺失或 PENDING 都必须为
false，禁止人工改写 aggregate 授权默认切换。

## 11. 生成与校验

```bash
python3 scripts/generate_phase5_beta_qa_templates.py --output-dir docs/art
python3 scripts/validate_phase5_beta_qa.py \
  docs/art/phase5-beta-qa-v1.pending.json --pending-template
python3 -m unittest -v tests.visual.test_phase5_beta_qa
```

PENDING 成功固定输出：
`records=0/12 result=NOT_RUN default_switch=UNAUTHORIZED`。这只证明模板未漂移，不是 Beta 通过。

## 12. 冻结 PENDING 产物

- Matrix：`docs/art/phase5-beta-qa-v1.pending.json`  
  SHA-256：`23980bba043e3a6d06ec4076b4483c8ea7cd433ee2d5ab684458310b9d499d32`
- Matrix schema：`docs/art/phase5-beta-qa-v1.schema.json`  
  SHA-256：`2722c2afe96c1ff5cc11c893c712ea32919ebc228f634939e2fc5c546e669b67`
- Record schema：`docs/art/phase5-beta-qa-record-v1.schema.json`  
  SHA-256：`d714ada0786027ee47d2b2021b9e1a14a24f0d011aea7503d3f6a23baa1223ce`

这些哈希只冻结 QA 模板与数据契约，不是实际对局、音频接受或 GPU capture 证据。
