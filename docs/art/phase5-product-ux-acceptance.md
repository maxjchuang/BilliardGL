# Phase 5 / M4 产品 UX 与内容验收

日期：2026-07-27  
Manifest：`assets/modern/product_ux/product_ux_v1.json`  
规则集：`wpa_heyball_2025_08_16_v1`

## 规则提示

manifest 精确覆盖 `FoulReason` 的 9 个 machine reason code（含不显示的 `none` sentinel）。每项绑定 C++ `text_key`，并提供中英文标题、解释、优先级、持续策略、图标 token、独特轮廓和非颜色文本前缀。

| Reason | 优先级 | 展示策略 | 非颜色冗余 |
| --- | ---: | --- | --- |
| `illegal_eight_ball` | 100 | 确认前持续，至少 6 秒 | 八边形、8 与叉号、`[LOSS]` |
| `cue_ball_off_table` | 92 | 5 秒 | 白球越框、`[FOUL]` |
| `cue_ball_scratch` | 90 | 5 秒 | 白球向下箭头、`[FOUL]` |
| `object_ball_off_table` | 85 | 5 秒 | 号码球越框、`[FOUL]` |
| `no_object_ball_contact` | 75 | 4.5 秒 | 断开的接触环、`[FOUL]` |
| `wrong_first_contact` | 70 | 4.5 秒 | 号码球斜杠、`[FOUL]` |
| `no_rail_after_contact` | 65 | 4.5 秒 | 带缺口的三条库边、`[FOUL]` |
| `illegal_break` | 60 | 直到玩家作出选择；12 秒仅作 fallback | 菱形与三条计数线、`[DECISION]` |
| `none` | 0 | 不显示 | sentinel |

多个 reason 同时出现时按 `priority_desc`、再按 code 升序稳定排序；最多显示 3 项，其余显示“另有 N 项”并可打开详情。

重要边界：规则类和 reason mapper 已有单元测试，但仓库中没有 `ChineseEightBallRules` 的主游戏 runtime 消费者。因此 8 个可见 reason 全部要求 `rules_reason_hud_bridge_v1`，状态为 `capability_gated`。这不是已完成 runtime 接线的声明。

## AI 三档验收

| 档位 | 用户说明摘要 | 目标成功率 |
| --- | --- | ---: |
| Rookie / 新手 | 选择明显进球，留下可利用机会 | 35%–50% |
| Club / 球房 | 规划 2–3 球路线，保留可读走位误差 | 58%–72% |
| Expert / 高手 | 管理局面风险、稳定连续进球，但不作弊 | 78%–88% |

每档使用 600 次固定步长试验：开球、开放球台、连续清台压力各 200 次；固定 seed 集，报告 Wilson 95% 区间，指标为对 reference policy 的整局胜率。区间互不重叠且随难度单调上升。当前没有 AI runtime，因此三档统一要求 `single_player_ai_v1`，不能把目标区间当作已经达到的结果。

## 设置、输入与可访问性

- 动作表覆盖现有产品键鼠路径：帮助、瞄准、击球、方向、力度、镜头环绕/平移/缩放、镜头跟随、返回母球和退出；另列设置及训练重置的 gated 默认键。
- 调试灯光快捷键不进入产品重绑定表。
- 同一 normalized control 且 context 重叠为 hard conflict，阻止应用并同时指出冲突动作；不同 context 的复用需显示 context。
- 应用设置使用 preview 后原子提交；取消丢弃未应用更改；恢复当前分类或全部默认前必须确认。
- 所有设置支持纯键盘焦点导航和可见 focus indicator。
- 字号缩放 85%–135%，超过 115% 必须 reflow；普通文字对比度不低于 4.5:1。
- 字幕支持关闭、关键、全部三档，包含说话人标签和非语音提示。
- 色觉模式含 default、红绿色觉两型、蓝黄色觉和高对比纹理；任何规则信息均不得只靠颜色。
- 辅助等级为 off/basic/guided，最高档仍禁止自动瞄准和自动击球。

输入重绑定 runtime 尚未实现，必须等待 `input_rebinding_v1` capability。

## 响应式样机

| Case | Logical | Physical | Scale | 最小文字 |
| --- | ---: | ---: | ---: | ---: |
| minimum | 1280×720 | 1280×720 | 1.0 | 14 px |
| reference | 1920×1080 | 1920×1080 | 1.0 | 14 px |
| high DPI | 1280×720 | 1920×1080 | 1.5 | 21 px |

三个 SVG 均含输入绑定页面、冲突说明、恢复默认、应用/取消以及“图标 + 轮廓 + 文本”的犯规预览。validator 检查 safe area、顶层区域不重叠、字号和 high-DPI 映射。

## 验收命令与接入门

```bash
python3 scripts/validate_phase5_product_ux.py assets/modern/product_ux/product_ux_v1.json
python3 -m unittest tests.visual.test_phase5_product_ux
```

六个固定坏输入覆盖：缺英文规则标题、只靠颜色、AI 区间重叠、默认键冲突、虚假 runtime 连接和越出 safe area。

- [ ] runtime 从权威 adjudication result 消费 reason，不从视觉或物理状态猜测。
- [ ] runtime capability 出现前不展示规则提示为“已接入”。
- [ ] AI 每档达到完整固定试验目标区间，并发布原始结果与 Wilson 区间。
- [ ] 输入保存前完成冲突检查，写入失败不破坏上一版本。
- [ ] 纯键盘可以进入、修改、恢复、应用和退出全部设置。
- [ ] 三个响应式 case 完成截图回归和 135% 字号 reflow 检查。
- [ ] 色觉模拟下图标轮廓和文字仍能独立传达 reason。
- [ ] C++/CMake、生产物理默认及 frozen evidence 保持不变。
