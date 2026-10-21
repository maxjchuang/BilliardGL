# 球台可视 / 物理对齐签字契约

版本：1.0-draft
日期：2026-07-27
状态：M1/M2 兼容绑定已批准；解析袋口与最终生产高模仍待独立物理晋升

项目决定（2026-07-27）：M1/M2 保留当前 interactive profile，不修改 physics；冻结 `surface_y=86.483976 cm`、`legacy_opening_band`、`nose=1.4R`、名义 mouth 端点、`0.05 cm` marker 容差和 `0.10 cm` 净空 QA 容差。绑定文件为 `assets/modern/graybox/focused_practice_hall_m1_alignment_v1.json`。捕获后视觉脱钩在 M1 不启用；只有 snapshot 明确提供 `pocket_id + capture_sequence` 一次性事件后才允许另行设计。

## 1. 目的与边界

本契约定义 ModernRenderer 球台、球、库边鼻口和袋口资产如何读取现有物理几何。它不修改物理 profile，也不把渲染网格升级为碰撞真值。

权威顺序：

1. 当前可执行程序实际选择的 `PhysicsProfile` 与 `table_specs` 常量。
2. 通过 promotion gate 的生产 profile。
3. 经过签字、用于资产检查的只读 alignment manifest。
4. glTF `SOCKET_*` marker 和可见网格。

后一级只能验证和表现前一级，不能覆盖前一级。DCC、glTF、截图或“看起来更真实”都不能改变球的位置、碰撞边界、袋口状态机或 production profile。

## 2. 坐标与术语

- 现有世界使用厘米、右手语义、`+Y` 向上、`+X` 沿球台短边、`+Z` 沿球台长边。
- glTF 使用米；导入 presentation world 时统一乘 `100.0`。
- 球台中心为 `(0, surface_y, 0)`；可打区域名义边界为 `X=±63.5 cm`、`Z=±127.0 cm`。
- “库边鼻口平面”指球体首先接触直库的可见/物理边界；球心直库极限比鼻口平面向台内缩一个球半径。
- 袋口局部坐标沿用 `PocketBoundaryFrame`：`inward` 指向台内，`tangent` 沿袋口横向；局部正 `depth` 指向台外，即世界方向 `-inward`。
- glTF 袋口 socket 使用右手基：本地 `+X=tangent`、`+Y=world up`、`+Z=-inward`；`+Z` 即台外和正 depth 方向。此前把 `+Z` 写成 inward 会形成左手基，不得使用。

## 3. 当前数据分层

### 3.1 已是代码权威的设备常量

下表由 `table_specs.h/.cpp` 定义并有 `table_specs_tests.cpp` 覆盖。除非另开物理/设备规格变更，这些值可直接用于 ART-201 灰模。

| 项目 | 权威值 | 源 | 资产含义 |
| --- | ---: | --- | --- |
| 球直径 | `5.715 cm` | `kChineseBallDiameterCm` | 可见球网格外径 |
| 球半径 | `2.8575 cm` | `kChineseBallRadiusCm` | 球心边界偏置与接触阴影基准 |
| 可打区域宽 | `127.0 cm` | `kChinesePlayfieldWidthCm` | 两条长库名义鼻口间距 |
| 可打区域长 | `254.0 cm` | `kChinesePlayfieldLengthCm` | 两条短库名义鼻口间距 |
| 设备目标台高 | `85.0 cm` | `kChineseTableHeightCm` | 新产品目标，尚非当前球状态高度 |
| 角袋名义口宽 | `13.2 cm` | `kChineseCornerPocketMouthWidthCm` | 角袋 mouth plane 横向全宽 |
| 中袋名义口宽 | `8.6 cm` | `kChineseSidePocketMouthWidthCm` | 中袋 mouth plane 横向全宽 |
| legacy drop-zone 深度 | `6.0 cm` | `kChinesePocketDropZoneDepthCm` | 兼容参数，不等于已批准的真实袋内造型 |

