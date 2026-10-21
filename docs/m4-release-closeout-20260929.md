# M4 发布收敛清单（2026-09-29）

本清单整理原实施计划 Phase 0–5 的剩余工作，不替代规格、不降低验收要求、不启动 Phase 6。细节归入下列七项，不把每个测试修正扩展成新的里程碑。状态仍为未完成。

## 2026-10-12 恢复检查

Root 已检查当前磁盘和进程：没有遗留构建或 GPU 任务，`git diff --check` 通过。以下是已经绑定的后继证据；它们不代表修改后的工作树已完成新的全量验收。

- `evidence/20260930-inset-product-native-v19/result.json`：绑定二进制 `55ec83cc22bc1b3168efaa574d24ee96d7468fcad1a8939c747d799fe38f27b4` 的普通 Training/Replay 原生链路 exit0，stage0–7 完整，关闭/重开、resize、seek/重建通过；定向 CTest 4/4。此前“普通入口未接线”的定位已被这条实现和证据取代。
- 新 EventVisitMarker 的 1/2/T 语义与同轨 actual 终端 core/capsule 顺序修正已有 CPU 正负证据及正式 CTest 注册；普通 v19 短路线没有触发 marker fallback，因此其原生字形/最终像素证据仍缺。
- v19 原生日志保留 `UnsignedInteger` sampler unit0 unloadable 警告，尚需定位具体 program/texture 绑定及实际语义回读，不能由色图出现轨迹推断无影响。
- 袋口 r25-exactcut 几何与 r26 局部间接光仅作为候选，默认资产及补光默认值未推广。近角内壁层次改善、关闭/恢复/灯灭/深底守门通过；六袋可见层次、正式六景及当前构建性能仍需闭合。r26 旧 metrics 相机字段已标为过期模板，正式捕获须记录运行时相机。

恢复后的两名 C++/图形开发分别负责 sampler/真实 GPU 闭合与 marker fixture 接入；技术美术负责六袋实际图审和运行时 capture 绑定。主构建/GPU 单人持有、明确交接。

| 编号 | 工作与完成条件 | 当前状态 / 负责人 |
| --- | --- | --- |
| R1 | 修复袋口造型和轨迹显示；最终六景、产品 HUD/轨迹在规定分辨率和 DPI 下满足规格 | 普通 inset 链路已有 v19 原生通过；剩余 marker GPU、sampler 警告、六袋/六景及完整截图门 |
| R2 | modern 完整截图覆盖后移除生产入口的固定管线路径，默认无参数启动进入正式产品 | 未完成；依赖 R1 的 modern 覆盖，不仅修改默认标志 |
| R3 | 完整版本化规则、可解释犯规、开球到胜负的产品可达性 | 06G 默认 capability 仍关闭；可信裁判输入及现实行为适用边界、重复违规终局尚待闭合。已完成的组件/回放不重复实现 |
| R4 | 音频语义区分、独立环境声、强度映射/防爆音和来源权属 | shipping 尚共用 hit.wav、Music/Ambience 共用 background.wav；候选技术通过不等于听测/内容批准 |
| R5 | 最终冻结候选的物理、确定性、自动化、规则/AI/settings/replay、截图和性能统一回归 | 历史通过不覆盖后续修改；在候选冻结后执行全量，避免每个小改重开整个矩阵 |
| R6 | 原计划 §9.6 的 5 名新手及 3 名台球经验用户研究 | 外部人工依赖，0/8 NOT_RUN；已通过的 13 课练习不要求重复，不冒充该研究 |
| R7 | 无 P0/P1 崩溃、数据损坏、确定性问题的综合确认，交付一致的资产/配置/可执行文件与回滚说明 | 待 R1–R6 及最终发布复核；不以测试数替代整体完成 |

## 已闭合范围

### 2026-10-12 后继证据与当前动作

本节取代恢复检查表中仍未更新的 marker/sampler 状态；旧定位及失败过程仅保留历史，不重复派工。

