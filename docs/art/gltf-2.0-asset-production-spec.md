# glTF 2.0 资产生产规范

版本：1.0-draft
日期：2026-07-27
适用范围：ModernRenderer 的球台、球、球杆、灯具和场馆资产

## 1. 交付格式与目录

- 权威交换格式为 glTF 2.0。
- 开发期首选 `.gltf + .bin + PNG`，便于 diff manifest 和定位纹理；候选发布包可转 `.glb`。
- Phase 2 不要求 KTX2/Draco/Meshopt。引入扩展前必须有加载器支持、回退和性能数据。
- DCC 源文件不作为运行时输入，但应归档到有版本和授权控制的美术源目录。
- 每个资产包包含模型、纹理、manifest、预览图、授权元数据和导出器版本。

建议逻辑布局：

```text
assets/modern/<asset_id>/
  <asset_id>.gltf
  <asset_id>.bin
  textures/
  <asset_id>.asset.json
  preview/
```

## 2. 单位、坐标与原点

### 2.1 单位

- DCC 场景与 glTF 均按米制作；`1 glTF unit = 1 metre`。
- 导入 presentation world 时统一应用一次 `metres_to_centimetres = 100.0`，适配当前 simulation 的厘米坐标。
- 资产节点必须使用 `scale = [1,1,1]`；禁止靠未应用 scale 修正尺寸。
- manifest 记录关键实测尺寸，离线检查允许误差：球直径 ±0.05 mm；球台物理对齐边界 ±0.5 mm；装饰资产 ±5 mm。

### 2.2 坐标

- 右手系，`+Y` 向上，`+X` 为球台短边向右，`+Z` 为球台长边向前。
- 球台 root 原点位于地面中心；可打台面高度由批准的视觉/物理映射定义，不沿用旧模型的 86.483976 cm 偏差。
- 球 root 原点位于球心。
- 球杆 root 原点位于皮头接触面中心，杆身沿本地 `+Z` 延伸，`+Y` 为上方。
- 房间 root 原点与球台地面中心一致；灯具单独保留可定位 root。
- 所有旋转在导出前应用；静态资产 root rotation 为单位四元数。

### 2.3 物理边界

- 渲染网格不作为权威碰撞体，simulation core 也不得解析 glTF。
- 球台资产提供只读的 alignment markers：台面中心、六个袋口中心、库边鼻口基准和台面高度；导入测试与 `table_specs` 对比。
- 标记节点命名为 `SOCKET_*`，运行时默认不渲染；不得用标记节点改变物理常量。

## 3. 命名规范

只使用 ASCII 小写 asset ID；节点和材质采用大写类型前缀，禁止空格、中文、DCC 默认名称、冒号和自动数字后缀。

| 类型 | 格式 | 示例 |
| --- | --- | --- |
| 资产 ID | `bg_<category>_<name>_<variant>` | `bg_table_chinese8_training` |
| 网格 | `MESH_<asset>_<part>_LOD<n>` | `MESH_table_rail_long_LOD0` |
| 节点 | `NODE_<asset>_<part>` | `NODE_table_pocket_corner_ne` |
| 材质 | `MAT_<asset>_<surface>` | `MAT_table_felt_teal` |
| 纹理 | `T_<asset>_<surface>_<map>_<size>` | `T_table_wood_orm_2k.png` |
| 插槽/标记 | `SOCKET_<purpose>_<id>` | `SOCKET_pocket_corner_ne` |
| 碰撞预览 | `DEBUG_COLLIDER_<part>` | 只用于离线检查，不进发布包 |

方向后缀使用 `n/s/e/w/ne/nw/se/sw`；左右件从玩家站在球台脚点、看向 `+Z` 的方向定义。

## 4. 网格要求