直库上的球心合法极限由物理实现按 `half_extent - ball_radius` 求得：

- `X=±60.6425 cm`。
- `Z=±124.1425 cm`。

可见鼻口必须仍位于名义边界 `X=±63.5` / `Z=±127.0`，不得误把球心极限当成网格位置。

### 3.2 当前可执行程序的兼容高度

当前 `game_state.h` 将 `kTableHeight` 绑定到 `kTableModelPlaySurfaceHeightCm = 86.483976 cm`，初始球心高度为：

```text
86.483976 + 2.8575 = 89.341476 cm
```

这与设备目标台高 `85.0 cm` 相差 `1.483976 cm`。该值是为了旧 `table.obj` 对齐而保留的兼容高度，不应被重新命名成真实设备高度。

Phase 1/2 在未获签字前必须遵守：

- 游戏内灰模的 `SOCKET_play_surface_origin.y` 使用 `86.483976 cm`，保证当前球不漂浮或嵌入。
- 美术 DCC 可以保留 85 cm 的参数化目标版本，但 production manifest 不得自行切换到 85 cm。
- 从 86.483976 cm 迁移到 85 cm 会改变球的 `position.y`、截图和确定性状态；必须由物理/运行时负责人另行批准和验证，不能只在 renderer 中加隐藏偏移。

### 3.3 当前交互玩法使用的库边参数

桌面与 rendered automation 选择 `interactiveChinesePoolPhysicsProfile()`。该 profile 当前包含：

- 球半径 `2.8575 cm`。
- 库边鼻高比 `1.4R`，即鼻口接触高度相对台呢为 `4.0005 cm`。
- `TableBoundaryProperties` 仍继承 `legacy_opening_band`，并未采用 Phase 3 的解析袋口候选。

`1.4R` 可用于当前互动版本的直库可见鼻口垂直基准，但它不是台高本身，也不授权采用未晋升的袋口曲线。

### 3.4 尚未晋升的解析袋口候选

`chinese_pool_pocket_boundary_v1` / Phase 3 v5 中出现以下值：

| 项目 | 候选值 | 证据状态 |
| --- | ---: | --- |
| 角袋喉宽 | `11.0 cm` | Grade-C 解析假设，缺少直接测量 |
| 中袋喉宽 | `7.0 cm` | Grade-C 解析假设，缺少直接测量 |
| 库鼻半径 | `1.2 cm` | Grade-C 解析假设，缺少直接测量 |
| 喉部深度 | `4.0 cm` | Grade-C 事件边界，缺少直接测量 |
| 捕获深度 | `6.0 cm` | Grade-C 不可逆捕获平面，缺少直接测量 |

这些参数通过了解析几何和确定性扫描，但真实轨迹与进球成功率实验仍缺失；Phase 3 v5 整体已被拒绝，`phase3_production_default.json` 明确没有晋升 Phase 3 candidate。

因此：

- 可以在 debug overlay、参数化灰模或独立实验 manifest 中预览。
- 不得写入 production 资产 manifest，不得以这些值制作不可逆高模，也不得让 ModernRenderer 静默选择它们。
- 物理负责人必须先决定生产边界继续使用 `legacy_opening_band`、单独晋升 pocket v1，还是冻结新的 pocket v2。

## 4. 六袋权威索引与 mouth frame

`buildPocketBoundaryFrames()` 以稳定 ID 生成四角袋和两中袋。以下 mouth center、类型和局部轴由代码确定；`Y` 一律为获批准的 play-surface 高度。