- `evidence/20261012-inset-marker-native-v6/result.json` 补齐记录源 fixture 的真实 GPU 证据：四组合 native exit0，六枚 1/2/T 字形的90个固定正负格核对归属和最终颜色，整个 plot 的411016像素等于 CP3 lane0。不是普通短路线触发 marker 的证明，也不证明全部 MSAA lanes 都是 core；该局部缺口不再作为当前 R1 阻塞。
- `evidence/20261012-inset-sampler-integer-nearest/result.json` 定位整数 RGBA32UI texture 错用 GL_LINEAR，修为 GL_NEAREST；绑定构建普通 stage0–7 的原生日志没有该警告，最终语义复制检查有局部后继证据。不外推当前全工作树验收。
- 仓库同级证据目录 `/Users/bytedance/Projects/github/BilliardGL-evidence/m4/20261012-draw-batch-r1/result.json`：六景 draw 从111/102/75/81/105/111降至96/87/72/76/90/96，六张完整 PPM 与原图逐字节一致。仅关闭绘制次数超预算。
- 同级 `20261012-shadow-compare-bilinear-a/result.json` 绑定 A-only 修正：维持2048/12 taps/bias/geometry，对比较结果双线性滤波；六景 draw 最大96、P95最大6.578ms，技术美术局部接受柔边和贴合暗核。高光 B 与对象采样/俯视构图仍待冻结合流，不能外推最终 R1。
- Ruling: 保留原 fixed-ROI 门及失败历史，另建显式对象 photometry 诊断契约，沿用原 felt/cue/venue 数值阈值，不用其 PASS 替换原验收失败。旧区域混入墙、库边或台呢；若映射错误会误判，必须重检对象绑定及照明，不能放宽阈值。
- 用户选择更真实台球声，并明确批准五类接触音 bounded 设计：只改 cue/ball-ball/rail/jaw/drop，使用短非谐波接触与噪声激励，保留 v2 和其他语义，新增 opt-in v3 与实际 SDL 新旧试听；不下载不明权属素材、不冒称实录。这是制作候选的批准，不是最终内容/来源或发布批准。shipping 旧 alias 不在此时替换。
- 当前下一动作：A/B、mask 与 tactical pose 冻结后一次正式六景合流捕获；R2仅已准备现代入口代码，生产目标未切换；R5最终全量仍待冻结版本。R6人工研究仍0/8，不重复用户已确认的13课练习验收。
- Root 后继实图复核：直接查看 A 的 reference/candidate `ball_closeup.png`，球下阶梯边缘减少、接触暗核仍贴合，接受这一局部可见改善；直接查看 B `candidate-v2/ball_closeup.png`，四球矩形灯形可辨、球色/号码保留，但单帧不足以排除固定贴片。B 仍等待实际 camera/ball 变化证据，不以静帧关闭运动反射要求。主构建/GPU 尚由开发持有，冻结后单次移交技术美术，不并行构建。
- B 后继 `20261012-ball-rect-specular-b/result.json` 已提供五帧 bounded GPU 证据：camera eye.x ±6cm 与 ball8.x ±1cm 时高光 centroid 随之变化，面积2226–2300像素；排除该有限范围的固定屏幕贴片，不代表全部运动连续性验收。Root 核对 renderer cpp/h SHA 与冻结记录吻合，并用 `cmp` 复核正常 base 图在 mask 前后及与 Bv2 原特写逐字节相同。2026-10-12 开发确认无构建/GPU在途后已明确移交技术美术，执行获批构图/对象采样合流六景；开发转 R2 只读准备，暂不改生产 CMake/default。
- R2 Root 独立复验（当前源，非主构建）：以 `/usr/bin/c++ -std=c++17 -Isrc/Billiards tests/launch_options_tests.cpp src/Billiards/launch_options.cpp` 编译到新临时目录并执行，exit0，输出 `compatibility=PASS production_routes=PASS shared_runtime_default_unchanged=PASS`；`product_main.cpp` 用相同标准及实际 `BILLIARDGL_SOURCE_ROOT` 做 `-fsyntax-only`，exit0。仅证明 parser/语法，不证明链接、ready backend 或默认窗口。CMake 当前生产仍编译 billiards.cpp/legacy renderer，退休目标未完成。
- R2 测试迁移边界：RenderedAutomationE2E 默认 legacy 像素合同、VisualBaselinesE2E 非正式 legacy 元数据、BreakCameraE2E 旧 angle_x/y 输入合同保留在明确 LegacyCompatibility 目标；headless/renderer-none、ModernRendererProductionE2E 保留生产绑定。四独立截图场景需新版实测，不直接复用旧 golden。最终还需生产 symbol/link graph 审计、无参数真实窗口和 missing-assets fail-closed，不能只改默认标志。
- 对象采样 validator Root 独立复验：阅读 `tests/visual/test_phase2_object_photometry.py` 后分别执行 `python3 -m unittest discover -s tests/visual -p 'test_phase2_*photometry.py' -v` 与 `-p 'test_phase2_production_visuals.py' -v`，6/6和17/17均exit0。覆盖错误母球class、空必需对象、缺venue、camera/binary错绑、缺席N/A不是PASS，以及阴影暗像素不能从felt统计删除。只关闭采样工具局部误判风险，真实六景明暗尚待捕获测量。
- 音频 v3 Root fresh复验：将 `BILLIARDGL_AUDIO_COMPARE_BIN` 显式绑定独立音频 build 的 `BilliardsProductAudioCandidateTests`，运行 `test_audio_contact_candidate_v3.py`，3/3 exit0（8.677s），包括实际 SDL 十段 queued contact 输出，不跳过 SDL 项。保存的旧→新五对试听 `evidence/20261012-audio-v3-contact-review/old-v2-then-new-v3-five-contacts.wav` 经 `afinfo` 确认48kHz/stereo/PCM16、4.48s。已呈现用户并询问听感；仍 NOT_CONTENT_ACCEPTED、无shipping替换。独立 peer 复审由另一开发负责，不新增团队人数。
- R3 当前只读报告 `evidence/20261012-r3-current-product-gap-review.md` 区分后继已实现与真实缺口：06G V4回放、06C/FullTable后继不重做；可信现实裁判普通入口/政策仍缺。另定位首次 nonphysical commit 没有 `reasons/textKeys`，presentation给出空reasonCodes、HUD回退“No current foul.”；这是待复现/修复的解释链缺口，不是许可开放可信输入。Root已读 runtime→presentation→HUD源链；下一切片须保持 authority结果、first/repeat区别和回放兼容，不用修改泛用HUD回退掩盖缺失原因。
- 唯一合流捕获 `BilliardGL-evidence/m4/20261012-integrated-r1-ab-tactical-object-photometry`（仓库同级）已 native exit0。Root看过全台正常图：六袋及16球可见，接受局部构图；核对 Smoke/core SHA 与冻结记录吻合。Root先尝试运行拷贝的 validator，因其相对 repo ROOT路径失效而退出（不算产品失败）；确认当前 repo validator 与 frozen copy `cmp` 一致后执行当前脚本，写独立 `root-machine-review.json`，exit1：原 machine门5/6失败，人工六景未输入、SSIM尚无批准baseline；性能门PASS，draw96/P954.331458ms/28.0143MiB。对象felt六景均.08878–.09647<.12，是真实偏暗；cue0四个实际可见场景均PASS、两个实际缺席N/A；venue俯视.00237/袋口.00320<.005，EV5.32/4.79超上限3。下一步同曝光、同六景的 authored lighting 根因/校准，不放宽门、不按scene独调，不宣布R1。
- 音频 I1/M1局部修复后 Root再独立运行5项Python（含actual SDL及覆盖/子树负例）exit0，原3个Audio CPU/SDL CTest exit0，再单独 ModernProductAudioSessionE2E exit0。独立peer fix-only复审已补到原报告，确认fresh-only exclusive写入和冻结子树guard；原试听WAV不覆写。该项仅关闭资产/证据覆盖风险和候选技术验证，不代替用户听感/权属及默认内容验收。
- 照明只读根因与一组候选：`evidence/20261012-r1-authored-light-calibration-proposal.json` 定位 floor 名称路由复用 rubber贴图，有效baseY约.00433，比 plain wall低4.75倍；不是AO或shadow。Root已派技术美术按原批准视觉目标制作新的候选root：固定EV7.2/.8与同六镜头，keyFlux8300/reference6200、独立matte-ground因子[.028,.032,.036]/rough.92/metal0，无共享纹理更改、不默认晋升。只是一次待实测艺术参数校准，不宣称预测PASS。CCT/WB实际消费缺口由另一开发只读拟最小接线，未改shader。
- R1完整剩余范围不能缩成亮度：本次manual review还拒绝 lowaim约28%球径（目标8–14%），break/post背景空盒感。Root实际检查 venue节点，只有wall/acoustic/lamp/trim，缺原视觉设计§4.2的少量长椅/置物架轮廓；floor变亮不等于场馆完成。还需运动高光/UV/LOD及实际水平落袋动态证据，旧5态shader响应和垂直袋心下降只证明各自局部，不替代普通运动验收。当前calibration不混这些后续改动。
- Root fresh venue suite：14项13PASS/1FAIL，未把生成器生成y.78与已存默认y.38的漂移改成green；新matte合同正例及默认改名/错误因子/暗加贴图负例通过，默认合同保持。候选恢复的同SHA四镜头预览仅 inherited_reference_only，不能代替当前GPU四镜头证据。旧venue禁止bench/chair的收缩合同与原目标少量物件存在冲突，后续须显式版本化裁决，不能改名绕过或偷偷删门。
- R2非shipping预演已建 Apple `EXCLUDE_FROM_ALL` 的 `BilliardsModernProductionCandidate`，不更换正式入口。独立目录编译/有限CPU/CLI及link审计见 `evidence/20261012-r2-modern-entry-rehearsal/result.json`；Root fresh执行 headless EOF得到exit0/ready backend none，显式legacy得到正确exit2，核对CMake/binary SHA匹配冻结记录，`otool`无GLU/GLUT/GLEW链接且精确fixed函数 `nm`探测无匹配。全局配置仍为历史目标发现GLUT/GLEW，默认窗口/截图/正式source和link迁移仍未验证；该临时目标不是最终产品退休的替代品，独立peer尚待结论。
- Root实际看过8300/独立ground候选的正常俯视与球体特写：背景floor不再黑面，球色/矩形key/软影保留，接受局部改善。对象报告felt六景.12483–.13523通过；仍拒breakEV1.907、pocketEV1.919与aim cue对比2.474（下限2.5）。旧门/人工/SSIM未因此关闭。后续镜头/场馆与真实CCT/WB会改变全景结果，不先对这几个边界值无依据连搜参数。
- 普通产品接入仍须核对：H500/FOV42是Smoke候选，实际Overhead preset目前distance380/FOV40并clamp至380；Smoke全台不能证明普通产品HUD/BIH/inset下完整构图。实际PlayerLow为145cm/20deg/42FOV，与Smoke低瞄也不同；后续必须验证/修正真实产品路径，保留用户13课验收，不把fixture构图替代产品完成。

