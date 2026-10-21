# Phase 3 GPU HUD / ProductShell Capture 验收契约

日期：2026-07-27  
当前结论：**模板与 validator PASS；GPU capture、人工 review、用户研究均未执行**

## 为什么是 594 张

机器矩阵显式展开全部组合，避免只截一张“好看样例”后代表所有状态：

- HUD：5 states × 3 viewports × 4 camera presets × 2 palettes × 3 UI scales
  = **360**。
- ProductShell：13 scenarios × 3 viewports × 2 palettes × 3 UI scales
  = **234**。
- 合计 **594** 个独立 capture ID；缺一个、重复一个或 selector 漂移都会失败。

三档 viewport 为 1280×720、1920×1080，以及 1280×720 logical →
1920×1080 physical 的 144-DPI Retina。四镜头固定为 `player_low`、
`elevated`、`overhead`、`broadcast`；UI scale 固定为 0.85、1.0、1.35。

Shell 的 13 个场景覆盖八个真实 ProductShell 屏，并显式增加 Settings 的 gameplay、input、
accessibility、audio、display 五个 category。每个 settings 场景记录首个 focus item
以及 `top_all_rows_fit` 或 `top_all_rows_fit_or_reflow_columns`；当前产品壳没有滚动容器，
因此不能声称测试了不存在的 scroll offset。若未来增加滚动，C++ source/matrix 必须同步
新增场景，否则 exact matrix validator 会失败。另用 main menu 的 training unavailable/available、
setup、training error/session、replay error/playback 和 help 场景覆盖
focus、selected、error、disabled。`match` 不是 Shell surface；生产路由固定为 GameplayHud，
因此 Match 视觉状态只进入上面的 360-case HUD 矩阵。

## 预采集 stale-contract / reachability 纠错

在任何实际 capture 或人工复审开始前，public controller 与唯一 production UI route 审计发现
旧 594-case 合同有三个不可真实复现的 selector。此次重签只替换同规模 factor，仍为
360 HUD + 234 Shell = 594，没有修改性能、字号、图标或状态指标阈值，也没有把任何
`PENDING` 晋升为 `PASS`：

1. 删除 `match_default` 的 18 个 Shell case。`ProductScreen::Match` 在
   `product_ui_frame_coordinator.cpp` 唯一路由到 `GameplayHud`，不可能同时提供 Shell focus、
   disabled 与 Shell icon 指标。替换为可经有效训练包到达的
   `main_menu_training_available`：`screen=main_menu`、`variant=training_pack_loaded`、
   training focused、expanded icons ≥7、focus ≥1、selected/error/disabled 均为 0。
2. 删除无错误状态下不可到达的 `training_catalog_empty`。public
   `loadTrainingCatalog` 失败会保留错误并进入 TrainingCatalog，因此替换为
   `training_catalog_invalid_pack_error`：empty item disabled、无 focus、error ≥1、
   disabled ≥1、expanded icons ≥2。
3. 删除错误假设 help 无 focus 且 6 项全 disabled 的 `help_disabled_reference`。真实 Help
   前 5 项为 reference disabled，`help_back` enabled 且自动获得 focus；替换为
   `help_back_focused`，focus ≥1、disabled ≥5，其余既有门不变。

manifest 的 `pre_capture_contract_corrections` 机器记录固定三次替换，并逐项声明
`capture_count_delta=0`、`machine_thresholds_changed=false`、
`evidence_status_advanced=false`。这是采集前修正错误测试前提，不是降低产品质量门。

## 每张截图的机器硬门

- 必须是 `rendered_automation_gpu_readback` 或 `gpu_framebuffer_readback`；桌面截屏、
  SVG flat proof 和用户研究截图均不接受。
- 路径、PNG/PPM 文件、物理尺寸和 SHA-256 必须一致；runtime 需绑定 git revision、
  executable SHA、renderer、GPU/driver、OS 与 UTC。
- safe-area violations、truncated text runs、overlap pairs、debug nodes、opaque icons、
  unexpanded icons、frame-time glyph rasterizations 必须全部为 **0**。