| ID / 名称 | 类型 | mouth center `(X,Z) cm` | `inward (X,Z)` | `tangent (X,Z)` | 名义 mouth width |
| --- | --- | --- | --- | --- | ---: |
| 0 / `corner_nx_nz` | 角袋 | `(-63.5,-127.0)` | `(+0.707107,+0.707107)` | `(-0.707107,+0.707107)` | `13.2 cm` |
| 1 / `corner_px_nz` | 角袋 | `(+63.5,-127.0)` | `(-0.707107,+0.707107)` | `(-0.707107,-0.707107)` | `13.2 cm` |
| 2 / `corner_nx_pz` | 角袋 | `(-63.5,+127.0)` | `(+0.707107,-0.707107)` | `(+0.707107,+0.707107)` | `13.2 cm` |
| 3 / `corner_px_pz` | 角袋 | `(+63.5,+127.0)` | `(-0.707107,-0.707107)` | `(+0.707107,-0.707107)` | `13.2 cm` |
| 4 / `side_nx` | 中袋 | `(-63.5,0.0)` | `(+1,0)` | `(0,-1)` | `8.6 cm` |
| 5 / `side_px` | 中袋 | `(+63.5,0.0)` | `(-1,0)` | `(0,+1)` | `8.6 cm` |

角袋 `inward` 是 45° 对角方向；视觉资产不得把角袋 mouth frame 当成只沿 X 或只沿 Z 的圆洞。

## 5. 口宽、通道与禁止遮挡区

### 5.1 名义开口与球心通道

在 mouth plane 上，解析分类器以 `mouth_width - 2R` 计算球心可通过宽度。使用当前球半径时：

| 袋型 | 名义口宽 | mouth plane 球心通道全宽 | 半宽 |
| --- | ---: | ---: | ---: |
| 角袋 | `13.2 cm` | `7.485 cm` | `3.7425 cm` |
| 中袋 | `8.6 cm` | `2.885 cm` | `1.4425 cm` |

这些通道数值用于理解球心空间，不能用来缩小可见袋口。可见 mouth 的两端仍由名义口宽定义；球体外壳需要围绕球心通道保持一个半径的净空。

### 5.2 禁止遮挡体积

对每个已批准 physics profile，资产检查器应从 `PocketBoundaryFrame` 和球半径生成球心可达走廊，再用半径 `R` 对走廊做膨胀。该膨胀体积是 `NO_VISUAL_SOLID`：

- 库边橡胶、木材、金属、皮革和袋内壁不得进入。
- alpha-tested 装饰、不可见碰撞代理和 LOD 简化网格同样不得进入。
- 可见阴影可以落入，但 shadow caster 本体必须位于实体几何处。
- 捕获后表现层的下落球可以离开 2.5D 平面；捕获前不得通过视觉动画绕开实体。

在未有自动膨胀检查器前，灰模使用额外 `0.10 cm` 安全余量，即可见实体离 `NO_VISUAL_SOLID` 至少 1 mm。该余量是美术 QA 门，不是物理参数。

### 5.3 直库禁止侵入区

除袋口开放段外，可见库边鼻口不得向台内越过名义平面：

- 左/右长库：`X=-63.5 / +63.5 cm`。
- 近/远短库：`Z=-127.0 / +127.0 cm`。

允许倒角和橡胶实体向台外、向上或向下扩展；任何 LOD 都不得向台内侵入超过提议的 `0.05 cm` 对齐容差。不得把 solver 的 penetration slop 或 maximum penetration 当成美术穿模预算。

## 6. 允许的视觉扩展

以下内容可以超出 2.5D 物理边界，但必须位于 `NO_VISUAL_SOLID` 外：

- mouth plane 之后的皮革包边、袋篮、袋内壁和回球结构。
- 台面以下的球下落、轻微反弹、袋布形变和音效触发。
- 库边向台外的木饰面、金属角件、台体和装饰倒角。
- 不改变轮廓的 normal/roughness 微细节。
- 捕获事件之后的表现性下落轨迹；必须由一次性 `Captured` 事件触发，并与 simulation ball 脱钩。

禁止：

