# BilliardGL 视觉与玩法优化分阶段实施计划

日期：2026-07-27  
状态：PROPOSED  
建议产品定位：高可信物理驱动的中国式台球模拟与训练工具

## 1. 目的

本计划将 BilliardGL 当前的视觉与玩法优化方向拆成可独立开发、验证、交付和回退的阶段。实施主线是保留已经具备确定性测试与证据治理的 simulation core，逐步替换 presentation layer，并将现有自动化、物理场景和遥测能力产品化为训练与回放功能。

计划不是一次性重写。每个阶段都必须满足以下条件：

- 主分支始终可构建、可启动、可进行一局基础游戏。
- 物理、规则、渲染和产品数据的边界清晰。
- 每个阶段有独立的功能开关、验收门槛和回滚路径。
- 新视觉不能以修改冻结物理 profile 或破坏确定性为代价。
- 未通过验证的物理能力不得因为 UI 已完成而对普通玩家开放。

## 2. 计划假设

排期以 2 名 C++/图形开发、1 名技术美术或外包美术、兼职 UI/UX 支持为基准。只有 1 名开发时，不应简单压缩时间，而应保持阶段顺序并将周期按约 1.8--2.2 倍估算。

初步发布目标：

- 桌面端 1920x1080、60 FPS。
- 以 macOS 当前开发环境为首要持续集成平台，同时不主动破坏 Windows 工程。
- 第一条完整体验只覆盖一张高质量中国式台球桌和一个中国式八球规则集。
- 在线匹配不属于第一轮视觉与训练版本的关键路径。

## 3. 不可破坏的工程边界

### 3.1 Simulation Core

以下模块继续作为权威模拟层，不允许依赖 OpenGL、UI 或资源对象：

- `game_runtime`
- `game_state`
- `physics` 及各接触求解模块
- `rules`
- `physics_profile`
- `physics_scenario`
- `physics_telemetry`

渲染只能读取已完成 tick 的状态或插值后的 presentation snapshot，不得反向修改球的位置、速度、旋转或规则结果。

### 3.2 物理证据边界

- 生产默认 profile 继续服从 `physics_models/promotion/phase3_production_default.json`。
- 视觉版本不得静默更换生产 profile。
- elevated cue、jump、masse、完整 miscue 边界等仍处于未验证状态；相应 UI 在能力未晋升前必须隐藏或明确标记为实验功能。
- 自动上塞等现有交互近似必须在 UI 中与玩家主动选择的击点参数区分。

### 3.3 自动化协议

- 已有 automation v1 命令语义保持兼容。
- 新命令通过 capability 声明，不能根据客户端版本猜测支持情况。
- 训练、回放、视觉质量和新击球控制使用版本化 schema。
- headless 模式不得初始化任何图形或 UI 依赖。

## 4. 目标架构

```text
platform / GLUT callbacks
  -> semantic input actions
  -> GameRuntime (authoritative fixed-step simulation)
  -> PresentationSnapshotBuilder
       -> camera system
       -> render scene
       -> HUD view model
       -> audio events

Renderer facade
  -> LegacyRenderer       (仅迁移窗口存在)
  -> ModernRenderer
       -> resource manager
       -> render passes
       -> UI renderer

Training / Replay
  -> versioned scenario + metadata
  -> action log + profile identity + seed
  -> isolated preview runtime
```

推荐逐步新增以下边界，文件名可以在实施时小幅调整：

```text
src/Billiards/render/
  renderer_backend.h
  legacy_renderer.cpp
  modern_renderer.cpp
  presentation_snapshot.h/.cpp
  shader_program.h/.cpp
  mesh.h/.cpp
  texture.h/.cpp
  material.h/.cpp
  framebuffer.h/.cpp
  render_metrics.h/.cpp
  passes/

src/Billiards/ui/
  hud_view_model.h/.cpp
  shot_controls.h/.cpp
  match_hud.h/.cpp
  training_hud.h/.cpp

src/Billiards/camera/
  camera_controller.h/.cpp
  camera_presets.h/.cpp
  camera_transition.h/.cpp

src/Billiards/training/
  training_scenario.h/.cpp
  training_session.h/.cpp
  trajectory_preview.h/.cpp

src/Billiards/replay/
  replay_format.h/.cpp
  replay_recorder.h/.cpp
  replay_player.h/.cpp
```

