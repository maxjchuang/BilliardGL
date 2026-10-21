# M1 “Focused Practice Hall”美术验收清单

日期：2026-07-27
适用范围：ModernRenderer 垂直切片的 graybox、lookdev 和游戏内截图
机器目标：[focused_practice_hall_v1.lookdev.json](../../assets/modern/lookdev/focused_practice_hall_v1.lookdev.json)

## 1. M1 可立即进入的内容

以下内容不等待生产球台高模，可以进入 M1：

- [ ] 参数化 `table_proxy`：台呢平面、四条直库、六个 mouth 占位和 alignment overlay。
- [ ] 一套共享 `shared_ball_sphere`，使用准确球直径和球心原点。
- [ ] `cue_proxy`：只验证 tip 原点、`+Z` 朝向、母球击点和瞄准轴。
- [ ] `rect_lamp_proxy`：使用 manifest 的 1.85×0.65 m 尺寸、位置、色温、光通量和照度目标。
- [ ] `room_shell`：只建立墙、地、天花和 2–3 EV 背景层级，不制作装饰高模。
- [ ] 球、台呢、木材、金属、皮革、球杆、灯具和背景的 shader 样片。
- [ ] 四类镜头参数和六个截图场景的 ROI overlay。

Graybox 可以使用程序几何或原创临时 glTF。不得复制、描摹、转换或提交竞品模型、纹理、截图、Logo、UI 或场馆布局。

## 2. 仍被物理签字阻断的内容

以下内容只有在[球台可视 / 物理对齐签字契约](table-visual-physics-alignment-contract.md)第 10 节关闭后才能冻结：

- [ ] 生产球台高模和最终 LOD。
- [ ] 角袋/中袋最终 jaw、throat、leather lip 和 pocket inner 轮廓。
- [ ] 依赖最终鼻高的库边截面和倒角。
- [ ] 绑定最终 play-surface 高度的台体、桌腿和灯具吊装高度。
- [ ] production `SOCKET_pocket_*_throat/capture/jaw_*`。
- [ ] 以 production manifest 打包的 `NO_VISUAL_SOLID` 相交门。

签字前允许制作参数化候选灰模，但必须标记 `experimental`，且不得进入 production manifest。

## 3. Lookdev 数据门

- [ ] 运行 manifest 校验器并通过。
- [ ] 所有 base color factor 标注为 `linear_srgb_d65`，纹理标注为 sRGB。
- [ ] normal、ORM 和 mask 标注为 Linear；ORM 为 `R=AO,G=Roughness,B=Metallic`。
- [ ] 所有 roughness、metallic、normal scale 和 clearcoat 使用 `minimum/target/maximum`。
- [ ] 球和清漆木优先使用 `KHR_materials_clearcoat`；fallback 保持能量守恒、高光连续和相同粗糙度顺序。
- [ ] 灯光长度、色温、强度和照度分别使用 m、K、lm、lux；曝光使用 EV100。
- [ ] 任何参数调整必须修改并评审 manifest，不能只保存在个人 DCC 或 shader 默认值中。

验证命令：

```sh
python3 scripts/validate_lookdev_manifest.py \
  assets/modern/lookdev/focused_practice_hall_v1.lookdev.json
python3 -m unittest tests.visual.test_lookdev_manifest
```

## 4. 材质与灯光验收

- [ ] 球体 roughness/clearcoat 在 manifest 范围内，矩形台灯高光随视角连续，无固定贴图高光。
- [ ] 台呢在俯视无摩尔纹，在低位有弱方向性；不含烘焙阴影和摄影高光。
- [ ] 木材、金属和皮革在相同灯光下靠反射差异可辨，不依赖过度色差。
- [ ] 球杆能分出 tip、shaft、butt；graybox tip 与母球接触点共线。
- [ ] 台灯中心照度目标 700 lux，corner/center 目标 0.85，允许范围 0.80–1.00。
- [ ] 球台平均曝光比背景高 2–3 EV；背景不是全黑，灯具不是纯白块。
- [ ] 参考曝光为 8.1 EV100、白平衡 4550 K；单场景不得私自改曝光掩盖材质问题。

## 5. 镜头与构图验收

- [ ] `player_shot`：母球直径至少 54 px，目标球至少 20 px，瞄准轴偏屏幕中心不超过 5%。
- [ ] `low_aim`：母球至少 70 px，目标球至少 24 px，目标袋对比度至少 2.4。
- [ ] `tactical_overhead`：完整台面和六袋可见，边缘安全区 8–12%，球至少 18 px。
- [ ] `broadcast_oblique`：球台覆盖画幅 65–82%，关键球至少 13 px，至少四袋可见。
- [ ] 镜头切换不改变曝光、材质或 pocket 开放宽度。
- [ ] Gameplay 镜头关闭景深和 bloom；M1 不使用镜头污渍、色差或强暗角。

## 6. LDR 截图 ROI 门

所有截图为 1920×1080、RGB8、tone-mapped sRGB，ROI 原点在左上角。指标定义和逐 ROI 数值以 manifest 为准。

- [ ] `break`：table、rack 和 background ROI 全部通过。
- [ ] `low_aim`：cue_ball、aim_corridor 和 target_pocket ROI 全部通过。
- [ ] `ball_closeup`：hero_ball 和 contact_shadow ROI 全部通过。
- [ ] `pocket_closeup`：pocket_mouth 和 nose_and_leather ROI 全部通过。
- [ ] `tactical_overhead`：full_playfield 和 ball_field ROI 全部通过。
- [ ] `post_shot_observe`：action_corridor 和 environment ROI 全部通过。
- [ ] 全帧高光截断不超过 0.5%，暗部截断不超过 18%。
- [ ] Golden 更新的 SSIM 人工复核下限为 0.93；超过差异必须有批准记录，不能自动覆盖。

ROI 数值门是快速失败条件，不替代技术美术评审。满足均值和非黑像素不代表材质、构图、穿模和运动稳定性已经合格。

## 7. 性能与交付门

- [ ] 1080p Medium 的 M1 竖切仍满足项目 P95 16.7 ms 总帧预算。
- [ ] 核心球台与球 draw calls 不超过 45；完整 graybox 场景不超过 100。
- [ ] Graybox 资源命名、单位、坐标、切线、纹理通道符合 glTF 规范。
- [ ] 资产失败显示具体文件/字段；不得以静默白材质通过截图。
- [ ] Legacy asset 未被修改，Modern manifest 不引用旧 OBJ/BMP。
- [ ] Headless simulation 输出不因 lookdev 内容改变。

## 8. M1 美术签字

| 角色 | 结论 | 例外 / 后续项 | 日期 |
| --- | --- | --- | --- |
| 技术美术 | 待填写 | 待填写 | 待填写 |
| 图形负责人 | 待填写 | 待填写 | 待填写 |
| 物理负责人 | Graybox 可继续 / 生产高模待契约 | 待填写 | 待填写 |
| 产品 / 项目负责人 | 待填写 | 待填写 | 待填写 |

M1 美术通过意味着参数、graybox、灯光、镜头和截图门可用于现代渲染竖切；不等于生产球台高模或袋口物理已经批准。