- 在 mouth 或 throat 可达区上方增加看似实体的皮革舌片、金属压条或木质尖角。
- 用黑色平面封住入口或把袋口视觉做得比当前 profile 更窄。
- 为了避免穿模而移动、缩小球，或在 renderer 中改变球状态。
- 用未晋升 candidate 的 jaw/throat 网格掩盖 active legacy physics 的差异。
- 让 LOD、质量档或摄像机改变袋口可见开放宽度。

## 7. `SOCKET_*` marker 契约

### 7.1 必需 marker

| 名称 | 位置 / 朝向 | 用途 |
| --- | --- | --- |
| `SOCKET_play_surface_origin` | `(0, surface_y, 0)`；世界轴对齐 | 台呢平面与球心高度检查 |
| `SOCKET_rail_nose_x_neg` | `(-63.5, surface_y+nose_h, 0)`；`+Z` 指向台内 | 左长库鼻口基准 |
| `SOCKET_rail_nose_x_pos` | `(+63.5, surface_y+nose_h, 0)`；`+Z` 指向台内 | 右长库鼻口基准 |
| `SOCKET_rail_nose_z_neg` | `(0, surface_y+nose_h, -127)`；`+Z` 指向台内 | 近短库鼻口基准 |
| `SOCKET_rail_nose_z_pos` | `(0, surface_y+nose_h, +127)`；`+Z` 指向台内 | 远短库鼻口基准 |
| `SOCKET_pocket_<id>_mouth` | 第 4 节 mouth center；本地 `+X=tangent,+Y=up,+Z=outward/depth` | 袋口 frame 与口宽检查 |
| `SOCKET_pocket_<id>_mouth_neg_t` | mouth center `- tangent × mouth_width/2` | 名义口宽负端点 |
| `SOCKET_pocket_<id>_mouth_pos_t` | mouth center `+ tangent × mouth_width/2` | 名义口宽正端点 |

其中 `nose_h = radius × nose_height_ratio`。当前互动路径为 `4.0005 cm`；若 profile 改变，marker 期望值必须来自 alignment manifest，而不是 DCC 常量。

### 7.2 profile 条件 marker

只有选定的 production geometry 明确定义解析 jaw/throat/capture 时，以下 marker 才能进入 production manifest：

| 名称 | 期望公式 |
| --- | --- |
| `SOCKET_pocket_<id>_throat` | `mouth_center - inward × throat_depth` |
| `SOCKET_pocket_<id>_capture` | `mouth_center - inward × capture_depth` |
| `SOCKET_pocket_<id>_jaw_neg_t` | `mouth_center - tangent × (mouth_width/2 + jaw_radius)` |
| `SOCKET_pocket_<id>_jaw_pos_t` | `mouth_center + tangent × (mouth_width/2 + jaw_radius)` |

在 `legacy_opening_band` 下，`jaw_radius=0.0001 cm`、mouth/throat 等宽且 legacy capture 使用兼容状态路径，这些值不构成可信的可见袋口造型。生产灰模只要求 mouth marker；条件 marker 应标记 `experimental` 或省略。

### 7.3 candidate v1 仅供 debug 的参考位置

若用独立实验 manifest 预览 `throat_depth=4 cm`、`capture_depth=6 cm`，中心位置如下。它们不得出现在 production manifest：

| ID | throat `(X,Z) cm` | capture `(X,Z) cm` |
| --- | --- | --- |
| 0 | `(-66.3284,-129.8284)` | `(-67.7426,-131.2426)` |
| 1 | `(+66.3284,-129.8284)` | `(+67.7426,-131.2426)` |
| 2 | `(-66.3284,+129.8284)` | `(-67.7426,+131.2426)` |
| 3 | `(+66.3284,+129.8284)` | `(+67.7426,+131.2426)` |
| 4 | `(-67.5,0)` | `(-69.5,0)` |
| 5 | `(+67.5,0)` | `(+69.5,0)` |

## 8. 提议的资产对齐容差

下列值是 Phase 2 资产验收容差，待物理负责人签字；它们不修改 solver tolerance。