不要一次创建全部目录和空类型。每个类型应在所属阶段出现第一个真实消费者时再加入。

## 5. 总体里程碑

| 里程碑 | 用户可见结果 | 累计建议周期 |
| --- | --- | ---: |
| M0 基线冻结 | 目标画面、性能预算和迁移约束可重复验证 | 2 周 |
| M1 现代渲染垂直切片 | 一张桌、球、球杆、台灯以新管线完成可玩闭环 | 6--8 周 |
| M2 视觉与击球体验 Alpha | 新资产、镜头、HUD、击点与力度交互完整 | 12--16 周 |
| M3 训练版本 Alpha | 摆球、练习、轨迹、慢放和回放可用 | 18--24 周 |
| M4 单机产品 Beta | 完整中式八球规则、犯规反馈和基础 AI | 26--34 周 |
| M5 在线版本（可选） | 房间、匹配、观战和排名 | 38 周以后 |

上述周期允许同一阶段内的美术生产和工程工作并行，但阶段质量门不能并行跳过。

## 6. Phase 0：基线、技术验证与产品约束

建议周期：2 周。  
目标：在修改画面前建立可量化基线，关闭会导致后续返工的技术选型问题。

### 6.1 工作包 P0-A：基线场景

- [ ] 固定六个视觉场景：开球、低位瞄准、球体特写、袋口特写、战术俯视、击球后观察。
- [ ] 为场景固定窗口尺寸、摄像机参数、球状态、随机种子和物理 profile ID。
- [ ] 通过 rendered automation 生成截图，禁止依赖桌面截屏。
- [ ] 记录当前 CPU 帧时间、GPU 帧时间、draw calls、三角形数和显存估算。
- [ ] 记录当前 1080p 与当前默认 1024x768 的帧率和截图。

建议新增：

- `tests/visual/visual_scene_registry.*`
- `tests/e2e/test_visual_baselines.py`
- `scripts/capture_visual_baselines.py`
- `docs/visual-validation.md`

### 6.2 工作包 P0-B：受治理的视觉基准

现有仓库约定不提交普通本地截图。视觉回归需要区分两类文件：

- 临时开发截图：继续写入 `/tmp`，不得提交。
- 经审核的测试基准：放入 `tests/visual/baselines/`，带场景版本、生成 revision、分辨率、色彩空间和审批说明。

需要先更新仓库治理文档和 `.gitignore` 例外，再引入第一批基准。基准更新必须单独提交，禁止被普通代码提交顺带覆盖。

首版比较器至少提供：

- 图像尺寸和可见像素检查。
- HUD/球台/球体兴趣区域检查。
- 绝对像素差与结构相似度阈值。
- 失败时输出 actual、expected 和 diff 到构建产物目录。

不同 GPU/驱动的像素级结果可能不完全相同，因此 CI 硬门先使用区域和宽容差指标；严格 golden 比较放在固定参考机器或人工审批流程中。

### 6.3 工作包 P0-C：技术 Spike 与 ADR

在两周内完成小型垂直验证，并为以下选择写 Architecture Decision Record：

1. OpenGL 上下文：目标使用 OpenGL 3.3 功能集；macOS 请求 4.1 Core Profile。
2. Shader 版本与平台宏策略。
3. UI：推荐产品 UI 使用 FreeType/SDF 或 RmlUi 类方案；Dear ImGui 仅用于调试面板，不作为最终 HUD。
4. glTF 2.0 加载库与资源转换方式。
5. 纹理格式：开发期 PNG/JPG，后续再评估 KTX2；首阶段不引入纹理压缩复杂度。
6. 线性空间、sRGB framebuffer 和 tone mapping 的平台一致性。

Spike 必须至少在屏幕上绘制一个 shader 球体、一个纹理平面和一段新字体，并能在 screenshot 模式输出。

### 6.4 Phase 0 退出门

- 六个基线场景可一条命令重复生成。
- `./scripts/check.sh` 保持通过。
- 有明确的 1080p/60 FPS 初始预算和参考机器说明。
- Core Profile、UI 和 glTF 选型均有 ADR，不再留到大规模实现中争论。
- 尚未修改生产物理 profile 或现有玩法语义。

### 6.5 回滚点

本阶段只增加测试、文档和独立 spike。删除 spike 编译目标即可回到原状态。

## 7. Phase 1：现代渲染基础与垂直切片

