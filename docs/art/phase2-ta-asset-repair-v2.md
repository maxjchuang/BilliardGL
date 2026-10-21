# Phase 2 技术美术资产修复包 v2

日期：2026-07-27  
状态：离线资产门通过，等待双哈希绑定的 production modern 六景复验

## 修复结果

### 袋口台呢拓扑

- 用沿球台边界连续行走的解析弧线轮廓替代“按网格中心删除三角形”。
- 四个角袋使用 `0.132 m × 0.56` 半径的四分之一圆弧；两个中袋使用
  `0.086 m × 0.56` 半径的半圆弧。
- LOD0/1/2 台桌三角形为 `2704 / 356 / 180`；台呢自身分别为
  `100 / 68 / 36` 个三角形。
- 离线门逐三角形检查 index 范围、非退化、`+Y` 一致 winding、边 incidence
  只能为 1 或 2、boundary 顶点 degree 必须为 2、且全表只有一个连续边界环；每个
  袋口至少有五个解析圆弧采样。

### 球号 atlas

- 从 1024×512 改为 4096×512；仍为 8×2 tile，每 tile 512×256。每个球的 tile
  保持球面经纬展开所需的 2:1 比例，避免把号码横向拉成腰带。
- 非对称二维 transition probe 同时使用 `红← / 绿→` 和 `蓝↑ / 橙↓`。修复球号
  专用 runtime V 变换后，front 与旋转 33° 捕获均保持四个方向标记和 `15` 的源图
  顺序/handedness；最终 atlas 因而保持 `upright_source`，不使用 whole-string、
  逐字符或源图垂直镜像补偿。
- KHR texture transform 对每 tile 使用 24 px 安全内缩；数字不得进入 gutter。
- 号码圆使用 38 px 半径；数字改为原创 7×9 粗笔画字形、4 px cell。`1` 使用明显
  左向起笔和全宽底座，`5` 使用连续左上竖、全宽中横与连续右下竖；在常用距离缩小
  采样后仍能把 `1/8/11/15` 与镜像、`2` 或 `51` 区分。
- 条纹上下边界位于 tile 安全内容区的 32% / 68%；tile 外围保留身份底色扩张。
- 新增 1/8/11/15 原生像素近景证据
  `focused_practice_hall_ball_identity_closeup_1_8_11_15_v2.png`，仅用于审查，
  不计入 runtime residency。
- 离线门解码 PNG，检查尺寸/格式/filter、四个难读球的 ink 像素下限和 gutter 泄漏，
  同时锁定 16 个材质的 24 px transform。

### Felt / walnut 校正

| 参数 | Before | After |
| --- | --- | --- |
| Felt baseColor sRGB8 | `[20,111,126]` | `[32,92,99]` |
| Felt chroma range | `106` | `67` |
| Felt UV repeat | `6×12` | `1×1` |
| Walnut baseColor sRGB8 | `[78,34,19]` | `[62,42,30]` |
| Walnut R/G ratio | `2.2941` | `1.4762` |
| Walnut grain cycles | `18` | `5` |
| Walnut pore cells | `72` | `24` |
| Walnut UV repeat | `3×2` | `2×1` |

完整 before/after 契约写入
`focused_practice_hall_pbr_v1.params.json/color_frequency_revision`，validator 会拒绝缺失、
漂移或“after 不低于 before”的饱和度、红偏和空间频率声明。

## 可复现产物

| 文件 | SHA-256 |
| --- | --- |
| `focused_practice_hall_modular_v1.gltf` | `340b790fcc921ce958e36d36640de315fbb7fdfa3305cc45bac63b77d136919b` |
| `focused_practice_hall_ball_identity_atlas_v1.png` | `4958d3b8443ddd3f9a3b559ca1e22b6130c169d733ec73c7b918aadcd14fd60e` |
| `focused_practice_hall_ball_identity_closeup_1_8_11_15_v2.png` | `dc4d45c650ea7271d0c790ad5f56785196589db4f84547c72570548e49e778ca` |
| `focused_practice_hall_felt_basecolor_srgb_v1.png` | `12d05772dcc2fe94ce017c71af3048ce9733da3ad4c05e49ea049f98fa1b3985` |
| `focused_practice_hall_walnut_basecolor_srgb_v1.png` | `f8f6a6381bb57cd1d9827c58675adba08d5dc4dbdfb7fa16e257cd62ec81a62e` |
| `focused_practice_hall_pbr_v1.params.json` | `252bd726263a51bb85ab02ad171f260253df92d27c5d0aa165ecf72e2ff1b2f5` |

Runtime full-mip texture预算为 `26,913,444 bytes`，其中 atlas `11,184,828 bytes`、
PBR `15,728,616 bytes`；远低于 256 MiB production Medium 上限。

## 复验

```bash
python3 scripts/generate_phase2_modular_gltf.py --output-dir assets/modern/phase2
python3 scripts/validate_phase2_modular_gltf.py \
  assets/modern/phase2/focused_practice_hall_modular_v1.gltf
python3 scripts/validate_phase2_pbr_package.py assets/modern/phase2
python3 -m unittest \
  tests.visual.test_phase2_modular_gltf \
  tests.visual.test_phase2_pbr_package
```

本包只证明资产离线契约。袋口视觉、球号实际球面投影、接触阴影、曝光和性能仍必须由
重载上述 glTF SHA、atlas SHA（以及推荐的 PBR params SHA）的六景按冻结阈值和
SHA 绑定人工轴独立复验。仅绑定 glTF 不足以证明捕获使用了正确球号版本。
