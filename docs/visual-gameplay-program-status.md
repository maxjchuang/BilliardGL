# BilliardGL 视觉与玩法优化执行状态

更新时间：2026-08-12（Phase 2 等待官方球架+Phase4/5最终共享源码后重建 / Phase 4 V16--V18 三条局部语法已证伪，Root已选择独立轨迹inset/minimap方向 / Rules UI v5/v3 manifest parity待最终构建）  
项目状态：ACTIVE  
执行依据：`docs/superpowers/specs/2026-07-27-billiardgl-visual-gameplay-optimization-plan.md`

## 目标

执行计划 Phase 0--Phase 5，达到可验证的 M4 单机产品 Beta。Phase 6 在线与竞技生态是独立决策项目，不阻塞本目标完成。

## 当前阶段

Phase 2：PBR 材质、灯光、glTF 资产与首座场馆。Phase 0 与 Phase 1/M1 已通过。

并行工作流：

| 工作流 | 负责人角色 | 当前任务 | 状态 |
| --- | --- | --- | --- |
| 玩法/架构 | C++/图形开发 1 | M4 WPA policy/referee-input critical path | policy-independent §6(c)/§17(a) telemetry 与窄产品子事务已分层；15行决策登记表 `66e105e9…a31ce` 经主Agent复验29/29，所有政策仍UNSELECTED。Audit v5保持=`11/9/13/7` |
| 现代渲染 | C++/图形开发 2 | Phase 4 dedicated trajectory inset/minimap seam audit | V18 exact-T/full-silhouette诊断与73/73 mutations已冻结为三处region不可行；正在只读映射独立viewport/FBO/semantic plane/source mapping与fail-closed资源边界 |
| 技术美术 | 技术美术 | Phase4 inset/minimap product decision + design gate | V18独立FAIL addendum `ec4f929a…3b310` 已由主Agent复验20/20；Root选择不扭曲世界投影、不删路线、不闪烁的独立inset方向，当前仅设计门、未授权实现 |
| 集成与验收 | 项目经理/主 Agent | M1--M4 门禁、真实 preview AI、设置/重绑定与冻结证据 | IN_PROGRESS |

## 当前里程碑审计

| 阶段 | 判定 | 已验证结果 | 未关闭门槛 |
| --- | --- | --- | --- |
| Phase 0 | PASS | 六场景基线、Core 4.1 spike、ADR、完整迁移前回归 | 无 |
| Phase 1 / M1 | PASS | modern 可玩竖切、只读 snapshot、双后端迁移、PBR/GPU/glTF 基础 | legacy 仅保留到 M2 完整覆盖后清理 |
| Phase 2 | PARTIAL / TARGETED V5 MACHINE+TA PASS / FULL-SIX FINAL INTEGRATION WAIT | targeted v5 两景 machine+TA PASS。v7/v8 的 pre-process failures 已冻结。产品默认球架现迁移到 WPA official fixed seed，旧 full-six source/视觉绑定不可继承；等待 Phase4 successor、Rules UI v5/v3 与官方球架进入同一 fresh build | v7/v8 auth 均不得重用；共享源码冻结后再生成 successor readiness、单次 full-six 与 TA42/42。product/golden、Phase2/M2未晋升 |
| Phase 3 / M2 | PRODUCT MODEL PASS / APP INPUT PASS / ENGINEERING FIRST-SHOT PASS / FORMAL V4 MACHINE+MANUAL 594 PASS / USER STUDY NOT_RUN | Training+Audio 8 图定向预检经 TA 8/8、64/64 轴通过后，只授权精确 runner `89dd252a…a6c9` 一次；v4 从空目录得到 594 actual GPU，主 Agent复验 594 unique ID/path/file、PNG SHA 零错、所有零指标最大值 0，默认 validator PASS。独立 TA 全量复审 594/594，人工轴 PASS/N/A/FAIL/PENDING=`3564/594/0/0`；manifest `a90c9d7e…8fc6`、manual `f4db8500…1b0e`、mapping digest `0e904a7d…a2a8` | v2/v3 保持历史 FAIL；5+3 真人研究仍 0/8 NOT_RUN，因此 M2 整体仍为 PARTIAL，不声明真人可用性或 golden 完成 |
| Phase 4 / M3 | PRODUCT LIFECYCLE PASS / HISTORICAL FAILURES FROZEN / INSET PRODUCT GATE IN_PROGRESS | v11 single-use batch前3项PASS、broadcast ordinal4固定3/1/7。V16/V17/V18三条bounded grammar均由fresh CPU replay证伪；V18 receipt `664d7b10…0398` 与独立addendum `ec4f929a…3b310` 经主Agent复验，五个production/CMake文件保持精确回退 | v11不得重跑；Root已选择独立trajectory inset/minimap，须建立新semantic plane/probe denominator与world↔inset mapping，并先关闭FBO/resolve/upload/stale-frame错误路径。机器5/5、TA35/35、targeted11/formal68、M3与12项外部真人未完成 |
| Phase 5 / M4 | PARTIAL / RULES UI V5/V3 FINAL BUILD WAIT / AUDIO TECH V2 10/10 / WPA 11/9/13/7 | v5/v2唯一授权因manifest缺`gate_sha256`在首ordinal前失败，0/16并冻结；v3 parity合同已覆盖完整preclaim字段。§6(c)、§17(a)保留policy-independent遥测，§6(f)、§10.4及§8/§9窄子事务已闭环；15行M4政策/裁判输入登记表 `66e105e9…a31ce` 已冻结并保持全UNSELECTED | 旧v5/v2授权不得重用；官方球架重基线门 `5c846067…270c9` 要求Phase2/4/5至少27张fresh证据。13行仍需root政策/外部裁判决策，§12.3/12.5-6/12.7/16/17 off-table需新或实质扩展physics；真实输入/屏读30/30、Beta0/12、音频人工/权属及外部QA未闭环 |

### 最近一次独立门禁结果