建议周期：4--6 周。  
目标：建立可并行生产资产和效果的现代渲染骨架，在一条完整可玩路径中验证，而不是一次迁移所有场景。

### 7.1 工作包 P1-A：Presentation Snapshot

- [ ] 新增只读 `PresentationSnapshot`，从最近两个已完成的物理状态和插值 alpha 构建。
- [ ] 只包含渲染需要的数据：transform、可见性、ball identity、cue transform、camera、HUD view model 和瞬态视觉事件。
- [ ] 离散状态仍取 current physics tick；球位和视觉旋转遵守现有插值规范。
- [ ] Renderer API 不再接收可修改的 `GameState&`。
- [ ] 用架构测试禁止 render/UI 模块包含 physics 求解头文件或调用状态 mutation API。

### 7.2 工作包 P1-B：双后端迁移窗口

- [ ] 建立 `RendererBackend` 生命周期：initialize、resize、render、capture、shutdown。
- [ ] 现有实现包装为 `LegacyRenderer`，不在包装过程中做视觉改造。
- [ ] 新增 `ModernRenderer`。
- [ ] 增加 `--renderer legacy|modern`；开发期默认 legacy，M1 通过后切换 modern。
- [ ] rendered automation 的 ready capability 报告当前 backend。
- [ ] 两个 backend 使用同一 `PresentationSnapshot`。

双后端只用于迁移，不长期维护。Phase 3 完成且 modern 成为默认后，应安排独立清理任务删除 legacy。

### 7.3 工作包 P1-C：GPU 资源与 Shader

- [ ] RAII 管理 shader、buffer、vertex array、texture、sampler 和 framebuffer。
- [ ] shader 编译/链接错误必须包含文件、阶段和完整日志。
- [ ] 支持开发期 shader reload，但 Release 构建失败时必须明确终止或回退到错误材质。
- [ ] 新增 mesh attribute 规范：position、normal、tangent、UV、可选 vertex color。
- [ ] 建立 per-frame、per-view、per-object 和 per-material 数据边界。
- [ ] 避免在每帧创建 GLU quadric、shader 或纹理。

### 7.4 工作包 P1-D：第一条 Render Pass

首个垂直切片只迁移以下内容：

- 球台面与库边。
- 16 个球。
- 球杆。
- 一盏台灯。
- 简单背景。

第一版 pass：

```text
shadow depth
  -> opaque forward PBR
  -> transparent/guide overlays
  -> tone map
  -> UI
```

首阶段不做 deferred renderer、TAA、复杂 volumetric lighting 或通用 render graph。当前场景规模小，forward PBR 更容易验证和维护。

### 7.5 工作包 P1-E：色彩与基础质量

- [ ] sRGB 纹理解码和线性空间光照。
- [ ] HDR 中间缓冲与统一曝光。
- [ ] ACES 或经过对比选定的 tone mapping。
- [ ] 4x MSAA，提供关闭和 2x/4x 质量档。
- [ ] 避免球体高光在运动时严重闪烁。
- [ ] 增加 GL debug callback 和每帧 GL error 统计，仅 Debug 开启详细输出。

### 7.6 Phase 1 测试

- shader parser/资源失败单元测试。
- snapshot 不改变 authoritative state 的测试。
- legacy/modern 对同一场景读取完全相同世界 transform 的测试。
- resize、最小化恢复、截图、资源销毁重建测试。
- modern backend 的六场景 smoke screenshot。
- 60/120/144 Hz 下物理 tick 数继续满足现有规范。

### 7.7 Phase 1 退出门

- modern backend 可以从启动、瞄准、击球运行到所有球停止。
- 球、球台和球杆不缺失、不全黑、不使用错误 transform。
- 1080p 参考机器 P95 帧时间不超过 16.7 ms；若暂未达标，必须有按 pass 分解的 GPU 数据和已批准例外。
- headless 输出与引入 modern backend 前逐 tick 等价。
- 物理 release gate、完整测试和 rendered E2E 全部通过。

### 7.8 回滚点

运行参数切回 legacy。ModernRenderer 不允许改变资源路径和 simulation state，因此回滚不需要迁移存档或物理数据。

## 8. Phase 2：核心材质、灯光、资产与场馆

建议周期：4--6 周。  
目标：消除最明显的 Demo 感，完成“一张可信球台”的视觉竖切。