- 发布网格必须为三角形；无非流形边、零面积面、重复面、悬空孤点和反向法线。
- 所有可见硬边使用真实小倒角或合理加权法线；不靠 normal map 伪造主要轮廓。
- 法线必须导出；使用 normal map 的网格必须导出符合 MikkTSpace 的切线和 handedness。
- 镜像负 scale 必须在 DCC 中应用；运行时禁止负 determinant transform。
- 同材质、同 LOD、同剔除需求的静态零件尽量合批；会单独隐藏或定位的功能件保持独立节点。
- 球使用一套共享网格和共享几何缓存，不为 16 个身份复制球面顶点。
- 球号/条纹使用稳定 UV 图集或贴花方案。若使用贴花，必须有避免 z-fighting 和 mip bleeding 的验证。
- 袋口、库边鼻口和球杆皮头的轮廓优先于看不见的台体底部细节。

## 5. UV 与 texel density

- `TEXCOORD_0`：所有平铺/独占 PBR 纹理；不得越出允许的 UDIM 之外，Phase 2 不使用 UDIM。
- `TEXCOORD_1`：可选 AO/lightmap，必须在 0–1 内且无重叠；未使用时不强制生成。
- 普通硬表面目标 texel density：约 `512 px/m`；英雄近景（球杆、袋口、木边）可到 `1024 px/m`；远景 `128–256 px/m`。
- 台呢以可平铺纹理实现，不为整张 2.54 m 台面分配唯一 4K。
- UV island 方向服从木纹/拉丝/台呢绒向；4K 图集 padding 不少于 16 px，2K 不少于 8 px，1K 不少于 4 px，并预留 mip 链。
- 球纹理极区和接缝不得穿过主要编号圆；编号周围至少保留两级 mip 的安全扩边。

## 6. PBR 材质与纹理通道

采用 glTF metallic-roughness 工作流：

| 纹理 | 通道 | 色彩空间 | 规则 |
| --- | --- | --- | --- |
| Base Color | RGB + 可选 A | sRGB | 不含烘焙高光、强阴影或 AO |
| Normal | RGB | Linear | OpenGL / glTF `+Y`，默认强度 1，可在材质中降尺度 |
| ORM | R=AO, G=Roughness, B=Metallic | Linear | 未用 AO 时 R=1；不得交换 R/G/B |
| Emissive | RGB | sRGB | 仅灯具发光面和明确自发光物；不代替照明 |
| Clearcoat | 按加载器批准 | Linear | 优先 `KHR_materials_clearcoat`；不支持时使用记录明确的 shader 参数近似 |

- 首阶段使用 8-bit PNG；仅对无 alpha 的照片型远景 base color 可评估 JPEG。
- normal、ORM、mask 禁止 JPEG。
- base color alpha 只在确有裁切/透明需求时使用；球台核心资产默认 OPAQUE。
- glTF sampler 必须有 mipmap；PBR 纹理默认 repeat，编号图集默认 clamp 或有充分 gutter。
- 材质不得依赖 DCC 私有节点。所有非标准参数写入 asset manifest 并由加载器验证。

### 6.1 材质槽预算

| 资产 | 最大材质槽 | 建议槽位 |
| --- | ---: | --- |
| 球台核心 | 8 | felt, cushion, wood, metal, leather, pocket_inner, body, optional_marker |
| 球 | 2 | shared resin + identity atlas/decal；优先 1 draw/material family |
| 球杆 | 5 | tip, ferrule, shaft, butt/wrap, joint |
| 台灯 | 3 | metal, diffuser, emissive |
| 场馆模块 | 每模块 3 | wall/floor/trim 或对应集合 |

槽位是上限，不是目标。相同 shader 参数和纹理集应复用材质实例。

## 7. LOD 规范与三角形预算

LOD 以屏幕覆盖率和镜头重要性选择，不以固定世界距离写死。切换必须有 hysteresis，避免球滚动时抖动。

| 资产 | LOD0 | LOD1 | LOD2 | 备注 |
| --- | ---: | ---: | ---: | --- |
| 球台核心 | ≤80k | ≤40k | ≤15k | LOD0 保留袋角和鼻口轮廓 |
| 单个共享球网格 | 3k–5k | 1k–2k | 300–700 | 16 球实例化；轮廓误差优先 |
| 球杆 | ≤6k | ≤3k | ≤1k | 皮头和细圆柱避免明显棱角 |
| 台灯 | ≤12k | ≤6k | ≤2k | 发光面轮廓保持 |
| 场馆静态可见集 | ≤120k | ≤60k | ≤25k | 按模块/遮挡，不含核心球台 |

