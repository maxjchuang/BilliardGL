# Phase 2 / M2 Production Modern 独立技术美术审查

审查日期：2026-07-27  
审查角色：Independent Technical Artist  
当前结论：**FAIL — v5 六景 6/6 人工失败、6/6 机器失败；性能门已全通过**

## v5 当前结论

- 六景目录：`build/test-output/gltf-six-v5`
- glTF SHA：`0f7d8a6c2f898ee8df81f09c4b84011759ad4b3a16c1a6b8ae1cadde1c493d42`
- SHA 绑定人工轴：`docs/art/phase2-production-modern-v5-manual-review.json`
- 完整机器报告：`/tmp/phase2-v5-ta-review.json`
- 性能：warmup `120`、sample `300`、P95 `3.81142 ms`、draw calls `100`、
  texture residency `27.0143 MiB`，全部 PASS。
- 人工阻断：low_aim cue 完全不可见且 guide 从球内开始；1/8/11/15 仍呈段码扭曲；
  球缺少紧凑 contact core；felt 有明显 screen-space 条纹/摩尔纹；袋口 cloth 裂齿虽消失，
  但 mouth/leather/void 仍脱节。
- 机器阻断：ball hero contrast `1.1273 < 2.2`；break table luma
  `.02424 < .12`；low cue luma `.11810 < .20`；pocket mouth edge
  `.00996 < .035`；tactical full-playfield luma `.03133 < .10`；post action
  luma `.02184 < .08`。

v5 之后的修复资产使用新的 glTF SHA，不能反向改变 v5 判定；必须由下一版六景复验。

## v4 历史冻结证据身份

- 六景目录：`build/test-output/gltf-six-v4`
- Capture manifest：`build/test-output/gltf-six-v4/manifest.json`
- Renderer：modern；macOS OpenGL 3.3 Core
- 输出：1920×1080；Medium / 4× MSAA
- 固定阈值：`assets/modern/lookdev/focused_practice_hall_v1.lookdev.json`
- SHA 绑定人工轴：`docs/art/phase2-production-modern-v4-manual-review.json`
- 机器报告：`/tmp/phase2-v4-ta-review.json`

截图 SHA-256：

| Scene | SHA-256 |
| --- | --- |
| break | `02f2efc3fe1a74bfb6f473c893e265765540fa92d9e32ac952d67e3b7ab11661` |
| low_aim | `03c4b4726355b641f4b6c1827b72240c495aea1c16cbf8cd16d62d00793e4615` |
| ball_closeup | `6daa0d1bac2b1c4dc2c086fa22dc2852e353445aa5ad61980f1674f02c96c25c` |
| pocket_closeup | `739eb39e3bb5710a18ba7b861343a89fe964d21dce16dac9b8eb5b8142f2bfa6` |
| tactical_overhead | `63d758a408409146e91f58d2856c169adbbda3fc250fe0befc17d863a6ba9dc8` |
| post_shot_observe | `a3957a7d8f9efa959af0806109c80c95ab1377c61fb6b14e3512aa9eb18cda37` |

截图或 manifest 变化后，以上人工结论自动失效，必须重新绑定 SHA 审查。

## v4 历史人工审查矩阵