R1/M3 主线纠正：V19 已选独立轨迹 inset，主世界叠加是次要诊断。Root 已复核 CP4-D addendum 明确仅 CPU/static 接受、GPU/product exposure=false；当前 `setTrajectoryInsetVisibleCurrentContext` 无产品调用，正常 Training/Replay 无可用 toggle，故这是实际未接线而非只缺验收。两名开发现分别实施 session/shell/context 与 app/renderer 当帧调用，复用 CP1–4；不把 world overlay 的继续完美化替代正式 inset 产品完成。新 inset 的独立分母/渲染语义不能借用旧 world PASS。

接线所需的原型限制修正：CP2 原先仅精确接受 1280×720/1920×1080 × UI100/135 四组合，coordinator 又使用固定示例 exclusions。现批准增加实际 viewport/UI85–135 解析并消费同帧真实 ProductUiFrame 面板区域；保留四个既有 profile 的精确几何、2:1 plot、文字与安全区合同，不通过夹紧缩放/假冒 profile 或关闭功能绕过产品要求。

原 V4 world 可见性合同是 source probe 的 compound glyph（各轨及整体≥0.90、局部对比≥4、4.5覆盖≥0.99），不是每枚 tick 100% 完整露出。后加逐 tick 完整翼 geometry 保留为诊断和错误尺寸/断裂回归，不擅自冒称原发布要求；所有隐藏 probe 仍留分母，绝不借另一轨 endpoint 充数。当前 world compound 也未达标，保留真实失败，不宣布通过。