### 8.1 美术目标板

在制作资产前批准以下目标：

- 球体近景和远景的高光形状。
- 台呢颜色、纤维尺度和粗糙度。
- 木质库边与金属角件的反射范围。
- 袋口皮革、内壁和遮蔽深度。
- 台灯照度、环境曝光与背景对比。
- 球员视角、俯视和电视视角的构图。

目标板只用于质量标准，不直接复制竞品的品牌、商标、模型或纹理。

### 8.2 工作包 P2-A：PBR 材质

- 球：base color、低 roughness、clearcoat 近似、正常的台灯反射。
- 台呢：高 roughness、微表面 normal、方向性细节和合理纹理尺度。
- 木材：base color、normal、roughness variation，避免单一重复木纹。
- 金属：metallic workflow。
- 袋口：皮革与黑色内壁拆分，不能再以纯黑平面表现深度。
- 球杆：木材、漆面、金属接头和皮头分材质。

球号与条纹应使用可控 UV 或贴花方案，保证旋转时不游移、镜像或接缝明显。

### 8.3 工作包 P2-B：灯光和阴影

- [ ] 台灯使用矩形区域光近似或经过验证的多点光组合。
- [ ] 实现 shadow map，控制 bias、peter-panning 和球体 acne。
- [ ] 球下增加稳定的接触阴影或小范围屏幕空间遮蔽。
- [ ] 袋口和库边必须产生明确遮蔽。
- [ ] 加入环境反射；优先使用预过滤 cubemap/IBL，避免实时反射探针超出预算。
- [ ] 背景保持低曝光，球台和目标球是视觉焦点。

### 8.4 工作包 P2-C：glTF 资产管线

- [ ] 新资产统一使用 glTF 2.0。
- [ ] 定义单位为厘米或在导入时强制转换为厘米，并写入资产规范。
- [ ] 定义坐标轴、正面、切线、UV、命名和 material slot 约定。
- [ ] 导入器验证缺失法线、切线、纹理和非法 scale。
- [ ] OBJ/BMP 作为 legacy bridge 保留到所有生产资产迁移完成。
- [ ] 增加离线资产检查脚本，CI 不启动 OpenGL 即可验证 manifest。

### 8.5 工作包 P2-D：第一座场馆

第一版只建设一个比赛/训练兼用场馆：

- 球台附近保留足够操作空间。
- 背景模型采用分级细节，不把预算消耗在看不清的家具上。
- 场馆不依赖品牌授权。
- 观众、复杂动画和动态广告板不进入本阶段。

### 8.6 质量档

| 选项 | Low | Medium | High |
| --- | --- | --- | --- |
| Shadow | 1024、简化 | 2048 | 4096 或稳定级联方案 |
| MSAA | Off/2x | 4x | 4x |
| Contact/AO | Off | 基础 | 高质量 |
| Environment | 低分辨率 | 标准 | 高分辨率 |
| Post Process | 基础 tone map | tone map＋轻量 bloom | 同左，允许回放景深 |

所有档位必须保持规则和可见信息一致；Low 不能隐藏关键球、瞄准线或犯规提示。

### 8.7 Phase 2 退出门

- 六个目标场景通过技术美术与产品共同评审。
- 静止球具有稳定接触感；球不能像漂浮或嵌入台呢。
- 袋口、库边鼻口和球体比例与物理几何对齐。
- 球号在常用摄像机距离可辨认，旋转无明显 UV 错误。
- 1080p Medium 达到既定帧时间预算。
- 资产加载错误提供具体文件和字段，不出现静默白材质。

### 8.8 回滚点

材质、灯光和场馆通过 manifest 选择。保留一个 minimal test scene，以便在资产或 shader 出错时区分内容问题与渲染器问题。

## 9. Phase 3：摄像机、HUD 与击球交互

建议周期：4--6 周。  
目标：把现有“观察模式＋黄色线＋黄色矩形”的操作转化为无需阅读帮助即可完成的击球流程。

### 9.1 工作包 P3-A：摄像机状态机

新增语义状态，而不是在输入回调中直接堆叠角度变化：

```text
ObserveFree
AimPlayer
AimOverhead
ShotFollow
ReplayFree
ReplayBroadcast
```

每个状态定义：

- FOV。
- 目标点与距离范围。
- 输入映射。
- 是否允许改变瞄准。
- 进入/退出过渡。
- 遮挡和球台边界限制。