- Phase 2 v10c DESIGN_GATE 用 D=57.15 mm / 同镜头 70.75 px 标定了单一 rim-sidewall-drop 拓扑、rounded tapered void、object-ID 连续性、PBR 参数与 tone-map luma 梯度、walnut 面积/侵入/切边门。v3 金色横条已定位为 legacy shot-power UI quad，v10c pocket-only 必须 `ui draw_calls=0` 且 full-frame UI delta=0。主 Agent 复验 v3 manifest/PNG/manual 绑定与 JSON/diff，design SHA `fd830e69…50f5e`；结论严格为 `APPROVED_FOR_IMPLEMENTATION_NOT_VISUAL_PASS`。
- Phase 2 v10c 唯一 actual-GPU closeup 固定为 `REJECTED_MACHINE`：rim 可见 run 0px；sidewall median/P10/min=3/2/2px；ordered stage 0 sampled；walnut 占 mouth envelope 40.731%；upper/lower sidewall luma 均 18.222；drop bottom/mouth 也略超上界。主 Agent复验 manifest/PNG/metrics SHA `1e225a18…c2fa`/`10887037…21e9`/`8aa6f07d…19f`、全部 production binding、offline validator 与 14/14 unittest，并目视确认仍读作木质 jaw 下的窄黑缝。v10c 不得覆盖；v10d 先冻结绑定字节再以新路径迭代。
- Phase 2 v10d 唯一 actual-GPU closeup 同样固定为 `REJECTED_MACHINE`：22/26 通过，walnut envelope 从 v10c 的 40.731% 降至 0.533%，侵入 max/median=1/0px，drop、连通、UI/gold、marker 均过；但 rim 仅 6px、sidewall median/P10/min=3/2/2px、ordered stage 0/2、upper/lower luma 都为 25.294。主 Agent复验 manifest/PNG/receipt SHA `acecb570…0a56d`/`60187364…f2bae`/`7ae010a2…81e49`、12/12 snapshot、offline validator 与 14/14 unittest，并目视确认袋口仍有极薄无层次构件。v10d 不得覆盖、不送 TA、不生成六景；v10e 先做离线屏幕空间与 tone-map 标定。
- Phase 2 v10e 完成可信离线标定但严格冻结为 FAIL：CPU object-ID raster 对 frozen v10d 的 rim/sidewall/ordered/walnut/luma 残差均在合同内，随后 v10e 预测 15 个门中 14 PASS；sidewall 22/18/17/24px、ordered 11/11、luma delta/保守=15.311/12.311、walnut=0.2873%、connected=143px。唯一失败是 rim 首列 15px 超 14px；design/report/freeze SHA `030e1146…590c8`/`7ef92794…a124`/`8f245eb7…c3735`，主 Agent复跑 offline validator、资产 14/14、calibration/determinism/bad-input 6/6 与 8/8 snapshot。GPU 从未授权或执行；v10f 只做 1px 离线收敛。
- Phase 2 v10f 离线 15/15 后只执行获准的唯一 actual-GPU closeup，并永久冻结为 `REJECTED_MACHINE`：26/28 machine，rim `[13,13,12,12,11,11,11,12,12,13,12]`、sidewall `[21,23,24,24,24,24,23,22,21,19,18]`、ordered 11/11、connected 144px、walnut 0.736%、UI/gold=0 与性能 P95 2.526ms 均过；但 CPU 预测 delta 15.302 在 GPU post-tonemap 后仅为 5.787（upper/lower 9.647/3.860），失败项恰为 `post_tonemap_luma_chain` 与 `sidewall_luma_delta_safety_at_least_12`。主 Agent复验 manifest/PNG/metrics/freeze SHA `0583853e…5fc12`/`b088e4bb…49aaa`/`29686bf3…9eb8`/`7b8d55dc…ae510`、15/15 bindings、20/20 tests 和原图；未执行第二次渲染、TA、六景、formal 或 golden。冻结 metrics 的 `v10c` scope 与 `PENDING_TA` 仅记作证据元数据缺陷，不回写历史；v10g 先修离线模型与亮度安全余量。
- Phase 2 v10g 离线 30/30、freeze 11/11 后只执行获准的唯一 actual-GPU closeup，并永久冻结为 `REJECTED_MACHINE`：53/55 machine。v10f 的层级差问题已修复，实际 rim/upper/lower/void=`42.941/32.294/19.4344/2.0`，rim-upper/upper-lower/lower-void gap=`10.647/12.8596/17.4344` 全过；rim/sidewall 几何、ordered 11/11、connected 144px、walnut 0.7355%、UI/gold=0 与 P95 2.30437ms 也全过。但 upper 目标 27--31 与 lower 目标 8--11 均因整体过亮失败。主 Agent复验 manifest/PNG/metrics/freeze SHA `6dbc9c8c…7a02`/`8743545b…bd64`/`a700726d…8061`/`301af66e…ed7`、20/20 byte-identical bindings、相关单测与原图；授权回执固定 actual GPU=1、additional=false，未送 TA/六景/formal/golden。v10h 必须新建门禁、离线报告和独立授权。
- Phase 2 v10h 纯离线候选完成 89/89 machine、225/225 全网格、6 个通过候选并按门禁唯一选择 B/U/L/R=`.05625/.99/.055/.84`。双模型 spread upper/lower=`.2771/.1285`，保守 upper/lower 区间=`[27.2809,29.5581]/[8.3711,10.4996]`，delta lower=`16.7814`；仅 sidewall COLOR_0 accessors 64/130/191 与准许材质 scalar 改变，285 个其他 accessor exact。主 Agent复跑 13/13 tests、validator、10/10 freeze verify，并在空临时目录生成完全相同 glTF `99ddceb6…e136` 与 manifest `4534acc2…98a`。报告 `54ed5553…bc78`、freeze `3121c61f…53d9`；图片数 0，GPU/TA/六景/formal/golden 全 false。
- Phase 2 v10h 随后完成 20/20 capture readiness 与 22/22 tests，fresh runner/executable/ready 精确绑定后由 root 写入一次性授权；唯一 actual-GPU 结果固定为 `REJECTED_MACHINE_NO_RERUN`，48/50 checks。实测 rim/upper/lower/void=`42.941/28.294/6.8596/2.0`，upper 已进入目标，lower 低于 8 且 lower-void=`4.8596`；其余亮度链、几何、ownership、UI 与 P95 2.44254ms 均过。主 Agent复验授权消费、13/13 freeze bindings/26 个原始与快照 SHA、原图，并把 output/freeze/授权设为只读；manifest/PNG/metrics/freeze SHA=`47bcf3df…8a29`/`cc500198…7b6f`/`954b9b6e…c7fd`/`293ae897…02e0`。不得重跑 v10h、送 TA 或晋升；v10i 先用 v10g+v10h 双实机锚点签门。
- Phase 2 v10i 用 v10g/v10h 两个 actual-GPU 锚点完成 86/86 离线门与 35 点插值网格，只改 sidewall `COLOR_0` accessors 64/130/191；root 复算 15 个 readiness bindings、20/20 checks、23/23 专项测试和缺授权 GL 前拒绝后，签发并消费唯一单图授权。actual GPU 50/50，rim/upper/lower/void=`42.941/29.5066/10.1444/2`、gap=`13.4344/19.3622/8.1444`、P95=2.36971ms；13/13 freeze binding byte-identical，output/freeze/auth 已只读。主 Agent与独立 TA 均查看原始 1920×1080 PNG，TA 7/7 scoped axes PASS；manifest/PNG/freeze/manual SHA=`28c5d222…016c`/`e5f701d6…3f6e`/`df0f5853…3f2f`/`43987deb…f29a`。该结果仅关闭 pocket_closeup，不是六景、formal、golden、Phase2 或 M2 PASS。
- Phase 2 v10i 正式六景 readiness 包固定为 `BLOCKED_BEFORE_GPU`；主 Agent独立复跑 13/13 fail-closed 测试。六个工程缺口为实际加载资产逐进程 path+SHA 证明、文件/字段结构化加载错误且禁止静默白材质、runner 显式 child env、自包含六资产包、120 warmup/300 sample 的 P95/draw/texture 方法，以及正式 validator 可接受的输出 manifest。blocked manifest `7cdfc197…dcf`；未授权 Cocoa/OpenGL、没有截图，也没有六景 machine/TA/golden 晋升。
- Phase 2 formal readiness v2 虽关闭上述六项，却在 root 签发前由独立审计发现 formal runner 未显式选择 modern；v3 又补齐 authorization identity/time/唯一绝对 output 绑定。最终 v3 readiness `ed8a1767…de00` 经 22 项 source binding、2 个 C++ CPU 与 47 个 Python tests 后只消费一次授权。唯一运行在第 2 景 `low_aim` fail-closed：`break` 原图有效，`low_aim` 仅底部 gold bar 可见，目标 region visible=0/max=2；冻结 PPM SHA=`92f3435b…40b`/`ea9f886a…aa36`，failure receipt `818ceb4b…291d`，状态 1 PASS/1 FAIL/4 NOT_RUN。不得删除 output 后重放；v4 先做 NO-GPU 相机/scene-state 根因与修复门。
- Phase 2 的冻结 v3 黑帧被精确归因为 `low_aim` 截图停留在 PlayerLow transition alpha=0 的高位灯具遮挡起点。修正仅新增 automation-only `instant:true` semantic preset seam，普通产品相机过渡与物理 tick 不变；六景 registry v2 显式锁定最终 camera state/preset/transitioning=false。新 targeted v5 只消费一次 `break,low_aim` 授权，两张 1920×1080 PPM 均 machine PASS：visible=`2,065,987/1,996,633`，目标 ROI=`1,121,348/555,841`，GPU timer observed、P95=0.996875ms、draws=85、texture=27.0143MiB。独立 TA 随后按原分辨率完成 2/2、13/13 轴 PASS，manual `79f0efcf…2931`，确认玩家低机位、杆球轴、球堆/袋口和灯具 clearance 均可读；这仍只是 targeted machine+TA 证据，full-six、product/golden、Phase2/M2 都未通过。
- Phase 3 formal v3 使用预检中获准的精确 runner 从空目录完成 594 张 actual GPU，但官方 validator 默认 fail-closed 为 590/594：仅四个 TrainingSession 1280×720/ui135 ID 失败，每张都有 2 个 truncated text runs；其余 590 张机器指标通过。manifest SHA `9eb765a5…db52` 经主 Agent复验，状态保持 `FAIL` 且没有生成 TA manual；该目录不可补写或覆盖。下一轮必须先以新 runner 完成覆盖四个失败轴及 Audio 回归控制的定向预检，再由 TA 精确绑定授权。
- Phase 3 Training+Audio 定向预检以 manifest `328bb26e…b2e` 和 runner `89dd252a…a6c9` 精确绑定，主 Agent复验 8 张 PNG/尺寸/source SHA 与零指标，TA 原图复审 8/8 capture、64/64 axes PASS（manual `87905b9b…c202`），且只授权一次空目录 formal。由此产生的 v4 manifest 为 `a90c9d7e…8fc6`：594 unique ID/path/file、逐图 SHA mismatch=0、mapping digest `0e904a7d…a2a8`、machine 594/594 PASS；P95/draw/texture 峰值分别 12.504166ms/82/75.654930MiB，所有零预算指标最大值为 0。TA 随后从零完成 594/594 原图复审，manual `f4db8500…1b0e`，人工轴 PASS/N/A/FAIL/PENDING=`3564/594/0/0`；主 Agent再次复验 review ID、嵌套 artifact SHA、N/A 分布与默认 validator，正式 GPU 视觉门通过。5+3 真人研究仍为 0/8 NOT_RUN，故不提前声明 M2 整体完成。
- Phase 4 真实产品生命周期现在在 Training 的 Shoot 成功前标记重试状态，成功后按 authority journal 去重消费 action/physics telemetry，累计首碰、每球碰库、入袋顺序、杆数与 tick；多杆非终态不提前 finish，只有 success/failure 才封存 Replay。reset/retry 清累计器并重建 recorder；可执行版本由 session attestation 注入，seed 取 card reset 合同，Open/Save 文件对话框通过可注入 adapter 与 macOS 实现分离。主 Agent 独立构建 `Billiards` 并复验 Phase4Controller/Input/PointerSession 3/3 PASS；本结果仍不代表 GPU UI 或人工可玩性。
- Phase 4 Overlay DESIGN_GATE 以 SHA `55b9caa8…2d22` 固定 predicted amber dashed+ring 与 actual cyan solid+ticks 的非颜色冗余、共享 lookAt/perspective homogeneous clip、各 profile 物理像素范围、legend/endpoint/rail/score/reason/replay-ready 层级及 Advanced 默认折叠；最低证据为 68 张 actual GPU，P95≤16.7ms、UI+overlay≤1ms、完整场景 draws≤100/新增≤8、纹理 delta=0。它只允许实施，不替代 GPU、TA 或 12 项外部真人记录。
- Phase 4 定向 actual-GPU v1 曾产生 7/7 machine preflight、12 个 Replay fixture、P95=3.569ms、UI+overlay GPU P95=0.557ms、8192 vertex fail-closed 与 authority before=after；但主 Agent复验时发现 manifest 绑定 source `7f6db4c8…028bf` 已漂移为 `55055991…6a45`，重建 runner 也不再匹配绑定哈希，且正式 dispatcher 用逐项 flags 调用只接受 batch plan 的 runner。故 v1 只保留为过时机器证据，不送 TA、不授权 68；v2 必须关闭可复现与端到端 dispatcher 门。
- Phase 4 v2 修正为一次 batch runner、exact 19-field plan、68 rows + 1 summary 严格校验与 PPM→PNG/evidence 闭环；5 个 missing/drift/incomplete/duplicate 坏输入测试由主 Agent复跑通过。fresh fixture manifest `509d8e77…211a`、68 ready `659ae45c…1def`、七图 preflight `f7a18082…b451` 均精确绑定 source `55055991…6a45`、Billiards `ad5c42e3…67d9`、runner `56588f45…d5be2`。七图 machine 7/7，draw max=84、texture=28.33MB、P95 total=4.359ms、overlay CPU=0.0119ms、UI+overlay GPU=0.615ms、8192 fail-closed；formal68=0、TA/manual NOT_RUN。主 Agent已逐张查看原图并交 TA 独立复审，尚未授权 formal。
- Phase 4 v2 独立 TA 原图复审随后以 manual `59f269a0…60f6` 固定为 FAIL：7 图×10轴=70，PASS/FAIL/N/A=`31/19/20`，19 个 FAIL 全 blocking。机器 overlap=0 未覆盖感知问题：全屏深色 row/card 持续压暗游戏区与双轨迹；720/ui135/CVD 和 Advanced 缺 `Rails N`；Retina actual-only 路径过暗且缺 `Replay actual` 图例；Advanced 缺 first/last tick、preview budget outcome、trace/content hashes。主 Agent复验 7/7 ID/PNG SHA、计数与授权字段，runner/ready 均 NOT_AUTHORIZED，formal68 保持 0；v2 不得覆盖，修复需 v3。
- Phase 4 v3 correction DESIGN_GATE `9cba251a…28bb` 已由主 Agent复验原 DESIGN_GATE、v2 manifest/manual 与 7 PNG 共 10/10 SHA。它把 1080 inspector 限为 ≤480×780/≤18% frame、无面板 corridor ≥1240×760 且与轨迹保护区 0 交叉；720/ui135/CVD 使用 ≤420×620 单列而非全宽卡片；轨迹 local contrast 机器底/设计目标为 4.0/4.5:1，并固定三档物理 stroke/dash/tick/endpoint/halo；Rails N、actual-only legend 与 Advanced 八字段不可省略。该门只授权 v3 实施，不授权 GPU/formal/M3。
- Phase 4 v3 定向预检后来完成 machine 7/7 与独立 TA 7/7、70 轴 PASS/N/A/FAIL/PENDING=`50/20/0/0`；单独授权的 formal68 从空目录执行到第 20 项时 fail-closed，冻结为 19 PASS/1 FAIL/48 NOT_RUN。唯一失败 `core__launch_right_english_one_rail_v1__broadcast` 的 source/visible/hidden probes 为 159/128/31，coverage 0.805031；对比、布局、内容、性能等其他记录门仍通过。failure receipt `a6c900f7…15ed` 绑定 20 个原始 1920×1080 PPM，v3 不得续跑或改写，也不构成 M3。
- Phase 4 v4 compound-visibility gate `1901a825…9f` 绑定 17 个 v3 输入，只授权 capture-side telemetry：同轨 final ownership、稳定 probe ID、compound core/halo-gap/tick/endpoint、独占像素归因与负向夹具；aggregate 和双轨 coverage 仍各 ≥0.90，对比与全部 v3 门不得降低，screen-space offset 上限为 0。下一步先实现和离线测试并生成至少 11 项 NOT_RUN preflight ready；actual GPU、TA、formal68、M3 和外部真人仍全部 false。
- Phase 4 v4 在 fresh source aggregate 与 11/68 NOT_RUN readiness 经主 Agent复验后只消费一次 targeted11 授权；首项 `camera_player_low_1080` 即以 source/visible/hidden=`95/46/49`、coverage=0.484211 fail-closed，固定为 0 PASS/1 FAIL/10 NOT_RUN，PPM `f3626f44…744`、failure receipt `2dbd4992…17f`。主 Agent已查看原始画面；不得重跑或转 formal68，先由 TA 区分真实几何遮挡与 attachment attribution 丢失。
- Phase 4 v4 ordinal1 独立 TA 根因门 `ad95db70…7fa` 经主 Agent复核 12/12 SHA、6 个可证伪假设与 5 项离线诊断。原图显示 predicted/actual 大范围同轨叠加，但面板/场景遮挡不是 49 hidden probes 的主要解释；更关键的是 semantic attachment 为脱离生产 MSAA/blend/tone-map/UI 的单采样重放，且 exact-role 候选规则不能用同轨 compound 的 halo/gap/tick/endpoint 证明可见，失败前也未序列化 per-probe 理由。当前严格为 `DIAGNOSTIC_REQUIRED_NOT_IMPLEMENTATION_APPROVED`，覆盖率 0.90、对比、95 分母、零 offset 与 probe 集均不变。
- Phase 4 final-ownership instrumentation 已按 `c3b6f032…34056` 完成 A→B：失败前 durable telemetry、同 production pass graph 的 MSAA/blend/tone-map/later-UI ownership、12/12 CPU 正负夹具、冻结 95 probe replay 与 static conformance 均 PASS。最终 v3 readiness `943d9bc1…eeb9` 经主 Agent同轮复验 readiness 9/9、dispatcher 7/7 与无授权 dry-run 后，只消费一次 targeted11 授权；runner 在任何 ordinal telemetry 前因 `integrated_semantic_msaa_resolve` 报 OpenGL 1282，固定为 0 completed/0 failed ordinals/11 NOT_RUN。授权/consumed/failure SHA=`b31a8631…29c9`/`82635d6c…c44e`/`8bf47205…1804` 已只读冻结，禁止重跑；当前只做 NO-GPU 根因与新 correction gate。
- Phase 4 v3 的 GL1282 由离线证据 `8dfd743a…5f04` 精确隔离为 `usampler2DMS` 与 `usampler2D` 两个 active sampler 同指向 unit0；最小门 `22a9cb6f…a6d` 后拆成两个独立 linked programs，12/12 static mutations、12/12 semantic fixtures 与 fresh Release build 均过。新 v4 readiness `a823fbae…d33b` 只消费一次授权并成功产生完整 ordinal1 telemetry，但 machine 仍报旧式 46/49；integrated telemetry 自身为 50/45、aggregate coverage 0.526316，并记录全部 candidates/arbitration/LDR RGB。批次固定为 0 completed/1 failed/10 NOT_RUN，failure `32ddc598…15e`，当前严格 NO-GPU 诊断为何新 telemetry 未转化为批准的同轨 compound visibility；阈值、95 denominator 与计划不变。
- Phase 4 v7 离线诊断 `3db8ccde…31ec` 证明 boundary provenance 被 per-probe area arbitration 分割；algorithm-only gate `9f6c5feb…2cf2` 后以 exact primitive stem/owned-island association 修正且不改 area ownership、阈值或视觉。v8 readiness `9a9d44ff…e44af` 只消费一次授权：ordinal1 从历史失败转为 PASS，ordinal2 elevated 权威 corrected integrated 为 72/83、coverage .867470（40/43 是 legacy diagnostic-only），固定 1 PASS/1 FAIL/9 NOT_RUN；failure/telemetry/PPM=`1b4cac8f…cfd89`/`48d34fb1…bd99`/`1761d043…1da7` 已只读冻结。83/83 boundary/ownership 有效；9 个 outer-silhouette LDR contrast<4、2 个 owned area不足。主 Agent原图观察到同轴重叠并获 telemetry 距离分布支持；后续仅可在新门下做 scoped visual-style/area 修正。
- Rules Core H 将 WPA Heyball §6(c)/(e) 的对手选择实现为稳定 `break_foul_opponent_choices` authority：非法开球仅提供 accept-table 与两种确定 breaker 的 Core E rerack，其他开球犯规仅提供 BehindHeadString 子事务与两种 rerack；父事务提交前不授予 BIH。§6(g) 需要“非故意 miscue 且未碰 rack”，现有 ShotFacts 缺 intent/miscue，因此 generic no-contact 以 `rules_break_no_rack_contact_classification_pending` 零 mutation 拒绝。主 Agent 独立重建 7/7 CTest、pending UI 16/16、audit 15/15 PASS；官方计数未静默重算，Product UI 仍隐藏。
- M4 的既有 AI/设置基线由主 Agent 在独立 Release build 复验 8/8：AI planner、600-case acceptance、async worker、Product AI turn、input adapter、settings model/menu/runtime 全部 PASS。固定 600 次接受集的 Rookie/Club/Expert 分别为 247/371/498，即 41.17%/61.83%/83.00%，位于批准区间且单调；该测试度量 public semantic control 的执行误差窗口，不冒充真人完整物理对局胜率。Phase 5 Beta PENDING 模板仍为 0/12 NOT_RUN，当前因 input/rules/shell source SHA 漂移 fail-closed，待本轮生产路径稳定后统一重生成。
- Rules UI DESIGN_GATE `26567d05…a08` 的 8 个只读 source/token 绑定与 12 个 authority action ID 已由主 Agent逐项复验。它要求 modal 打开时无选择/无 armed/无 timeout action，第一次激活只 arm、第二次独立 Confirm 才提交；placement preview 永不修改 authority；stale/drift/ACK mismatch/AI actor 全 fail-closed；Core E foundation actions 绝不进入产品节点；§6(g) 只显示阻塞原因，§12.3/§17(e) 与 stalemate 不作 UI 推断。进入产品暴露前必须 540/540 机器状态、60/60 actual GPU、60/60 十轴人工及真实键鼠/双语/CVD/screen-reader 会话通过。
- Rules UI 第一层实现已新增独立 authority state/request/ACK model 和 renderer-ready draw-list，机器矩阵覆盖 15 state×2 locale×2 palette×3 viewport/density×3 scale=`540/540`；主 Agent从空临时证据路径重跑得到与正式 evidence 完全相同 SHA `8e478cf7…b9ac`，validator、4 个新坏输入测试与既有 16/16 pending contract 全过。60 项 GPU/manual 模板为 `0cd3c325…31e`，全部 PNG null、GPU/manual PENDING；当前实现尚未编入 product Core4.1 evidence path，故 product_exposed=false、Beta 0/12 且不能授权 GPU。
- Rules UI 后续已接入 `ModernProductSession`、生产 draw-list/hit-rect、`ProductUiFrameCoordinator`、runtime 与 ACES 后 renderer，并以 evidence mode + 空默认 internal capability 双门保持正式产品关闭。主 Agent复验 4/4 相关 CTest、23 个 source binding aggregate、60 个 unique capture ID（15 state×4 profile、双语/standard/CVD 各 30）及未授权 output=absent；machine evidence 重生成 SHA `db92c8d6…ad0`，ready `9321c021…3987`。actual GPU/manual/Beta 仍 0/60、0/60、0/12，不能据此 product expose。
- Phase 5 音频内容修复 DESIGN_GATE `c9bdbe92…1e00` 绑定 10/10 输入并由主 Agent独立重算 4/4 WAV：现有 10 个语义 cue 只有 4 个物理 WAV、仅 `pocket_drop` 为一对一，9 个语义仍共享别名；GameOver 有 111 个满幅 PCM 样本，四文件采样率分散且均非拟定 48k/24-bit master。该门只规定 9 个新增原创 distinct 资产及 peak/loudness/transient/loop/无削波/盲测/权属门，不是听音、设备播放、内容接受或 Beta 授权；True Peak、LUFS、外部真人与权属证据仍为 NOT_MEASURED。
- Phase 5 据此生成的 `assets/audio/phase5-v1` 含 10 个一对一语义候选（10/10 file SHA 与 decoded PCM SHA 均唯一），全部 48kHz/24-bit PCM；8 个 mono one-shot 的 onset-to-peak 最大 17.98ms、crest 14.18--16.42dB、尾 10ms 均低于 -139dBFS，2 个 stereo loop 均 32s、边界 jump 量测 floor -300dBFS、头尾 100ms RMS delta 最大 0.234dB。manifest `d213424d…a7cc`、generator `7c2b7afa…e9ce`；主 Agent复跑 validator、7/7 fail-closed 测试并从空临时目录得到 byte-identical 11 文件。状态严格为 `MACHINE_CANDIDATE_NOT_CONTENT_ACCEPTED`，旧 WAV/产品映射未改，听音与外部接受前不得接入 Beta。
- Phase 5 audio v1 随后以纯离线 stdlib 工具完成 BS.1770、8× intersample true peak、crest/onset/tail/clipping 与 loop seam/proxy 复算；主 Agent重跑全量 validator、6/6 坏输入测试并从 WAV 得到 byte-identical report `28d15d3c…e2bc`。8 个 one-shot 全过；`music_loop=-16.648294 LUFS` 对 `-24±2`、`venue_ambience=-25.026475 LUFS` 对 `-30±3`，故严格为 8 PASS/2 FAIL。设备打开数 0，未播放、未改 WAV、未接受内容；下一步只允许新 v2 根中的常数增益修正。
- Phase 5 audio v2 严格按 design gate 只在新资产根对 music/ambience 施加 -7.36/-4.98 dB 常数增益；主 Agent复验 10 个资产 SHA、8 个 one-shot byte-identical、RIFF 非 data chunk、全量 validator 与 7/7 测试。report `b4153d5b…5f11` 的 music/ambience 为 `-24.008294/-30.006475 LUFS`，技术门因此 10/10 machine PASS；状态仍是 machine-only，设备听音、真人内容接受、权属、产品接线与 Beta 均为 false。
- Rules UI v1 只消费一次 60 项 GPU 授权；第 1 项完成后，第 2 项 `group_choice_open_unselected--p2--zh_cn` 因 capture runner 将文本 `1.35` 读成 float 并扩宽为 `1.3500000238`，被 coordinator 的精确 double 上界 fail-closed。结果固定为 1 完成/1 FAIL/58 NOT_RUN，receipt `003bfe13…351e`；v2 门只允许 runner `CaptureCase.uiScale float→double`、`stof→stod`，不得改 production source、合法区间或阈值。
- Rules UI v2 的 runner-only 修正由主 Agent复验为精确两处 delta；20/20 source bindings、CPU frame 60/60+5/5 坏输入、4/4 产品集成和两次 540/540 byte-identical machine 均通过。随后只消费一次新授权并完成 actual GPU 60/60：ID/plan/PPM SHA/尺寸/canonical GPU hash/after-tonemap/draw=1 全部闭合，results `4cd41cec…79f`、PPM aggregate `f68335a4…a5f`、machine receipt `3b9a807c…cc0`；root 原尺寸抽查 12 图未见 blocker，但独立 TA 60×10 轴仍进行中，故 manual/product/Beta 不晋升。
- Rules UI v2 独立 TA 最终完成 60/60 原尺寸、600 轴并冻结为 blocking FAIL：PASS/FAIL/N/A/PENDING=`472/32/96/0`，40 case PASS/20 FAIL。失败是 4 个 armed-confirm 缺 exact action+actor 文案与 focus/armed/confirm 非颜色区分，以及全部 16 个 placement valid/invalid 缺 ghost cue-ball、invalid cross-octagon+dashed ring、behind-head hatch/boundary/world text；manual `40ab2954…40c0`。v3 repair gate `6b587c67…5e25` 只允许 presentation copy、draw shapes、table-world overlay 与测试，要求 540/540 machine、另行授权 28 GPU 和 TA 28×10；product/Beta/真实输入/屏读仍 false。
- Rules UI v3 随后严格按 gate 完成 presentation-only 修复：4 个 armed-confirm 具备 exact actor/action 双语文案、focus-chevron/selected-check-diamond/confirm-double-frame+enter-arrow；16 个 placement 具备 ghost cue-ball+crosshair、invalid cross-octagon+dashed ring、behind-head hatch/boundary/双语 region+coords。fresh build 下 540/540 两次 byte-identical、CPU 28/28+5/5 与定向 runtime tests PASS，readiness `f13625b6…17e5f`。唯一 targeted28 授权尝试在 ordinal0 前由 runner 报 `hud_token_sha256`：readiness/auth 漏掉 runner 强制的 HUD、Shell 与 canonical source-binding manifest 三字段；无 marker、无 output、0 completed/0 failed cases/28 NOT_RUN，root 策略撤销该授权。failure/correction gate=`f29fe3ee…3a8a`/`2155c603…77a3`，视觉实现尚未获得 actual GPU/TA 证据。
- Rules UI v3 的 binding correction 随后用 fresh manifest 加入 HUD/Shell runtime rows，并统一 `source_binding_manifest_sha256`；主 Agent复验 runner 18-field auth 合同、相关 Python 38/38 与 runtime 后，唯一新授权从 fresh root 完成 exact 28 actual GPU。权威 JSONL/evidence/PPM/PNG 均为 28，runner 的 legacy `60/60` stdout 明确不作计数；machine receipt `c7be73e9…e1c4c`、case/output aggregates=`ac9288a1…f7e4`/`741ac3a0…9284`。独立 TA 原图复审 24 case PASS/4 FAIL、272/8 轴；16 placement 与 8 controls 无回归，但 4 armed-confirm 只显示裸 ✓/→，没有可辨的独立 focus chevron、selected check-diamond、confirm double-frame+enter-arrow，manual `f044e39c…b954`。因此 scoped visual repair 仍 FAIL，product/golden/M2/Beta/真实输入/屏读均未晋升。
- WPA Heyball as-built reconciliation v2 逐条绑定 107 项 source/test evidence，主 Agent复跑 validator 与 10/10 坏输入并复核全部路径/SHA；40 行当前为 `8 IMPLEMENTED / 12 PARTIAL / 13 MISSING / 7 PROCEDURAL_ONLY`，其中 17 个 M4-required 未闭环。audit JSON `a1e675cc…4809`；§12.3/§17(e) 保持 policy_decision_required，§6(g) 保持 classification_pending/零 mutation，未 product reachable 的 Core C--H foundation 不计 IMPLEMENTED，`profile_complete=false`、`product_exposed=false`。
- Rules Core G 将 BehindHeadString 自由球摆放实现为 stable context/action 权威事务：WPA-II-03-E/08 的页码与 production `-Z` head side 在 offer 中可读，母球中心必须严格满足 `z < -playfieldLength/4`，同时重验 finite、实际球径台界、袋口/颈/库角及活动球重叠。Core F 犯规 8 号 spot 成功后原子生成该 offer；提交、重试、冲突、漂移、无效 authority geometry 与 reset stale 全覆盖。主 Agent 独立重建与 CTest 7/7、UI 合同 16/16、audit 15/15 PASS，Product HUD 仍隐藏，冻结 SHA 未变。
- Rules Core F 完成 `eight_on_break_spot_or_rerack` 权威事务：合法分支仅提供 `spot_continue`/原开球方重摆，犯规分支仅提供 8 号复位+开球线后自由球/对手重摆/原开球方重摆；提交在任何 mutation 前重验 context、decision/revision/actor/action/payload/idempotency 与完整球状态 hash，再原子复用 Core E，无解或状态漂移保证零部分修改。Automation 可读写 runtime action，Product HUD/Shell 仍隐藏。主 Agent 独立重建与 CTest 7/7 PASS，另复验 pending UI validator+16/16 和 completeness validator+15/15 PASS；三份冻结 SHA 未变，未触碰 §12.3/§17(e)。
- Rules Core E 新增 `ball_respot_and_rerack_executor` 内部 foundation：WPA-II-04 的 foot spot/apex、中心 8 号、两底角异组与最少同组邻接约束同固定 seed 的非官方全编号顺序明确分层；16 球 rerack 与 WPA-II-07 单球 respot 都先生成只读 revision/hash-bound plan，提交时按当前完整球状态重算并拒绝篡改/陈旧计划。成功原子清运动/pocket interaction、revision+1 且 exactly-once 记账；无解、冲突或 reset 后零部分修改。主 Agent独立 7/7 PASS；automation capabilities 与 Product UI 均明确不暴露 foundation action。
- Phase 2 v10b v3 证明 machine geometry 不等于感知深度：尽管 16/16 机器门和 9 项 production binding 全部闭合，TA 仍在 1920×1080 整帧中确认黑腔与矩形槽断开、liner 无连续深度梯度、walnut wedge 破坏过渡和金色横条污染四个 blocking 根因。主 Agent 复验 manifest/PNG/manual SHA 为 `3aaac740…caa1`/`fb00df3d…c1d`/`b6399859…d0361`，JSON 与 diff 均正常；manual 明确 `formal_six_scene=false`、Phase2/golden=false，v3 固定为 FAIL。
- Phase 3 v2 的 594/594 machine PASS 不能覆盖人工失败：TA 已查看全部原始 PNG，590 张 PASS、4 张 FAIL，只有 `text_readability_and_hierarchy` 轴失败 4 次；失败固定为 Audio Settings 在 1280×720/ui135 的 standard/CVD 与普通/Retina 四个 ID，第二行文本被 row container 裁切。主 Agent 复验 594 review ID/SHA 与实际 PNG 全部闭合、axis PASS/FAIL/N/A=3560/4/594、PENDING=0；官方 validator 默认 fail-closed，仅 `--allow-failed-manual` 允许归档结构。manifest/manual SHA 分别为 `6bffb485…0288`/`51f16043…db3c`，v2 不可覆盖。
- Rules Core D 为普通 post-break ball-in-hand 增加稳定 actor/revision/decision offer、authority 只读 preview 与 exact `rules.ball_in_hand.confirm_full_table_placement`。提交以实际 production table/ball radius 重验 center bounds、pocket mouth/throat/jaw、active-ball overlap 和 finite 坐标；成功才原子恢复母球、清 pending、revision+1 并 exactly-once 记录 event/action，幂等/冲突/reset stale 完整。behind-head-string 不复用全台 action且保持阻塞；主 Agent 独立重建复验 7/7 PASS，Product HUD 仍无该 action 暴露。
- Phase 3 修复预检以第二次最终采集 manifest SHA `839737a9…2313` 为唯一冻结绑定；TA 在发现并排除过时的首次哈希后逐张查看 25 个原分辨率实际 GPU PNG，25/25 PASS，五类旧缺陷分别为 3/3、3/3、7/7、7/7、11/11 PASS。人工 review SHA 为 `58c2809c…b0c1`，主 Agent 复核 capture ID 与 PNG SHA 25/25 一致。该证据仅允许启动新一轮正式 594，明确不替代旧 FAIL、不授予正式验收/golden，也不代表 5+3 真人研究。
- Rules Core C 将 §10.4 同时定组 pending 扩展为稳定 revisioned authority 事务：offer 绑定 decision/actor/exact action，提交具备 expected revision、idempotency、原子互补组选、完整 effects/readback、64-hex state hash、reset stale 与 exactly-once event/action journal；AI 不代选且 Product HUD 不暴露 staging。主 Agent 独立重建并复验 GameRuntime/HUD/Rules/AI/Collector/RulesIntegration/Automation 7/7 PASS，冻结三份 SHA 未变；产品决策 UI 仍未晋升。
- 规则运行时选择性回归 18/18 PASS，AutomationProtocol 1/1 PASS；默认 Legacy 普通击球等价。
- Phase 4 系统、规则核心、shot-fact、AI planner/acceptance/worker、设置模型的针对性 CTest 均通过。
- AI 固定 600-case 结果：Rookie 41.17%、Club 61.83%、Expert 83.00%，均位于批准区间且单调；Wilson 95% 区间同时输出。
- 真实 AI preview evaluator 使用 authority 的 state/profile/boundary clone；测试确认 authority hash、tick、events 不变。
- AI 产品入口支持 `--ai-opponent --ai-difficulty rookie|club|expert`；主线程只做零等待 poll，pending 时冻结 authority tick，陈旧结果拒绝。
- ReplayRecorder 已接统一 authority 成功动作流；主 Agent 独立复核 Phase4/recorder/AI/settings/rules 5 个 CTest 5/5 PASS。
- Training/Replay/Trajectory 产品层与 Product HUD/完整规则产品态的独立复核 CTest 均通过；这些结果不替代实际 GPU UI 与人工可玩验收。
- Phase 2 正式 v5 独立复审为 0/6 machine、0/6 manual；失败截图不可自我批准为 golden，SSIM >= 0.93 门保持 unresolved。
- Phase 2 v6-l 独立机器复审仍为 0/6；性能、upright ball identity 与 cue/guide 分离门已通过，但六景的局部结构、明暗层次、袋口几何可读性和边缘密度仍不足，因此永久 reject，未进入正式人工门。
- 产品音频将每个真实公开 authority fixed-step 的有界只读遥测/运行时事件映射为 typed cue，并由 `ModernProductSession` 与 `ProductAudioController` 按 generation/attempt/tick/sequence 恰好一次去重后提交 SDL_mixer consumer；失败或 isolated step 不消费，reset/场景替换不重播。四总线即时音量/mute、lazy device init、WAV cache 与显式 device/asset/channel 错误已实现。主 Agent 独立补跑 `BilliardsModernProductAudioSessionE2E` 1/1 PASS，并复核 fake E2E 不链接 SDL/Cocoa/OpenGL。现有四个 WAV 的共享映射明确标为 `degraded_asset_alias`，SDL_mixer 不伪装支持 pitch。
- `ProductInputController` 将 30 个产品动作/37 个默认绑定经动态上下文路由到公开的 GameRuntime、训练、回放和设置接口；新增 Aim 上下文 ArrowKeys 击点调整按 physics profile 径向上限 clamp，并经公开 `GameRuntime::setTipOffset` 应用；旧 v1 settings 只补缺失绑定。`ProductInputAdapter` 与 `ModernProductSession` 已接 Cocoa 实际输入路径。鼠标壳命中直接复用 draw-list 的稳定 ID/物理矩形，覆盖 720p、1080p、Retina、边界/空白/移出/右键与 Settings 行激活；Match/Training/Replay 游戏态仍走既有输入 authority。该结论仅关闭 CPU/真实应用输入路径，不代表 GPU UI 或人工可玩验收。
- `ProductShellController` 提供 Main/SinglePlayer/Match/Training/Replay/Settings/Help 九态，只暴露获批 WPA Heyball 与三档 AI；shipping SDF draw-list 覆盖 4 视口×3 UI scale×9 screens=108 组。主 Agent 独立复核 hardening 后测试 PASS；这仍不替代 GPU 截图与人工可玩性。
- 43-token Shell 包已由独立 strict loader 验证真实文件 SHA 与 exact schema；九屏 108 组 draw-list 在 standard/CVD 下均无残留 `Icon`、unknown 或 opaque fallback。CPU frame coordinator 每帧只选择一个 overlay：Match 为 Gameplay HUD，其余八屏保留 Shell controls；非产品/automation/windowless 路径为空。该结论不声称生产 renderer 集成或产品 capture 已完成。
- 独立 `ProductUiGpuRenderer` 与 Core 4.1 dispatch 已通过无头 recording-dispatch 和真实 sRGB FBO readback smoke；top-left physical px/NDC、GL scissor、straight alpha、SDF R8 atlas、standard/CVD 原值、resize/context loss、状态恢复及显式错误均有测试。它尚未在 `ModernRenderer` 产品帧被调用，因此不能把独立 smoke 表述成 594-case 产品 capture PASS。
- `ModernProductUiRuntime` 以可信默认 SHA 或注入配置校验 HUD manifest/HUD tokens/Shell tokens/font，统一执行 strict load、registry merge、atlas freeze、coordinator build 与 GPU render，并返回 overlay/screen/canonical SHA/metrics/error。无头生命周期 E2E 已覆盖缺失/篡改/unknown inventory/font face/context/upload、resize stale、context loss/reinitialize/shutdown，以及未初始化隐藏模式零 draw。
- `ModernProductUiRuntime` 已由 `ModernRendererProductUiPass` 接入真实产品帧：ACES tone-map 后在 LDR/sRGB target 进行 straight-alpha UI submit，成功后才 blit/present；legacy/automation/windowless request 保持原五 pass 且 product backend 零调用，失败码显式。主 Agent 独立构建主程序并复跑 Shell/runtime/renderer/Core4.1/app E2E 8/8 PASS，目检 2560×1440 Main/default GPU smoke 的六个入口与 descender 字形完整；该 smoke manifest 明确 `is_594_case_completion=false`。
- `ProductInputAdapter` 将 Cocoa key up/down、滚轮、鼠标移动/拖拽/按钮、修饰键和 Retina backing 坐标归一化；`ModernProductSession` 让设置、训练、回放、Shell、AI 与 renderer 共享同一个 GameRuntime。`--product-mode` 保持 opt-in，automation/screenshot/windowless 路径隔离；新增 headless pointer-session CTest 与 macOS app-session Retina 点击 E2E。尚未接 GPU 壳绘制，且不能以 CPU/app 测试替代人工可玩验收。
- `BilliardsModernProductFirstShotEvidenceE2E` 通过同一产品会话的 Retina Shell pointer 与 Match 键鼠输入完成 Main→SinglePlayerSetup→Expert AI→WPA Heyball→Match→Observe/Aim→非默认 yaw/tip/power→release→权威 `shot_ended`；恰好一次 `shoot`、cue contact applied、AI jobs=0、Help=false、Assist 无自动瞄准/击球权限，主 Agent 独立 5/5 回归 PASS。证据明确为 `engineering_proxy` 且 `phase3_five_plus_three_user_study=NOT_RUN`。
- Phase 5 规则反馈/本地化合同锁定当前 exact 8 个 `FoulReason` 的机器码、真实 text key、中英文标题/解释/下一步与 turn/ball-in-hand/win/loss 文案；validator 与 16/16 坏输入测试经主 Agent 独立通过，flat proof 目检。break foul choices、8-on-break spot/rerack、stalemate、free-ball placement 四项均为 `capability=pending`、`product_exposed=false`；该合同不改 runtime/规则语义，且明确 `profile_complete=false`、`FLAT_NOT_GPU`。
- WPA 官方 `Rules of Heyball`（2025-08-16）21 页、PDF SHA `2b92dd50…24a24` 的旧审计 4/12/17/7 保留为历史；最新 as-built v2 已按 Core A--H 与 product gate 重算为 8/12/13/7，并以 107 项路径/SHA/行号 fail-closed。collector 的 off-table identity 和官方 §12.3 p9/§17(e) p11 内部冲突仍未关闭；在规则负责人签署前后者固定为 `PARTIAL + policy_decision_required`，完整 profile 继续不可选。
- Rules Core B 把首次 cue/object 碰撞扩展为严格升序、去重的同 physics tick 集，collector 以两阶段扫描消除 rail/contact 记录顺序差异；实现§13 合法球推定和§10.4 两组同时进球的 `groupChoicePending`。pending 下 hand adjudication、Shoot、direct cue impact 和 AI 全部返回 `rules_group_choice_pending`，且 physical hash/tick/events/applied-actions 不变；Product HUD 屏蔽 staging options，automation 仅读回冻结的 exact action ID。主 Agent 独立重建并复验 Rules/Collector/Runtime/Automation/AI/GameRuntime/HUD 7/7 PASS；真正 choice action 与 UI 仍未完成。
- Phase 3 594-case pending 合同已在正式 capture 前修正三个不可达/错误因子：Match Shell 改为已加载训练包的 Main、无错空训练目录改为公开路径可达的 invalid-pack error、Help 改为真实 `help_back` focus；总数仍为 HUD 360 + Shell 234，性能阈值不变，evidence 仍 `PENDING/NOT_RUN`。主 Agent 独立 pending validator 与 18/18 坏输入测试 PASS。
- Phase 3 actual GPU micro 首轮如实因 complete-scene draw calls 102--129 与 foul token 指标错误而失败；真实 UI 在保持绘制顺序、逐顶点 clip、单 atlas、straight-alpha/sRGB 语义下合并为一次 backend draw 后，6/6 micro machine PASS，complete-scene draws=100、P95 2.807--11.693 ms、纹理 58.08/75.65 MiB。主 Agent 独立核对图片 hash、报告字段并重建重跑 GPU/runtime/renderer/Core4.1 四项测试 4/4 PASS；micro 继续标记 `NON_FORMAL/formal_pass_claimed=false`。
- Phase 3 exact 594 首轮正式运行在 391/594 fail-closed：公开 ReplayBrowser 路径先触发 invalid-path error、后 focus 会按产品语义清错；改为公开 focus→invalid load 后 targeted 3/3 PASS。新增测试一度因两个临时 snapshot 的不匹配迭代器触发 Release UB/SIGSEGV，修复为持有单一 snapshot 后 Release 与 ASan/UBSan 均 PASS。第二轮从干净目录完成 actual GPU 594/594，但正式 validator 如实判为 FAIL：450 machine PASS、144 FAIL；其中 128 例有一个 truncated text run，40 个 ui135 例使用 13px-base caption（17.55 physical px，Retina 26.325）低于 14px-base 合同。P95 最大 14.208 ms、draws 最大 100、纹理最大 75.655 MiB、warmup/sample 最小 120/300 均过；整批仍是 transition，未进入 TA manual。
- Phase 3 对第二轮失败补充 exact text-run 诊断：`Any legal object ball / Any legal pocket` 在旧 ui085 clip 溢出 13.75 px；ui135 因 userScale 错误参与 breakpoint 而切到 compact 13px base。修复为 density-normalized breakpoint、严格 profile minimum×density×userScale 与真实 wide panel 后，失败轴 60/60 preflight PASS；第三次干净 formal 由官方 validator 和主 Agent独立确认为 actual GPU 594/594 machine PASS、0 truncation，PNG hash/尺寸 594/594，P95 最大 9.492 ms、draws 最大 100、纹理最大 75.655 MiB、120/300 帧。manifest SHA `f4a4233a…60b8`；manual 尚未提供，故只关闭机器门。
- Phase 3 独立 TA 随后以 33/33 contact sheets 加 6 个原尺寸疑点复核完成 594/594 manual；产物 SHA `8fe61120…d847`、失败 ID 全集 SHA `024b3354…1a330`，PASS/FAIL/N/A 轴计数为 3096/468/594，PENDING=0。官方 validator 的 `manual=594` 仅证明结构与 SHA 绑定完整；人工 overall 明确为 FAIL，根因为 Shell 标题基线碰撞、broadcast 白色灯具入镜、overhead 上桌面裁切、post-shot 空面板和 foul 横幅遮挡。旧 manifest/manual 固定为失败证据，修复后需新批次全量重拍与独立复审。
- Phase 5 决策 UI/action fail-closed 规格固定 6 个 pending capability 与 7 个 flow，覆盖开球犯规选择、开球进 8 的重置/重摆、全台及开球线后自由球放置、同时定组选择、重置/重摆进度恢复、僵局处理；每个 flow 都定义中英文文案、焦点/键鼠、取消/超时/恢复、authority ACK/idempotency/error、可访问性、token、前后条件。主 Agent 独立 validator PASS、16/16 坏输入测试 PASS并目检 flat proof；它明确 `FLAT_NOT_GPU_NOT_RUNTIME`，没有改 C++、没有晋升 capability。
- Phase 3 shipping HUD token 包新增 16 个仓库原创的 24×24 normalized rect/circle/polyline/polygon 图标、standard 与 color-vision-safe 两套 palette，并绑定五态、720p/1080p/Retina safe-area、最小字号和 debug/product 隔离契约。主 Agent 独立复核 manifest/tokens/proof SHA，validator PASS、15/15 unittest PASS，并目视检查 flat proof；该 proof 明确不是 GPU screenshot，M2 runtime 视觉门仍未关闭。
- Phase 2 v9 将袋口几何拆成 mouth plane 内侧 open-U throat/black bottom 与外侧 apron wedge，新增半空间、无 360° tube、共享端点和 body-contiguous hard assert；主 Agent 独立复核 modular/venue validator、mutation test、候选 hash 与唯一 1920×1080 closeup。几何断言虽 PASS，TA 仍明确 REJECT：袋嘴到黑底骤缩成装饰孔、库鼻直梁/棕色楔块外露、cloth→facing→liner→bottom 材料链不清、袋腔遮挡/深度不足且远端袋口有片状结构。v9 仅为 structural transition，不能跑正式六景；下一版先以 physics pocket frame/球径重签比例与截面。
- Phase 2 水平方向探针最初漏检了 V 翻转；主 Agent 在 incomplete v6-k 中发现号码上下倒置后，以蓝↑/橙↓/红←/绿→二维非对称 probe 定位到 ball-atlas shader 的单独 V 反转。移除该反转后的 front/33° 均为蓝↑顶、橙↓底、红←左、绿→右，`15` 正向可读，双轴方向门 PASS；普通 PBR 纹理与物理未改。`4958d3b8…fd60e` 源 atlas 可恢复用于下一候选，但所有 probe 与 v6-k 仍为 transition/reject，必须以新候选重新闭合哈希并通过机器 6/6。
- 三份冻结证据的 SHA-256 继续精确匹配，物理 Phase 3 候选仍为 `REJECTED (not promoted)`。