- 用户确认 13 个练习关卡人工通过。
- 06C 两种重新开球及原始成功回执：`evidence/m4-visible-06c-rerack-20260923-v2/`。Root 复核 12/12 SHA、两个真实 ACK、revision 20→21、actor/breaker 和 16 球静止状态。
- Group/FullTable 已有后继原生通过证据；06G V4 已有回放组件证据。历史覆盖表的对应失败不能继续当作当前事实，亦不能从组件通过外推整个 R3 完成。

## 本轮可复验基线

`evidence/20260929-trajectory-baseline-v1/physical-1280x720/` 保存本轮七状态真实捕获及 normal/no-glyph/restored 图。Root 核对原生进程 exit0、捕获前后二进制 SHA 一致（`df71de1b8ea9603f093ce58e5d9d2c3827838aaaa7aeaca8706cc1ee18b44d53`），文字配对检查通过；Training 预测线 40/45、Replay 刻度中心 2/3 通过旧像素预检，故整体仍拒绝。中心预检不能单独代表整枚刻度，需要与同帧几何和实图合读。此目录替代已空的临时 r23 目录作为当前修复起点，未宣称 R1 完成。

## 需要外部确认的边界

- R3 的 06G 不是只缺两选项 UI：现有原生产品未配置可信声明 authenticator，没有 proposal/final declaration 的普通产品接入；输入又限制在可靠击点半径，组件直接 miscue 输入不能证明玩家可达。已向用户询问“保持可靠击点并披露适用边界”或“开放滑杆及可信判定”，尚未获得选择。不自动打开 capability，也不把 fixture 后半链测试替代产品入口完成。
- R6 已询问能否安排 8 名参与者，尚未得到安排；保持 0/8，不重复已通过的训练关卡试玩。
- 这些外部依赖不阻止 R1 的轨迹/袋口工作，也不表示整体已满足 blocked 条件。