首版摄像机预设：球员低位、轻度高位、正俯视、电视斜俯视。镜头切换使用有最大速度和缓动的过渡，但击球前的精细瞄准不能被持续的自动漂移干扰。

### 9.2 工作包 P3-B：Shot Control Model

将玩家意图从当前 `yaw + shotPower` 扩展为版本化控制模型：

```text
aim yaw
tip offset x/y
requested power or stroke speed
cue elevation capability
chalk state capability
assist level
```

实施顺序：

1. 母球击点 UI 与水平/垂直 tip offset。
2. 新 UI 参数通过同一个 `CueImpactInput` 进入 runtime。
3. 力度从离散矩形升级为带刻度、数值和预计杆速的控件。
4. 增加鼠标拉杆/出杆实验输入，但保留键盘和滚轮无障碍替代。
5. cue elevation UI 先接 capability gate；物理未晋升时普通模式不显示。

视觉球杆、瞄准线和物理接触必须读取同一个控制模型。禁止三处各自转换角度、击点或力度。

### 9.3 工作包 P3-C：产品 HUD

默认 HUD 只显示决策所需信息：

- 当前玩家和球组。
- 当前目标/规则状态。
- 力度与击点。
- 犯规、继续击球、换手和游戏结束反馈。
- 当前镜头/辅助等级的短提示。

帮助内容移入可导航面板；默认画面不再常驻四行操作说明。调试物理参数、FPS、碰撞数据和 renderer pass 只能通过开发者 overlay 打开。

### 9.4 工作包 P3-D：瞄准辅助分级

| 等级 | 内容 |
| --- | --- |
| Off | 不显示预测线，只保留必要准星 |
| Basic | 母球到第一次接触的短线和目标球方向提示 |
| Training | 基于真实 preview runtime 的多段轨迹与参数信息 |

预测轨迹不得使用与生产物理不同的手写碰撞公式。Training 模式创建隔离的 runtime clone，复制当前状态和 profile，在严格的 tick、时间和球数预算内运行；preview 结果不能生成规则事件、音频或修改权威状态。

### 9.5 自动化扩展

建议增加 capability-gated 命令：

- `set_tip_offset`
- `set_camera_preset`
- `set_assist_level`
- `begin_stroke` / `update_stroke` / `release_stroke`，若鼠标出杆方案通过评审
- `get_presentation_state`

所有击球控制都需要 round-trip 状态查询和边界验证测试。

### 9.6 Phase 3 用户测试

至少邀请 5 名不熟悉项目的用户和 3 名有真实台球经验的用户，执行：

- 从启动到完成第一次有效击球。
- 设置一次低杆和一次右塞。
- 切换到俯视观察再回到低位瞄准。
- 理解一次犯规或换手提示。

建议门槛：

- 新用户中位数 30 秒内完成第一次有效击球。
- 80% 用户无需打开帮助即可找到力度与击点。
- 台球玩家能解释当前击点、方向和辅助等级。
- 不出现镜头移动意外改变 shot intent 的回归。

### 9.7 Phase 3 退出门

- 从观察、瞄准、设置击点、击球到观察结果形成完整闭环。
- UI、球杆模型和 physics input 使用同一数值来源。
- 每个不受支持的 cue input 有明确 capability 行为。
- HUD 在 1280x720、1920x1080 和高 DPI 下无重叠或截断。
- legacy renderer 删除前已有 modern backend 的完整 screenshot 覆盖。

### 9.8 回滚点

新 shot controls 通过独立 feature flag 开启。发生问题时可恢复 yaw＋power 输入，但不得恢复“摄像机决定击球方向”的旧耦合。

## 10. Phase 4：训练、轨迹分析与确定性回放

建议周期：6--8 周。  
目标：将 BilliardGL 的自动化场景、遥测和确定性优势变为玩家直接可见的核心功能。

### 10.1 工作包 P4-A：训练场景格式

复用 canonical physics scenario，外层增加 `training_scenario_v1` 元数据：

```text
scenario id / version
title / description / tags
initial physics scenario
allowed controls and assist level
success conditions
failure conditions
reset behavior
scoring model
reference replay ids
```

首版成功条件只使用确定、可测试的指标：指定球入袋、母球进入区域、碰库次数、最终停球区域、犯规数、最大尝试数。不要在 v1 中加入自然语言或图像识别评分。

