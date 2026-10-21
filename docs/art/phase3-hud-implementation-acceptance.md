# Phase 3 HUD 实现与验收清单

日期：2026-07-27  
规格：`assets/modern/hud/focused_practice_hud_v1.hud.json`  
范围：产品 HUD 与 UX 数据绑定；不改变 renderer 架构、构建系统或物理规则

## 0. Shipping token 技术美术审计

- 现有五状态、双布局、1280×720 / 1920×1080 / Retina、safe area、FreeType/SDF
  和 debug overlay 隔离契约审计为 `PASS_EXISTING`。
- 阻塞 GPU 接入的真实缺口是 `UiPrimitiveKind::Icon` 仅携带字符串 token，却没有仓库
  自有的可展开几何；同时标准与色觉友好配色未形成运行时 token 集。
- `focused_practice_hud_shipping_tokens_v1.json` 现提供 16 个原创 24×24 归一化图标，
  只使用 rect/circle/polyline/polygon，覆盖 power、tip、camera、foul、help、球组及
  犯规别名；文本继续复用现有 FreeType/SDF atlas。
- `standard` 与 `color_vision_safe` 两套 palette 均保留文字、形状、数值/纹理冗余；
  犯规、球组和力度信息不得只靠颜色。
- `focused_practice_hud_shipping_tokens_flat_proof_v1.svg` 仅是静态 token/排版证明，
  不是 GPU rendered capture，也不是人工用户研究。正式完成仍需要 shipping draw-list
  展开这些命令后的真实截图和交互验收。

## 1. 产品目标

HUD 的首要任务是在不遮挡球路的前提下，让玩家在 30 秒内完成第一杆：知道当前玩家、球组和目标，进入瞄准，理解力度与预计杆速，必要时调整母球击点，然后蓄力并释放。背景材质与球台提供沉浸感，HUD 只提供当前决策需要的信息。

运行时必须支持五个独立状态：

| 状态 | 必须显示 | 不应显示 |
| --- | --- | --- |
| `default` | 当前玩家、球组、目标球/目标袋、镜头、assist 短提示 | 力度、tip offset、调试数据 |
| `aiming` | 默认信息、力度刻度、百分比、`EST.` 杆速、tip offset | 击球后统计、犯规层 |
| `post_shot` | 玩家/目标、击球结束、母球行程、接触摘要、回看提示 | 蓄力与 tip 控件 |
| `foul` | `FOUL` 文字、警告图形、原因、处理结果 | 只用红色但无文字的提示 |
| `help` | 键鼠/触控板提示、首杆路径；暂停 shot input | 调试 telemetry、未晋升能力 |

四张评审构图中的 `post_shot_foul` 同时展示两个可叠加层，仅用于减少评审文件数量；实现仍须保持 `post_shot` 和 `foul` 两个独立状态。

## 2. 数据权威与绑定

- [ ] HUD 只读取 presentation snapshot / view model，不直接读取或修改 simulation core。
- [ ] 当前玩家、球组、目标球、目标袋来自规则或 presentation snapshot 的明确字段；不得从球颜色或屏幕位置反推。
- [ ] `power_percent` 来自当前 shot intent，范围钳制到 `0–100`，显示使用整数百分比。
- [ ] `estimated_cue_speed_mps` 由当前批准的 shot-model presentation 映射提供，固定显示 `EST.` 与 `m/s`；不得标成测量值或权威物理值。
- [ ] tip offset 使用球半径归一化坐标 `[-1R,+1R]`，显示 X/Y 数字；控件不能提交球轮廓外的值。
- [ ] 镜头 preset 与 assist level 来自 presentation state；短提示最长显示 3 秒，然后只保留可再次唤出的帮助入口。
- [ ] 犯规原因与处理结果来自规则事件，不能由 HUD 根据动画猜测。
- [ ] snapshot 缺字段时显示明确的中性缺省文本，不复用上一杆的陈旧值。

## 3. 两种布局与 safe area

### 1920×1080

- Safe area：左 64、上 36、右 64、下 54 px。
- 状态卡固定左上，镜头/assist 短提示固定右上。
- 瞄准时力度区在左下，tip offset 在右下；二者不可覆盖可打台面中心。
- 犯规条位于上方中央，不能覆盖左右状态卡。