| 检查 | 提议容差 | 失败处理 |
| --- | ---: | --- |
| 球网格直径 | `±0.005 cm` | 阻止资产导入 |
| play surface marker 高度 | `±0.05 cm` | 阻止球台导入 |
| 直库鼻口平面 | `±0.05 cm`，且不得向台内超差 | 阻止对应 LOD 导入 |
| pocket mouth center | 每轴 `±0.05 cm` | 阻止球台导入 |
| mouth 两端点 / 全宽 | `±0.05 cm` | 阻止球台导入 |
| throat/capture/jaw 条件 marker | 每轴 `±0.05 cm` | 阻止该 profile 资产晋升 |
| LOD 间 marker 变化 | `0 cm` | 阻止 LOD 打包 |
| `NO_VISUAL_SOLID` 安全余量 | 至少 `0.10 cm` | 修网格，不放宽物理 |
| 台呢与鼻口可见接缝 | 缝隙/重叠不超过 `0.05 cm` | 修网格或法线 |

如果 1 mm 的 no-occlusion 安全余量导致袋口造型不可接受，应回到物理/profile 评审，不得由美术单方面缩小球或移动 socket。

## 9. 自动化验收建议

离线资产检查器应读取 glTF marker 和运行时导出的 alignment manifest，并执行：

1. 球直径、surface、四条直库和六个 mouth frame 的数值比较。
2. 所有 LOD marker transform 完全一致。
3. 以当前 profile 和球半径采样 pocket passable corridor，对可见三角形做 `NO_VISUAL_SOLID` 相交检查。
4. 生成俯视 debug 图：鼻口平面、球心极限、mouth、jaw、throat、capture 和 mesh silhouette 分色叠加。
5. 在六个 rendered 场景中保留袋口特写和球贴库边截图。

首版如果尚无三角形体积相交器，至少要实现 marker 数值检查与固定俯视 overlay；人工截图不能替代最终离线检查。

## 10. 待物理负责人确认 / 签字项

以下问题不应由技术美术假设：

- [x] M1/M2 兼容资产使用 `86.483976 cm`；85.0 cm 迁移不在本阶段发生。
- [x] M1/M2 兼容 pocket geometry 使用 `legacy_opening_band` 和 mouth-only marker。
- [ ] 若晋升解析 profile，确认 corner/side throat width、jaw radius、throat depth、capture depth 和 `geometry_id`。
- [x] 当前互动路径 `nose_height_ratio=1.4` 作为 M1/M2 可见鼻口高度兼容值。
- [x] mouth width 按局部 `tangent` 上两个名义端点间距离定义，并与 socket 端点一致。
- [x] `0.05 cm` marker 容差和 `0.10 cm` no-occlusion 安全余量已批准。
- [x] M1 捕获表现继续由 simulation 驱动，不脱钩；未来只有 snapshot 提供 `pocket_id + capture_sequence` 一次性事件后才能另行启用。

签字记录：

| 角色 | 姓名 | 决定 / 例外 | 日期 |
| --- | --- | --- | --- |
| 物理负责人 | 项目兼容决策记录 | M1/M2 保留 interactive/legacy；解析袋口仍待晋升 | 2026-07-27 |
| 图形负责人 | 待填写 | 待填写 | 待填写 |
| 技术美术 | 待填写 | 待填写 | 待填写 |
| 产品 / 项目负责人 | 待填写 | 待填写 | 待填写 |

## 11. Phase 0 对齐关闭条件

- 第 10 节 M1/M2 兼容项已有明确决定；解析 profile 项保持条件阻断。
- 决定被写入版本化 alignment manifest，并绑定 physics profile ID / geometry ID。
- ART-201 灰模导入后通过 marker 数值检查和六袋 overlay。
- 未晋升 candidate 参数没有进入 production manifest。
- current/target 台高差异有显式迁移任务或明确的兼容保留决定。