Medium 1080p 目标同时可见预算：

- 几何：典型镜头 ≤250k 三角形，最坏电视镜头 ≤350k。
- draw calls：核心球台与球 ≤45，完整场景 ≤100，UI 另计。
- 纹理驻留：核心资产 ≤192 MiB，完整场馆 Medium ≤256 MiB；统计包括 mip 链和 GPU 格式，不以磁盘大小代替。
- 单个材质首阶段最多绑定 base color、normal、ORM、emissive/clearcoat 必要纹理；禁止为细微变化无上限增加贴图。

预算必须与参考机器的 16.7 ms P95 总帧预算共同评审。未达预算时先降低不可见底部和远景复杂度，不牺牲球、袋口和瞄准轮廓。

## 8. 建议纹理尺寸

| 内容 | 建议尺寸 | 说明 |
| --- | --- | --- |
| 球身份图集 | 2K，必要时 4K | 16 球共享，验证常用镜头编号可读性后决定 |
| 台呢平铺 | 1K–2K | 微表面可与低频色差分频复用 |
| 木边图集 | 2K | 四边保持方向与随机偏移 |
| 袋口/金属图集 | 1K–2K | 英雄近景优先 |
| 球杆图集 | 2K | 细长 UV，避免大量空区 |
| 台灯 | 1K | 发光面不需要高频纹理 |
| 场馆 trim / tile | 1K–2K | 远景复用，不做唯一 4K 墙面 |

## 9. Manifest 与授权元数据

每个资产的 `.asset.json` 至少记录：

- `asset_id`, `schema_version`, `gltf_path`, `content_hash`。
- `author`, `source_type`, `license`, `redistribution_allowed`, `created_at`。
- `dcc`, `exporter`, `exporter_version`, `units`, `up_axis`, `forward_axis`。
- LOD 网格、三角形数、材质槽、纹理尺寸与估算驻留内存。
- 物理对齐标记及期望尺寸。
- 使用的 glTF extensions；`extensionsRequired` 必须全部位于加载器白名单。

任何字段缺失、授权未知或 hash 不匹配都应在 CI 中阻止资产晋升为 production manifest。

## 10. 离线验收门

资产在不启动 OpenGL 的检查器中通过：

1. glTF schema 和所有 URI 可解析，无绝对路径与目录逃逸。
2. 单位、root transform、坐标和关键尺寸正确。
3. 必需法线/切线/UV 存在，切线有限且归一化。
4. 三角形、节点、材质槽、纹理尺寸和估算内存不超预算。
5. ORM/normal 色彩空间与通道声明正确，纹理为 2 的幂或有明确例外。
6. LOD0/1/2 的包围盒与原点一致，不发生位置跳变。
7. 六袋 alignment marker 和库边基准与物理 spec 在容差内。
8. manifest、授权字段和 content hash 完整。

游戏内还必须通过灰模、PBR、运动、LOD、截图和性能评审，离线通过不等于视觉批准。

## 11. 导出前检查清单

- [ ] 冻结变换，root scale=1、rotation=identity。
- [ ] 真实尺度和原点通过测量。
- [ ] 命名无 DCC 默认名、冒号、空格或品牌名。
- [ ] 网格三角化，无非流形、零面积、反法线。
- [ ] UV 密度、方向、padding 和编号接缝通过。
- [ ] MikkTSpace 切线与 normal map `+Y` 一致。
- [ ] PBR 通道、色彩空间、alpha mode 和 sampler 正确。
- [ ] LOD 包围盒、材质和原点一致。
- [ ] manifest、授权、预览和导出版本完整。
- [ ] 游戏内六场景与 1080p Medium 性能门通过。