## 迁移前基线

基线 revision：`c405cb2acfa111b1af3b1a33f06d24af2c8aff01`  
基线 executable SHA-256：`64beefdbe309399fb9a0323e338a3d81a9c7d03a7da1ed7321131d24eae7b85a`

参考机器（不记录设备唯一标识）：

- Apple M3 Pro，12 核 CPU，18 核 GPU。
- 36 GB RAM。
- 内置 3024x1964 Retina 显示器；外接显示器逻辑分辨率 2560x1440 @ 60 Hz。

现有测试证据：

- `BilliardsRenderStateTests`：PASS。
- `BilliardsScreenshotTests`：PASS。
- `BilliardsArchitectureTests`：PASS。
- `BilliardsLaunchOptionsTests`：PASS。
- rendered automation E2E：PASS。
- break camera E2E：PASS。
- Phase 3 physics release gate：按治理规则确认候选为 `REJECTED (not promoted)`，生产默认未被错误替换。

物理性能基线，1000 ticks：

| 指标 | 值 |
| --- | ---: |
| mean step | 0.043462 ms |
| p95 step | 0.070292 ms |
| p99 step | 0.118292 ms |
| peak RSS | 5,865,472 bytes |
| artifact bytes/tick | 2,147 |

当前 framebuffer screenshot 是完整进程启动测量，约 1.95 秒，包含资源加载、窗口和进程开销，不能当作 GPU 帧时间。Phase 0 必须补充稳定的逐帧 CPU/GPU 测量。