### 10.2 工作包 P4-B：自由摆球与练习会话

- [ ] 鼠标拾取球时使用独立编辑状态，不直接绕过 runtime mutation 规则。
- [ ] 校验球间重叠、球台边界、袋内非法位置和母球存在性。
- [ ] 一键重置、上一步、重新击球。
- [ ] 保存为本地训练场景。
- [ ] 从自动化场景导入时保留 profile、单位和 schema。

### 10.3 工作包 P4-C：Replay v1

回放文件至少绑定：

- 格式版本。
- 可执行/内容版本。
- physics profile ID 与内容 hash。
- 初始场景。
- 随机种子。
- 按 tick 排序的语义 action log。
- 最终状态摘要与校验 hash。

回放以重新执行 action log 为权威，不以逐帧录制位置替代模拟。可选 checkpoint 只用于快速 seek，加载后必须能通过后续 hash 检查发现漂移。

播放能力：暂停、1/4x、1/2x、1x、2x、逐 tick、跳到击球、自由镜头和电视镜头。慢动作只改变播放调度，不改变固定 physics step。

### 10.4 工作包 P4-D：轨迹和数据解释

- 预测层显示预计轨迹；回放层显示实际轨迹，两者使用不同颜色和图例。
- 可显示入射/分离方向、母球最终位置、碰库次数和速度曲线。
- 高级遥测默认折叠，避免普通玩家被工程数据淹没。
- 所有单位明确显示，不从视觉旋转反推物理角速度。

### 10.5 首批训练内容

建议只做 12 个高质量练习：

- 3 个定杆/跟杆/拉杆基础。
- 3 个左右塞吃库。
- 2 个角袋与中袋准度。
- 2 个连续走位。
- 2 个开球与母球控制。

每个练习需要说明目标、推荐辅助等级、成功判定和参考回放。内容验收与系统验收分开。

### 10.6 Phase 4 退出门

- 任一训练局面可以加载、完成、评分、复位和保存回放。
- 同一回放在同平台重复执行得到相同最终 hash。
- profile/hash 不匹配时明确拒绝或进入兼容模式，不能静默播放错误结果。
- preview runtime 与 authoritative runtime 完全隔离。
- 12 个首发练习均有自动化成功/失败测试和人工可玩验证。

### 10.7 回滚点

训练与回放使用独立入口和文件格式。关闭入口不会改变普通比赛存档或 simulation core。

## 11. Phase 5：完整规则、犯规解释与基础 AI

建议周期：8--10 周。  
目标：从“能轮流击球”升级为可以完成一局、理解判罚并反复挑战的单机产品。

### 11.1 规则引擎准备

当前 `rules.cpp` 只覆盖基础球组与换手。完整中式八球需要先补足 shot fact：

- 首次接触球。
- 接触是否合法。
- 接触后是否有球碰库或入袋。
- 每个球的入袋顺序。
- 母球落袋/离台。
- 八号球入袋时的球组剩余状态。
- 开球阶段和合法开球条件。
- 自由球/摆球权限。

物理层只报告事实，规则层负责解释事实。不得在碰撞求解代码里硬编码“犯规”或“获胜”。

### 11.2 规则模块

- 新增版本化 `Ruleset` 接口和 `ChineseEightBallRules`。
- 明确选用的规则来源与版本；不同地区变体必须是显式配置。
- 每次判罚输出机器可读 reason code 和玩家可读文本键。
- HUD 从 adjudication result 读取，不自行推断犯规。
- 通过 scenario table 驱动至少覆盖开球、球组确定、空杆、错球、无库、母球落袋、合法/非法八号球和胜负。

### 11.3 基础 AI

AI 只通过与玩家相同的 semantic shot action 击球：

1. 枚举可见目标和袋口。
2. 使用隔离 preview runtime 评估候选。
3. 以入袋概率、母球位置、犯规风险和下一杆可打性评分。
4. 根据难度加入瞄准、力度和击点误差。

首版不追求全局最优。必须设置每回合候选数和计算时间预算，避免阻塞渲染线程。AI 计算结果应可记录到 replay，但调试搜索树不进入普通回放文件。

### 11.4 Phase 5 退出门

