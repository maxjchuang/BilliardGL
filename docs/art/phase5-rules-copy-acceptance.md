# Phase 5 WPA Heyball 规则反馈与中英文本包

日期：2026-07-27  
规则集：`wpa_heyball_2025_08_16_v1`  
状态：**内容合同已生成并通过离线 validator；尚未接入 runtime；profile 仍不完整**

## 范围

`phase5-rules-shipping-copy-v1.json` 将现有 `ChineseEightBallRules`、`ProductHud`、规则 profile
文档和 HUD/Shell shipping token 固定为一个中英文文案合同。它不修改规则语义或 runtime，
也不把待实现的选择/放置流程标成可用。

现有八个 machine reason code 均为 `capability=implemented`：

- `illegal_break`
- `no_object_ball_contact`
- `wrong_first_contact`
- `no_rail_after_contact`
- `cue_ball_scratch`
- `cue_ball_off_table`
- `object_ball_off_table`
- `illegal_eight_ball`

每项均绑定实际 text key、现有经审核 canonical shape token/runtime alias、中英文标题、解释、
下一步，以及 ball-in-hand、turn、handoff、win、loss 的机器语义。shape、标题、解释和下一步
形成冗余通道，禁止仅靠颜色表达。

## 待实现 UI 的隔离

以下能力统一为 `capability=pending`、`product_exposed=false`：

- 开球犯规后的对手选择；
- 开球 8 号球的重置点位/重摆选择；
- 僵局确认与重摆程序；
- 自由球合法区域放置和确认。

它们拥有预留 machine/text key、双语说明和操作按钮文案，但只是实现规格。runtime 在能力晋升
并通过产品验证前不得渲染为可点击产品功能。规则 profile 文档列出的冻结球、连击、推杆、跳球、
裁判程序和完整 end-to-end rack 等缺口仍然存在，因此 manifest 固定
`profile_complete=false`。

## 呈现与可访问性

优先级为：比赛结果 modal → 规则选择 modal → 犯规 banner → 自由球 banner → 换手 toast →
信息 toast。结果和选择 modal 持续到确认/合法选择；banner/toast 使用固定持续时间。

- banner 不抢焦点，modal 限制焦点在自身可用操作内；
- 键盘支持 Tab、Shift+Tab、方向键、Enter 和 Space；
- modal 首焦点落到第一个可用操作，并始终显示 shape focus；
- 犯规/结果使用 assertive live region，换手/信息使用 polite；
- 朗读顺序为标题、解释、下一步、当前操作；
- standard 与 color-vision-safe palette 都要求 shape+text；
- 规则必要文字始终显示，字幕设置不能隐藏它。

所有 shape token 来自现有 HUD/Shell 包；validator 拒绝未知 token 和新增未审核 alias。

## Flat proof 边界

`phase5-rules-shipping-copy-flat-proof-v1.svg` 仅用于检查文案层级、双语长度、pending 隔离和
版面密度。画面明确标注 `FLAT TEXT/LAYOUT PROOF · NOT GPU · PROFILE INCOMPLETE`。
它不是实际 framebuffer、GPU capture、可访问性人工测试或 runtime 接入证据。

## 验证

```bash
python3 scripts/generate_phase5_rules_copy_package.py --output-dir docs/art
python3 scripts/validate_phase5_rules_copy_package.py \
  docs/art/phase5-rules-shipping-copy-v1.json
python3 -m unittest -v tests.visual.test_phase5_rules_copy_package
```

坏输入覆盖缺 reason code、重复 text key、pending 暴露、turn/ball-in-hand/handoff/win/loss
语义漂移、仅颜色、无 action、双语缺失、无下一步、未审核 alias、伪称 profile 完整及 flat
proof 冒充 GPU。

## 冻结产物

| 文件 | SHA-256 |
| --- | --- |
| `phase5-rules-shipping-copy-v1.json` | `bbc5bd4199d1c876948aa3641957c0b9bbe1748b24bf517670d36d24b49781aa` |
| `phase5-rules-shipping-copy-v1.schema.json` | `b4c056db14ae00e2fdab6ec5374b2bce0da683a8306988482f4adfe836219d68` |
| `phase5-rules-shipping-copy-flat-proof-v1.svg` | `5d2c214c0885a42b1b4c4542c4eaf3a23bb8ce6329784b0f85e0494a6b18bb38` |

上述哈希冻结内容合同与 flat proof；不表示文案已接入 runtime，也不是实际 GPU capture。
