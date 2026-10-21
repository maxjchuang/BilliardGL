# Phase 3 / M2 ProductShell Shipping Token 技术美术验收

日期：2026-07-27  
结论：**独立资产、C++ strict loader/source audit、九屏 CPU 展开与 flat proof PASS；GPU capture 待提交路径接入后完成**

## 边界与结果

- 本包是 `product_shell` 独立 namespace，不修改或覆盖 HUD v1 token set。
- 43 个 token 全部是仓库原创的 normalized 24×24 geometry，只使用
  `rect`、`circle`、`polyline`、`polygon`；运行时不解析 SVG。
- standard / color-vision-safe 两套 palette 均满足文字 4.5:1、关键图形
  3:1 的最低对比契约。状态同时保留形状、相邻 SDF 文字和状态/数值文字。
- alias 为空；unknown token 必须 hard error，不允许近似映射、cycle 或 fallback。
- 字体继续复用现有 FreeType/SDF atlas；最小文字为 14 logical px、Retina
  为 21 physical px。focus chevron、selected、error 图标最低分别为 10、14、
  22 logical px，与现有 draw-list 尺寸一致。
- shipping/debug namespace、nodes 和 release-capture 权限完全隔离。
- flat proof 明确标记 `data-runtime-capture="false"` 与
  `data-user-research="false"`；它不能代替 GPU 截图或用户研究。

## 九屏 coverage

| 屏幕/全局状态 | 实际 shapeToken |
| --- | --- |
| Main menu | `single_player_badge`, `training_target`, `replay_triangle`, `settings_sliders`, `help_circle`, `exit_door` |
| Single-player setup | `rules_shield`, `ai_diamond`, `play_triangle` |
| Match | `rules_shield`, `settings_sliders`, `help_circle` |
| Training catalog | `training_target`, `empty_box` |
| Training session | `training_target`, `reset_arrow`, `catalog_grid` |
| Replay browser | `replay_file`, `empty_box` |
| Replay playback | `play_triangle`, `pause_bars`, `speed_down`, `speed_up`, `camera_frame`, `seek_previous`, `seek_next`, `replay_file` |
| Settings | `assist_rings`, `mouse_axis`, `vertical_arrows`, `keyboard_outline`, `text_scale`, `pattern_circle`, `caption_box`, `music_note`, `venue_waves`, `impact_burst`, `ui_click`, `camera_frame`, `distance_arrow`, `glow_circle`, `focus_brackets`, `camera_motion`, `shake_lines` |
| Help | `aim_crosshair`, `power_bars`, `cue_ball`, `camera_frame`, `rules_shield`, `back_arrow` |
| Error / item fallback / selected / focus | `error_cross_octagon`, `empty_box`, `item_square`, `selected_check_diamond`, `focus_chevron` |

去重后的 exact set 为 43 个：

`aim_crosshair`, `ai_diamond`, `assist_rings`, `back_arrow`, `camera_frame`,
`camera_motion`, `caption_box`, `catalog_grid`, `cue_ball`, `distance_arrow`,
`empty_box`, `error_cross_octagon`, `exit_door`, `focus_brackets`,
`focus_chevron`, `glow_circle`, `help_circle`, `impact_burst`, `item_square`,
`keyboard_outline`, `mouse_axis`, `music_note`, `pattern_circle`, `pause_bars`,
`play_triangle`, `power_bars`, `replay_file`, `replay_triangle`, `reset_arrow`,
`rules_shield`, `seek_next`, `seek_previous`, `selected_check_diamond`,
`settings_sliders`, `shake_lines`, `single_player_badge`, `speed_down`,
`speed_up`, `text_scale`, `training_target`, `ui_click`, `venue_waves`,
`vertical_arrows`。

validator 通过平衡括号解析 `item(...)`、`setting(...)` 与
`addPrimitive(... UiPrimitiveKind::Icon ...)`，直接比对上述 exact set；新增、删除
或重命名 C++ token 都会 fail closed。ProductScreen enum 也必须保持九屏精确覆盖。

## 产物身份

| 文件 | SHA-256 |
| --- | --- |
| `focused_practice_shell_shipping_tokens_v1.manifest.json` | `cb937f7c590a7ab0a2b7666e1fd6b522eb032c9393909558e31872fc4faf743a` |
| `focused_practice_shell_shipping_tokens_v1.json` | `c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3` |
| `focused_practice_shell_shipping_tokens_flat_proof_v1.svg` | `7f6b32654f1d20d3323a111024fe08dad47961933dd78ebce9e57892a1185778` |

现有 HUD v1 三个 SHA 保持不变：manifest `969d3b8b...`、token
`abf73aa2...`、flat proof `8058de94...`。

## 验证与坏输入

```bash
python3 scripts/generate_phase3_shell_token_package.py \
  --output-dir assets/modern/hud
python3 scripts/validate_phase3_shell_token_package.py \
  assets/modern/hud/focused_practice_shell_shipping_tokens_v1.manifest.json
python3 -m unittest -v tests.visual.test_phase3_shell_token_package
```

15/15 单测通过，其中 10 个坏 fixture 分别证明以下输入会被拒绝：缺失 token、
近似 alias、24×24 越界 geometry、CVD 低对比、flat proof 冒充 runtime capture、
缺失 CVD palette、缺失屏幕 coverage、color-only 状态、debug/shipping namespace
合并、manifest SHA 篡改。其余测试覆盖 committed package、byte determinism、CLI
报告、实际 C++ exact-set audit，以及新增 C++ token 的提取能力。

C++ runtime 已由 `loadShellShippingTokenSet` 按独立 exact schema 加载，并在
`BilliardsProductShellTests` 中对真实九屏、4 个 viewport、3 档 UI scale、standard /
color-vision-safe 两套 palette 完成 CPU 展开；所有 `Icon` 都被消耗为显式 triangle，
unknown/opaque fallback 为零。HUD 与 Shell 通过显式 token-set id 保留各自 namespace
和 palette，同名但不同几何不会被静默覆盖。`BilliardsProductUiFrameCoordinatorTests`
进一步验证正常产品帧只选择一个 overlay：Match 使用 Gameplay HUD，其余八屏保留
Shell controls；automation、windowless 与非产品模式不产生 overlay。以上均为无头 CPU
证据，仍需实际 GPU submit/capture；flat proof 不提前关闭运行时视觉门。