- 可以从开球完整进行到合法胜负。
- 每种犯规都有 reason code、HUD 解释和规则测试。
- AI 不调用玩家不可用的隐藏物理接口。
- 三档 AI 在固定测试集上体现可测量的成功率差异。
- AI 思考不会造成超过既定预算的主线程卡顿。

### 11.5 回滚点

旧基础规则在迁移期作为 `legacy_rules` 测试 fixture 保留，但产品入口只展示已通过完整门槛的规则集。

## 12. Phase 6：在线与竞技生态（独立项目）

建议周期：至少 12--20 周，不进入 M4 关键路径。  
启动条件：M3 训练版本稳定、M4 规则 Beta 通过，并且已有明确的在线用户目标。

必须先完成专门设计，不直接从本计划进入编码。至少覆盖：

- 权威服务器还是确定性 lockstep。
- 输入同步、延迟隐藏和断线恢复。
- physics/profile/content 版本协商。
- 房间、邀请、观战和比赛恢复。
- 账号、隐私、举报、聊天和封禁。
- 排名与反作弊。
- 服务器成本、日志和运营后台。

第一版建议顺序：私人房间 -> 好友邀请 -> 非排名匹配 -> 观战/回放 -> 排名。锦标赛、团队赛和商城不应同时启动。

## 13. 横向质量工作

### 13.1 性能预算

以 60 FPS 为目标的初步 16.7 ms 预算：

| 项目 | CPU 或 GPU 预算 |
| --- | ---: |
| 固定步进与规则（平均显示帧） | CPU <= 2.0 ms |
| scene preparation | CPU <= 1.5 ms |
| UI 与提交 | CPU <= 1.0 ms |
| shadow | GPU <= 2.5 ms |
| opaque PBR | GPU <= 5.0 ms |
| post process | GPU <= 2.0 ms |
| UI | GPU <= 1.0 ms |
| 余量 | >= 3.0 ms |

Phase 0 应根据参考机器修订预算。每个 render pass 使用 timer query；CPU 使用现有 timing 体系扩展。性能报告必须输出 P50/P95/P99，不能只报平均 FPS。

### 13.2 可访问性与设置

在 Phase 3 同步提供：

- 可重绑定按键。
- 鼠标灵敏度和反转选项。
- FOV 与镜头距离。
- 色弱友好的实色/花色和轨迹配色。
- UI scale。
- 单独的音乐、环境和效果音音量。
- 关闭 bloom、景深、动态镜头和镜头震动的选项。

### 13.3 音频

音频不是本轮主线，但视觉升级完成后旧声音会更明显。Phase 2--3 至少需要：

- 按碰撞冲量映射球球、球库和球袋声音强度。
- 区分球球、库边、袋口与落袋声音。
- 避免同一 tick 多接触造成爆音。
- 场馆环境声可独立关闭。

音频仍从 runtime event/physical fact 读取，不能反向驱动物理。

### 13.4 数据与可观测性

仅记录本地、非个人化的开发遥测时，建议包括：

- 各阶段耗时与质量档。
- shader/asset fallback 次数。
- 首次有效击球耗时。
- 摄像机预设使用率。
- 训练重试次数与完成率。
- replay divergence。

任何线上用户遥测必须在在线项目中单独设计隐私与同意流程。

## 14. 分支、PR 与提交策略

每个工作包拆成可以独立验证的 PR，建议每个 PR 控制在一个明确行为变化内。推荐顺序示例：

1. `visual baseline registry`
2. `presentation snapshot boundary`
3. `renderer backend facade`
4. `shader and GPU resource layer`
5. `modern vertical slice`
6. `PBR materials and lighting`
7. `glTF production assets`
8. `camera state machine`
9. `shot control model and HUD`
10. `training scenario and replay`
11. `complete Chinese eight-ball rules`
12. `AI opponent`

大规模资产更新与 shader/loader 代码更新分开提交，便于判断回归来自内容还是代码。不得在视觉 PR 中顺带修改物理参数。

## 15. 每个 PR 的完成定义

- 有与行为风险匹配的单元、集成或 rendered E2E 测试。
- `./scripts/check.sh` 通过。
- 涉及渲染时生成六场景中的相关截图，并附 actual/diff 供审核。
- 涉及物理输入时运行 release gate 和普通击球等价检查。
- 涉及 schema 时提供版本、兼容策略和坏输入测试。
- 涉及资源时通过离线 manifest 检查和缺失资源测试。
- 涉及性能时提交 before/after 的 P50/P95/P99。
- 更新对应文档，不提交普通临时截图或日志。
- 明确说明 feature flag、默认值和回滚方法。