### 1024×768

- Safe area：左 32、上 24、右 32、下 32 px。
- 使用 compact profile，不把 1920 布局按 0.533 倍机械缩小。
- 最小文字 13 px；帮助面板和所有提示不得越出 safe area。
- compact 犯规条移到顶部状态卡下方，避免与左右卡重叠。

### DPI 与用户缩放

- [ ] OS density scale 使用 `clamp(os_dpi / 96, 0.85, 1.50)`。
- [ ] 用户 UI scale 支持 `0.85–1.35`，与 OS scale 相乘后重新执行布局约束。
- [ ] 缩放后卡片边缘和 1 px 线条进行像素对齐；文本保持亚像素定位。
- [ ] 宽高比 `<1.55` 或宽度 `<1280` 时选择 compact profile。
- [ ] 用户最大缩放时允许上下重排，不允许裁字、覆盖或退回小于 13 px 字号。

### 机器验证视口证据

manifest 的 `responsive_validation_cases` 固定三组工程门禁，并逐状态检查所有矩形位于物理 safe area 且互不重叠：

| Case | 逻辑 / 物理视口 | Safe area（L/T/R/B） | 最小物理字号 | 关键重排 |
| --- | --- | --- | ---: | --- |
| `minimum_wide_1280x720_96dpi` | 1280×720 / 1280×720 | 43 / 24 / 43 / 36 | 14 px | 犯规条移到左右顶卡下方；帮助面板缩为 760×620 |
| `reference_1920x1080_96dpi` | 1920×1080 / 1920×1080 | 64 / 36 / 64 / 54 | 14 px | 使用 wide reference 布局 |
| `high_dpi_1920x1080_144dpi` | 1280×720 / 1920×1080 | 64 / 36 / 64 / 54 物理 px | 21 px | 复用 1280 reflow 后按 1.5 倍像素对齐 |

validator 不接受 manifest 中预填的“通过”布尔值，而是重新计算每个 active state 的 safe-area containment 与两两矩形相交。

## 4. 视觉层级与可访问性

- [ ] 字体只使用系统字体栈，字重限制为 400/500；数字使用 tabular figures。
- [ ] display / primary / body / caption 四层在两种 profile 中保持顺序，不用全大写替代层级。
- [ ] 普通文本对实际面板背景对比度至少 4.5:1，大字和非文字控件至少 3:1。
- [ ] 球组同时使用 `SOLIDS/STRIPES` 文字、实心/环形图形、球号和颜色。
- [ ] 目标球始终显示号码；目标袋始终显示方向文字或位置图形。
- [ ] 犯规同时使用文字、警告三角形和颜色。
- [ ] 力度同时使用分段条、刻度、百分比和预计杆速，不用单一颜色长度表达。
- [ ] tip offset 同时使用球轮廓、十字线、位置点和 X/Y 数值。
- [ ] assist 状态同时显示文字和图标；颜色只用于辅助强调。
- [ ] 正常 gameplay 禁止持续闪烁；犯规进入动画最多一次，并尊重 reduced-motion 设置。

## 5. 输入提示与 30 秒首杆

默认键鼠提示：`TAB` 进入/退出瞄准，`SPACE` 蓄力/释放，方向键调整 tip offset，鼠标拖动环绕，滚轮缩放，`Q/E` 切镜头，`F1` 帮助，`ESC` 关闭。

首杆观测脚本：

| 时间 | 玩家应完成 | HUD 证据 |
| --- | --- | --- |
| 0–3 s | 读当前玩家、球组和目标 | 左上状态卡，无自动消失 |
| 3–7 s | 接受默认镜头或轻微环绕 | 右上镜头/assist 短提示 |
| 7–11 s | 按 `TAB` 进入瞄准 | 力度和 tip offset 同时出现 |
| 11–18 s | 确认瞄准，可选调整击点 | 十字线与 X/Y 值同步 |
| 18–27 s | 按住 `SPACE` 蓄力 | 刻度、百分比与预计杆速同步 |
| 27–30 s | 松开完成第一杆 | 切换到 post-shot，输入提示更新 |