- 最小实际字号不得低于 `14 × deviceScale × uiScale` physical px；这与现有
  85%--135% UI scale 求解一致，不另设阈值覆盖参数。
- 每个状态必须达到矩阵规定的 expanded icon 与 focus/selected/error/disabled 最小数；
  不需要某状态时数量必须为 0，避免错误状态“多画也算过”。
- 复用现有性能门：warmup ≥120、samples ≥300、P95 total frame ≤16.7 ms、
  complete-scene draw calls ≤100、texture residency ≤256 MiB。validator 不提供放宽参数。
- HUD/Shell token 与 manifest 必须匹配已冻结的四份 SHA；两个 flat proof 不属于这些
  runtime asset bindings。

## 人工复审门

人工模板同样预建 594 个条目和七个轴：构图/玩法遮挡、文字层级、图标与状态冗余、
双 palette 可读性、focus/selected/error/disabled、镜头一致性、debug/shipping 隔离。
HUD 的 Shell-state 轴必须为 N/A；Shell 的 camera 轴必须为 N/A。

只有在以下条件全部满足后，独立技术美术才可复制并填写模板：

1. 实际 capture manifest 的 594 个 `machine_status` 全部为 PASS。
2. manual review 绑定该 manifest 的精确 SHA-256，并为每个 capture 绑定截图 SHA。
3. 所有 PENDING 状态和证据占位符被逐张原分辨率观察替换。
4. manual summary 与逐轴 FAIL 一致。

模板自身明确 `golden_creation_authorized=false`。通过本验收也不能自动创建 golden；
用户研究仍须按计划由真实参与者单独执行，不能由 automation 或 flat proof 代替。

## 产物与 SHA-256

| 文件 | SHA-256 |
| --- | --- |
| `phase3-gpu-capture-manifest-v1.schema.json` | `51c8944bae02f5b73448fef6c14e590c16f5e4c286475934112f945d2a2a17e6` |
| `phase3-gpu-capture-matrix-v1.pending.json` | `e4f4d3357e84ecdece03a3f271a24ced6985b06a80cd0d8560ee25781182a86c` |
| `phase3-gpu-manual-review-v1.schema.json` | `6844ac210bb65f7c4540ca8c1e196584d320eb5c7c161ba44b75c2cd70260e2b` |
| `phase3-gpu-manual-review-v1.pending.json` | `9e407b3feac4e59822250048e713185255a17b542065ce40b607b68911450081` |

## 复验

```bash
python3 scripts/generate_phase3_gpu_capture_templates.py --output-dir docs/art
python3 scripts/validate_phase3_gpu_capture_evidence.py \
  docs/art/phase3-gpu-capture-matrix-v1.pending.json \
  --manual-review docs/art/phase3-gpu-manual-review-v1.pending.json \
  --pending-template
python3 -m unittest -v tests.visual.test_phase3_gpu_capture_evidence
```

模板 validator 的成功输出包含 `acceptance=NOT_RUN`，不会输出 GPU evidence PASS。
实际 capture 完成后去掉 `--pending-template`；只有实际文件、机器门与可选人工 review
全部匹配时，CLI 才能输出 evidence PASS。

人工文件本身可以是结构完整的 `FAIL` 证据，但默认 CLI 会返回非零并输出
`manual_outcome=FAIL acceptance=FAIL`，不再把它说成 evidence PASS。只有在归档/检查失败
证据时才使用 `--allow-failed-manual`；该选项仅让结构验证返回零，输出仍明确
`acceptance=FAIL`，不能用于发布门。

18/18 单测包括：模板确定性、594 exact coverage、三项 production-route/public-controller
可达性合同、logical passing fixture，以及对
PENDING/flat proof 冒充、缺 case、opaque/unexpanded icon、debug 泄漏、safe-area、
重叠、截断、小字号、状态 indicator 缺失、性能超门/阈值篡改、机器未过先人工审、
manifest/截图哈希错绑、人工 PENDING 与 surface-specific N/A 错误的 fail-closed 检查。
