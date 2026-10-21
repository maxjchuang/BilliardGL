# Phase 3 / M2 Shipping HUD Token 技术美术验收

日期：2026-07-27  
结论：**离线 token / flat proof PASS；GPU 展开截图与人工交互验收待完成**

## 审计结论

| 项目 | 结论 | 证据 |
| --- | --- | --- |
| default / aiming / post-shot / foul / help | PASS | 主 HUD manifest 的五个独立 runtime state |
| 1280×720 / 1920×1080 / Retina | PASS | 三个 responsive validation case，Retina 为 1280×720 logical → 1920×1080 physical |
| safe area / 最小字号 | PASS | 720p 为 43/24/43/36 px；1080p 为 64/36/64/54 px；最小 logical 13 px、Retina physical 21 px |
| FreeType / SDF 复用 | PASS | token contract 指定 existing FreeType SDF atlas，不附带字体文件 |
| 标准与色觉友好配色 | PASS | 两套 palette；文字、形状、数值/纹理为强制冗余通道 |
| debug / product 分离 | PASS | 独立 namespace、独立 layout nodes、默认隐藏、release capture 禁止 |
| Icon GPU 几何 | PASS（资产） | 16 个原创 normalized 24×24 token，仅含 rect/circle/polyline/polygon |
| C++ CPU 消费/展开 | PASS | strict loader；固定拓扑三角展开、clip、线宽 pixel snap、source/resolved token、standard/CVD palette；五态真实 draw-list E2E |
| GPU rendered capture | **待完成** | flat SVG 不作为 GPU 截图 |
| 人工用户研究 | **待完成** | automation proxy 和 flat proof 均不能替代用户研究 |

## Shipping token 增量

- `power_bars`：四级柱状轮廓，长度、段数、百分比共同表达力度。
- `tip_target`、`tip_ball_outline`、`tip_position_marker`：球轮廓、十字线和位置点。
- `camera_frame`：四角取景框与镜头圆，不依赖文字也能识别镜头语义。
- `warning_triangle` 及犯规 aliases：始终与 `FOUL`、原因和处理文字共同出现。
- `help_circle`：圆形问号；帮助面板仍使用现有 SDF 文本层级。
- solid / stripe / open / unknown 球组符号：填充、横带、菱形和问号菱形，颜色只做冗余。

现有 C++ draw-list 已能输出 `Panel`、`SolidRect`、`Line`、`Icon`、`GlyphQuad`，但此前
`Icon.token` 没有仓库自有几何定义。本增量规定 CPU 在 GPU submit 前将 normalized
commands 展开为既有 solid/line triangles；运行时不解析 SVG，也不引入第二套字体系统。

## 产物身份

| 文件 | SHA-256 |
| --- | --- |
| `focused_practice_hud_v1.hud.json` | `969d3b8bc7de46600d03f77a10e577fe560e2106a612e708739a96a73c27a6e1` |
| `focused_practice_hud_shipping_tokens_v1.json` | `abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860` |
| `focused_practice_hud_shipping_tokens_flat_proof_v1.svg` | `8058de94319eabcb6f0d2c15599dc5854ba2924bc403fcce9cadffab7e3818ca` |

## 复验

```bash
python3 scripts/generate_phase3_hud_package.py --output-dir assets/modern/hud
python3 scripts/validate_phase3_hud_package.py \
  assets/modern/hud/focused_practice_hud_v1.hud.json
python3 -m unittest tests.visual.test_phase3_hud_package
```

离线 validator 固定检查两套 palette、五状态、图标命令边界、alias 目标、SDF/字号、
720p/1080p/Retina safe area、debug namespace，以及 flat proof 对 GPU capture / user
research 的否认元数据。正式 GPU 验收必须另外捕获 product HUD，不能复用 flat proof。

C++ 侧另由 `BilliardsHudShippingTokenTests` 与
`BilliardsShippingHudRenderTests` 验证 strict failure、alias/cycle、两套 palette、
720p/1080p/Retina、85%--135% UI scale、CPU clip、确定性字节/顶点和五态真实
draw-list 展开。该二进制不链接 OpenGL/Cocoa/SDL；结果仍不是 GPU screenshot。
当前 HUD v1 不包含完整 ProductShell icon，Shell 未批准 token 必须显式失败，待独立
Shell shipping token 包完成后才能关闭产品壳图标资产门。