- [ ] 新用户测试的中位首杆时间不超过 30 秒。
- [ ] 玩家不调整 tip offset 也能完成路径；可选步骤不能阻断第一杆。
- [ ] 同时出现的键鼠提示不超过 3 条，优先级为阻断错误、当前动作、镜头、帮助。
- [ ] 帮助打开时暂停 shot input，关闭后恢复之前的产品状态而非重置 shot intent。

### 无需等待用户研究的自动化代理指标

`hud_first_shot_keyboard_mouse_v1` 可先作为工程验收门：

- 默认状态卡在启动后 500 ms 内可见；11 s 前进入 aiming。
- `TAB` 后 100 ms 内 power 与 tip 控件同时可见。
- 单次 `SPACE` hold 至少采集 3 个单调不减的 power 样本，并显示 `EST.` 与 `m/s`。
- tip offset 步骤可跳过，自动化不得代替玩家自动瞄准或自动击球。
- 30 s 内只允许发送 1 个 release 命令；release 后 250 ms 内进入 post-shot。
- 事件顺序固定为 `hud_default_visible → aim_toggled → power_hold_started → power_sampled → shot_released → hud_post_shot_visible`。
- 任意时刻上下文输入提示不超过 3 条。

这些指标验证状态机、可见性和首杆可完成性，不宣称替代后续真实用户研究。

## 6. 调试层隔离

- [ ] 产品 HUD 树中不存在 FPS、frame time、draw calls、tris 或 snapshot tick 节点。
- [ ] 调试数据只属于 `billiardgl_debug_telemetry_v1` overlay，默认关闭，用 `F3` 独立切换。
- [ ] 调试层不参与产品 HUD 的 safe-area 布局，不会挤动产品控件。
- [ ] release screenshot、视觉基线和用户测试强制关闭调试层。
- [ ] 自动化可分别查询产品 HUD 可见性与调试 overlay 可见性。

## 7. Capability-gated 占位

- `cue_elevation` 仅在 `capability.cue_elevation_ui` 与 `physics.cue_elevation_promoted` 同时为真时创建节点。
- `chalk` 仅在 `capability.chalk_ui` 与 `physics.chalk_model_promoted` 同时为真时创建节点。
- 默认 manifest、全部 mockup、帮助文本和输入提示均不显示这两项。
- 未满足条件时必须完全省略节点，不显示灰色按钮、锁图标或“即将推出”。
- UI 能力开启不能替代物理晋升；任何单侧 flag 都必须保持隐藏。

## 8. 实现顺序

1. 建立只读 `HudViewModel`，映射 presentation snapshot、shot intent、camera 和 rules event。
2. 实现 manifest profile 选择、safe-area 和 DPI/user-scale 求解。
3. 实现五态 HUD state reducer；状态切换不得改变 physics 或 shot intent。
4. 实现状态卡、context hint、power、tip offset、post-shot、foul 和 help 七类产品元素。
5. 将 debug telemetry 接到独立 overlay root。
6. 加入键鼠提示与首杆引导埋点；引导只提示，不自动瞄准或击球。
7. 用 rendered automation 在两种分辨率和五态采集截图，并与 SVG 构图逐项对照。

## 9. 最终验收门

- [ ] `python3 scripts/validate_phase3_hud_package.py assets/modern/hud/focused_practice_hud_v1.hud.json` 通过。
- [ ] `python3 -m unittest tests.visual.test_phase3_hud_package` 通过。
- [ ] 1920×1080 和 1024×768 的 default、aiming、post-shot、foul、help 全部生成 rendered capture。
- [ ] 所有产品元素在 safe area 内，状态内无重叠、裁切或小于最小字号。
- [ ] HUD 不遮挡母球、瞄准走廊、目标袋和关键 HUD-less gameplay ROI。
- [ ] 色觉模拟与灰度截图中仍可识别球组、目标、力度、犯规和 tip offset。
- [ ] `EST.` 杆速与当前 shot model 映射一致，tip offset 显示与提交值一致。
- [ ] 犯规原因/处理、玩家切换和 post-shot 统计来自同一 snapshot/event 序列。
- [ ] debug overlay 隔离测试通过，release capture 中不存在调试数据。
- [ ] cue elevation / chalk 双 flag 组合测试覆盖四种组合，只有双真时可见。
- [ ] 首次用户 30 秒首杆路径通过，不依赖未晋升物理能力。
