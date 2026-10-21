# Phase 5 / M4 产品 UX 验收包

`product_ux_v1.json` 是基于 `wpa_heyball_2025_08_16_v1` 与 Phase 3 HUD 的机器可读产品合同，覆盖规则提示、AI 难度、设置、键鼠重绑定与可访问性。

包内三张 SVG 分别验证 1280×720、1920×1080 和 144 DPI（1280×720 logical / 1920×1080 physical）布局。全部图形和文案均为 BilliardGL 原创，不使用竞品或第三方资产。

```bash
python3 scripts/generate_phase5_product_ux.py
python3 scripts/validate_phase5_product_ux.py assets/modern/product_ux/product_ux_v1.json
python3 -m unittest tests.visual.test_phase5_product_ux
```

规则 reason mapper 目前只在规则库及单测中存在，尚未接入主游戏 runtime。所有可见 reason 都保持 `capability_gated`；AI 与输入重绑定同样不会因为内容和 UI 已完成而被假定可用。