## 执行约束

本轮局部进度：普通输入日志的规则提交信息已在中央 logger 实现并通过组件测试和 Root 独立复验，等待统一构建进入产品；不据此关闭整个 R3/R7。Training 底部 HUD 裁切候选未改变失败像素，已撤回，仅保留失败证据，继续定位实际遮挡来源。

后继定位：`20260929-trajectory-overlap-diagnostic-v2` 对全 208 点关联到实际教学 world 三角形（目标环/箭头），开发报告 unmatched=0；Root 已读取实际 contributor 明细并看过基线 Training 图。不能继续将这项称为菜单/HUD 穿透。正在分离交互 chrome 专项检查与总后续叠加覆盖；教学叠加仍须影响轨迹最终像素归属和对比度，不作为放宽轨迹可读性的理由。

分类误报已由 `20260929-trajectory-chrome-classification-v1` 原生捕获闭合：Root 核对 Training 交互 chrome 交叠为 0、文字配对仍通过，且 `cmp` 确认 Training 与原基线图逐字节相同。原 total-overlay 归属仍保留；Training/Replay 均仍报 body_width_mismatch，故 R1 仍未完成。下一步集中解决剩余真实可读性问题，不再扩展这一分类检查。

Replay 刻度长度候选：`20260929-trajectory-tick-wings-v1`，Root 已对照前后真实图，中间刻度上下白翼从几乎被覆盖变为可见。保留这一局部生产改善并冻结长度；三枚刻度的 geometry shape 仍未通过，后续以现有 MSAA owner 数据辨别遮挡/身份片段/测量问题，不继续盲目增长刻度，不称完整 R1 通过。