## 16. 发布门槛

### M1：现代渲染垂直切片

- modern backend 完整可玩。
- 物理状态逐 tick 不变。
- 1080p 达到性能预算。
- 关键资源、shader 和 screenshot 测试通过。

### M2：视觉与击球体验 Alpha

- 一张完整高质量球台和场馆。
- 四种摄像机预设和完整 shot-control HUD。
- 新手可在 30 秒内完成首次击球。
- 球、台、灯、袋口、球杆通过目标板评审。

### M3：训练版本 Alpha

- 12 个训练练习。
- 自由摆球、慢动作、逐 tick、轨迹和 replay。
- 回放最终 hash 可验证。
- preview 不污染正式 runtime。

### M4：单机产品 Beta

- 完整中式八球规则和可解释犯规。
- 基础 AI 和完整对局。
- 设置、输入重绑定和可访问性达到发布要求。
- 没有 P0/P1 崩溃、数据损坏或确定性问题。

## 17. 风险登记

| 风险 | 影响 | 预防与应对 |
| --- | --- | --- |
| Core Profile 与旧 GL 代码不能共存 | modern 启动失败 | Phase 0 验证上下文；双 backend 可以是不同启动配置，不强求同一 context 混用 |
| 驱动导致截图微差 | CI 误报 | 固定机器严格 golden，通用 CI 使用区域和宽容差指标 |
| 美术资产晚于代码 | 垂直切片无法评审 | 先用批准的灰盒与材质球，尽早锁定 glTF 规范 |
| PBR 完成但构图仍差 | 技术升级未改善体验 | Phase 0 先批准目标镜头；Phase 2 视觉门同时验收灯光与构图 |
| UI 暴露未验证物理能力 | 玩家看到错误行为 | capability gate；未晋升能力默认隐藏 |
| 预测轨迹与真实结果不一致 | 训练价值受损 | 使用隔离的同一 GameRuntime/profile，不写第二套公式 |
| 双渲染器长期存在 | 维护成本翻倍 | M2 后立即安排 legacy 删除门槛 |
| 回放跨版本漂移 | 存档不可用 | 绑定 profile/content hash，显式拒绝或受控迁移 |
| 完整规则污染 physics | 后续模式难扩展 | physics 只输出 shot facts，ruleset 独立判罚 |
| AI 计算阻塞画面 | 操作卡顿 | 候选/时间预算，后台计算，结果通过 semantic action 提交 |
| 在线范围膨胀 | 主线长期无法发布 | 在线作为 M4 后的独立项目和决策门 |

## 18. 建议立即启动的第一批任务

以下任务构成第一个两周迭代，不包含生产渲染改动：

1. 建立六个确定性视觉场景及 capture 脚本。
2. 为现有画面记录 1024x768 与 1080p 基线和性能数据。
3. 建立受治理视觉 baseline 规则和 diff 产物。
4. 完成 macOS OpenGL 4.1 Core Profile 的 shader 球体 spike。
5. 完成 UI 字体与 glTF 单资产加载 spike。
6. 输出三个 ADR：OpenGL/context、UI、glTF/asset pipeline。
7. 明确 M1 参考机器和 16.7 ms 预算。
8. 运行完整 `check.sh` 和 physics release gate，保存迁移前结果摘要。

两周结束时召开一次 Go/No-Go 评审。Go 的条件不是“spike 看起来更漂亮”，而是现代路径能够截图、资源方案可维护、性能预算可测量，并且 simulation core 没有被新依赖污染。

## 19. 最终完成定义

第一轮视觉与玩法优化在满足以下条件时完成：

- 固定管线渲染已从生产入口移除。
- 一张中国式台球桌在球、台呢、袋口、库边、灯光和镜头上达到批准的目标画面。
- 玩家无需阅读帮助即可完成观察、瞄准、设置击点、控制力度和击球。
- 训练、轨迹和确定性回放成为正式产品功能。
- 中国式八球规则能够从开球运行到可解释的胜负。
- 所有视觉和玩法变化仍通过物理、确定性、性能、自动化和截图质量门。
- 在线功能是否启动由独立商业与技术决策决定，不阻塞单机训练版本发布。