| 场景 | 构图 | 材质/色彩 | 接触阴影 | 对齐/比例 | 号码/seam | Venue/焦点 | 结论 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| break | FAIL：灯具仍裁切且无 cue | FAIL：felt 偏青，wood 红且高频 | FAIL：rack 与母球漂浮 | FAIL：袋口/库鼻压黑、cloth 齿 | FAIL：rack 号码不可稳定辨识 | FAIL：背景层次弱 | **FAIL** |
| low_aim | PASS：cue/guide 已出现 | FAIL：台呢与木材偏色 | FAIL：母球漂浮 | FAIL：cue 从球内起步/触球位置偏低 | FAIL：目标球号不足 | FAIL：背景层次弱 | **FAIL** |
| ball_closeup | PASS：四球居中 | FAIL：树脂层次弱 | FAIL：无紧凑 contact core | N/A | FAIL：8/11/15 数字破碎、条纹锯齿 | N/A | **FAIL** |
| pocket_closeup | PASS：目标袋已居中 | FAIL：felt/wood 偏色 | N/A | FAIL：袋口台呢有三个明显三角齿/裂缝 | N/A | N/A | **FAIL** |
| tactical_overhead | PASS：完整桌面已恢复 | FAIL：桌面过暗、色彩偏离 | FAIL：球体 grounding 弱 | FAIL：袋口/库鼻黑位挤死 | FAIL：号码不可读 | FAIL：venue 层次不足 | **FAIL** |
| post_shot_observe | PASS：散球构图可读 | FAIL：felt/wood/lamp 层级错误 | FAIL：球体漂浮 | FAIL：近袋/库边压黑 | FAIL：号码/seam 证据不足 | FAIL：灯具裁切 | **FAIL** |

N/A 只表示该轴不适用，不能抵消场景级失败。机器 ROI 通过也不能覆盖人工轴失败。

## v4 Medium 固定阈值机器结果

| 场景 / ROI | Actual / Required |
| --- | --- |
| break / rack | contrast `1.2018 / ≥1.8` |
| break / table | luma `.037584 / ≥.12`；non-black `.91031 / ≥.94`；edge `.012883 / ≥.015` |
| low_aim / cue_ball | luma `.16146 / ≥.20`；contrast `1.3841 / ≥2.5` |
| low_aim / target_pocket | contrast `1.0524 / ≥2.4`；edge `.0000 / ≥.02` |
| ball_closeup / hero_ball | contrast `1.0541 / ≥2.2` |
| ball_closeup / contact_shadow | contrast `1.0048 / ≥1.25` |
| post_shot_observe / action_corridor | luma `.032728 / ≥.08`；edge `.015036 / ≥.018` |
| tactical_overhead / full_playfield | luma `.049398 / ≥.10`；edge `.014068 / ≥.02` |
| tactical_overhead / ball_field | luma `.065467 / ≥.09`；edge `.015555 / ≥.025` |

机器通过项：break/background、low_aim/aim_corridor、post_shot/environment，以及
pocket_closeup 两个 ROI。pocket_closeup 仍因可见几何裂缝人工 FAIL。

### 性能门

| 指标 | Actual | 门限 | 结论 |
| --- | ---: | ---: | --- |
| Warmup | 120 frames | ≥120 | PASS |
| Sample | 300 frames | ≥300 | PASS |
| P95 total frame | 21.0068 ms | ≤16.7 ms | **FAIL** |
| Complete-scene draw calls | 100 | ≤100 | PASS |
| Texture residency | 19.0143 MiB | ≤256 MiB | PASS |

仓库仍无 approved golden，因此 `review_ssim_floor=0.93` 保持 unresolved，不能静默跳过或
用当前失败截图自批准。

## v4 后资产修复状态

v4 之后已交付 `docs/art/phase2-ta-asset-repair-v2.md` 中的离线通过资产：连续六袋口
cloth 拓扑、2048×1024 / 24 px inset 球图集、1/8/11/15 近景，以及 felt/walnut
色彩和频率校正。这些改动不能反向改变 v4 判定；必须由重载指定 glTF SHA 的 v5 六景证明。

## 复验命令

```bash
python3 scripts/validate_phase2_production_visuals.py \
  build/test-output/<six-scene-modern>/manifest.json \
  --manual-review build/test-output/<six-scene-modern>/ta-manual-review.json \
  --output build/test-output/<six-scene-modern>/ta-review-report.json
python3 -m unittest tests.visual.test_phase2_production_visuals
```

validator 是只读消费者，不提供阈值覆盖参数。正式复验仍需至少 120 帧 warmup、300 帧
采样、P95 ≤16.7 ms、draw calls ≤100、纹理驻留 ≤256 MiB，并记录测量方法。