## 已确认约束

- rendered screenshot 已通过独立 FBO 在 1024x768 与 1920x1080 下验证，不依赖显示器或 GLUT 窗口尺寸。
- 当前 shell 的默认 PATH 不包含 CMake，但已安装 `/opt/homebrew/bin/cmake`。验证命令需显式补充 PATH 或使用绝对路径；这不是代码失败。
- 普通临时截图继续写入 `/tmp`；受治理的 visual golden 是否提交须先完成仓库规则更新。
- 现代渲染依赖不得进入 headless simulation core。

## Phase 0 退出门状态

| 退出条件 | 状态 | 证据/缺口 |
| --- | --- | --- |
| 六个视觉场景可重复生成 | PASS | 两档分辨率、双次像素与状态 hash 一致 |
| 完整 `check.sh` 通过 | PASS | 61/61 非 rendered CTest、414 个物理治理测试、release gate 与 validation run 全部通过 |
| 1080p/60 FPS 预算与参考机器 | PASS | 16.7 ms 初始预算；Core slice 4x MSAA 单帧 CPU 7.81 ms/GPU 1.22 ms（正式 P95 待 M1） |
| Core Profile ADR | PASS | macOS 实机 OpenGL 4.1 / GLSL 4.10 Core |
| UI ADR | PASS | FreeType + SDF；ImGui 仅调试 |
| glTF/asset pipeline ADR | PASS | cgltf + stb_image；glTF 2.0 契约与原创 graybox validator |
| 生产物理与玩法语义未改变 | PASS | 当前没有物理参数变更 |

## 下一决策门

Phase 0 Go 需要同时满足：

1. 六场景能够稳定截图并产生机器可读 manifest。
2. Core Profile spike 能独立构建、运行和截图。
3. UI、glTF 与美术生产规范彼此一致。
4. 完整测试和物理 release gate 保持预期结果。
5. 1080p drawable 与逐帧计时缺口关闭，或被明确拆成进入 Phase 1 前的阻塞任务。

Go 后第一批 Phase 1 任务：

- PresentationSnapshot 只读边界和架构测试。
- RendererBackend 生命周期与 legacy 包装。
- ModernRenderer 的资源 RAII 与 shader 错误路径。
- 一张桌、16 个球、球杆、台灯的 forward PBR 垂直切片。

## 变更纪律

- 不在视觉 PR 中修改物理 profile。
- 不在大资产提交中混入 loader 或 shader 行为修改。
- 每个阶段先通过自动化门，再切换默认 feature flag。
- legacy renderer 只保留到 modern 完整覆盖，不形成永久双维护。
- 每次阶段验收都更新本文档的证据和未决风险。