袋口 `20260929-pocket-apron-collar-r19-clearance`：净空修正后各 LOD 连续边距离满足 29.575mm 要求；Root 实看新图木桥消失，但仍呈长折面滑槽和外接托盘，拒绝推广。下一步回到 cloth/上口/台体的 84.84mm 高差结构，不继续用附加深槽遮挡堆叠外形。

1. 先修已复现的产品缺陷；验收工具仅修会造成误判或漏判的必要缺陷，不新增与规格无关的门槛。
2. 任何候选变更明确文件范围、对应 R 编号、期望可见结果和失败判据；不靠删除失败采样、缩减分母或放宽阈值取得通过。
3. 构建/GPU 单一负责人，交接明确；证据存持久目录，记录实际构建与资产绑定。
4. 外部人员、权属或政策选择单独报告，不用自动化伪造；不以外部依赖为理由停止仍可执行的产品修复。
5. 阶段汇报描述关闭了哪个 R 项或哪项产品缺陷，不用新增测试数量表示里程碑推进。

原始要求见 `superpowers/specs/2026-07-27-billiardgl-visual-gameplay-optimization-plan.md` §8.7、§9.6–9.7、§10.4、§11、§13、§16、§19；详细历史证据仍保留于 `m4-current-audit-20260922.md` 和 `m4-beta-verification-matrix.md`。


### 2026-10-15 后继：袋口、真实窗口与完整物理步（仍未达到 M4）

本条优先于前部旧“恢复检查”；不重开已闭合的路标、整数采样器或用户已通过的13课。

- R1 袋口：实物像素复核否定了完整木角内嵌小皮圈的旧候选。新原创包边让两段木库真实结束于皮革两翼，selected-v6 曲线的 v7 实体已取得 Root 独立材料相交、拓扑和三个原生静态角度检查。仅一角局部；完整球桌下柜连接、中袋独立直线包覆、六袋运动/物理开口和六景视觉仍未验收。见 [后继检查记录](evidence/20261014-m4-root-checkpoint-v1.md) 与 [独立局部实体检查](evidence/20261015-root-selected-v6-material-partition-review-v1/independent-topology-run.json)。
- R1/R2 原生：独立 v34 预演实际164TU fresh构建/链接成功。同一二进制经真实鼠标击球准备源，切换相机后 F9 推进 tick1，再切换相机后推进 tick2，第三次拒绝且状态保持，正常退出与日志闭合。只验证 cloth partial；普通完整击球仍拒绝。正式 Billiards 入口未切换，验证提示未出现在真实 HUD，Enter/方向键实际绑定差异保留。见 [真实原生审计](evidence/20261015-root-owned-cloth-camera-native-review-v34-nativefix-v4/interactive-bundle-04/root-independent-actual-native-review.json)。
- R1 完整物理：v36 最近一次真实 attempt351 已处理14次球球事件，后因贴合球链需持续接触力岛/约束ODE而失败；最后完整350及全部球状态保持。不能以事件证书或两次原生单步替代完整击球和六袋验证。性能候选两步均值11.03ms仍不满足原2ms/显示帧预算，首次准备166ms也保留为失败。
- R3–R7 沿后继证据仍开放：规则产品闭环、音频内容批准、冻结版全量回归、真人研究0/8、发行包和无P0/P1证明均未完成。13课通过不替代研究；Phase6未启动。
