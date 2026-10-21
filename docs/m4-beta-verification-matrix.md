# M4 单机产品 Beta 验证矩阵

日期：2026-08-11  
状态：ACTIVE  
范围：`docs/superpowers/specs/2026-07-27-billiardgl-visual-gameplay-optimization-plan.md` 的 Phase 0--Phase 5。Phase 6 不在本矩阵内。

本文件是发布审计索引，不替代阶段规格。`PASS` 只表示列出的证据已经由主 Agent 独立复核；任何 `BLOCKED` 或 `PARTIAL` 都会阻止 M4 完成。

## 2026-09-23 当前增量审计

2026-09-29 收敛入口：`m4-release-closeout-20260929.md` 将剩余工作归为七项，保持原规格。下方旧表中“全部政策未选择”、FullTable 旧失败和 06G 回放旧限制已有后继材料，不再单凭旧状态派生待办；仍须区分组件与产品可达性。06C 两 rerack 回执已由 v2 fresh 证据及 Root 哈希/ACK 复核闭合。

下方阶段门保留此前冻结证据，不能当作当前源码的全量验证结果。当前工作以 `docs/m4-current-audit-20260922.md` 的逐次记录为准，整体验收仍未通过。

- 用户确认本轮13个练习关卡人工验证通过；5名新手+3名有经验用户研究仍为0/8，不能互相替代。
- Release check r6曾通过133项非rendered CTest及414项Python物理检查，之后仍有HUD、像素矩阵和回放修改，须对最终冻结候选重跑。物理候选仍未晋升，两个已知物理验证偏差保留。
- 真实FullTable摆球交互在绑定窗口配置下通过；HUD/轨迹跨分辨率像素矩阵仍在修正，不继承旧截图为新版本证据。
- High回放后处理在已绑定单场景达到预算；袋口r12视觉仍未接受，最终六景与整包性能尚未闭合。
- 默认无参数启动仍使用legacy；`--product-mode`才进入modern产品入口。计划§19要求固定管线从生产入口移除，当前尚未完成；迁移须遵守§9.7的modern完整截图前置门，不通过简单改默认值掩盖未验收路径。
- 下方冻结协议源码SHA与当前值不一致：当前 `automation_protocol.cpp` 为 `80874e50076ba392028ae02b902a1e4fcce55eb7cb538bf7ce90419c74dc8427`。已检查差异涉及支撑/离台、轨迹代次及接触/复位诊断序列化，未删除旧命令；既有Release协议单测和未修改的headless客户端脚本本次均exit0。此为兼容性局部证据，不擅自重写冻结基线。其余两项冻结SHA本次仍精确匹配。
- 音频语义区分、真实听测/内容验收、完整规则产品可达性及最终无P0/P1确认仍有未完成项。

## 历史阶段门（与上述增量审计合读）

| 门 | 当前判定 | 可重复证据 | 剩余阻塞 |
| --- | --- | --- | --- |
| M0 基线冻结 | PASS | `scripts/capture_visual_baselines.py`、六场景 manifest、Core 4.1 spike、ADR、迁移前全回归 | 无 |
| M1 现代渲染竖切 | PASS | renderer selection、PresentationSnapshot 架构测试、GPU RAII、glTF importer/draw package、modern rendered E2E | 无 |
| Phase 2 生产视觉 | PARTIAL / TARGETED V5 MACHINE+TA PASS / FULL-SIX FINAL INTEGRATION WAIT | v7/v8 pre-process failures冻结；默认球架现迁移到WPA official fixed seed，旧full-six视觉/source binding不可继承。重基线门 `5c846067…270c9` 固定Phase2/4/5最小27张fresh证据 | v7/v8 auth不得重用；Phase4 successor与Rules UI v5/v3稳定后再做唯一shared build及单次full-six。六景TA/product/golden与Phase2全门未过 |
| M2 镜头/击球/HUD | PARTIAL / FORMAL V4 MACHINE+MANUAL PASS / USER STUDY NOT_RUN | v2/v3 分别固定为 TA FAIL/machine FAIL；Training+Audio 定向预检经 TA 8/8、64/64 轴通过并绑定 runner 后，v4 从空目录完成 594 actual GPU。主 Agent复验 594 unique ID/path/file、逐图 SHA mismatch=0、machine 594/594、所有零指标最大值 0；独立 TA 复审 594/594，人工轴 PASS/N/A/FAIL/PENDING=`3564/594/0/0`，默认 validator PASS。manifest `a90c9d7e…8fc6`、manual `f4db8500…1b0e`、mapping digest `0e904a7d…a2a8` | v2/v3 不得覆盖；5+3 真人研究仍 0/8 NOT_RUN，因此不能声明 M2 整体完成、真人可用性或 golden 完成 |
| M3 训练/回放 | PARTIAL / HISTORICAL FAILURES FROZEN / INSET PRODUCT GATE IN_PROGRESS | v11前3项PASS、broadcast固定3/1/7。V16/V17/V18三条bounded grammar均已由可复现CPU证据否决；V18 receipt `664d7b10…0398`、TA addendum `ec4f929a…3b310` 与五个恢复的production/CMake基线均已复核 | v11不得重跑；Root已选择独立trajectory inset/minimap，须先冻结独立semantic plane/probe denominator、world↔inset映射、布局与FBO/resolve/upload/stale-frame fail-closed合同。机器5/5、TA35/35、targeted11/formal68/M3/外部真人未执行 |
| M4 规则/AI/设置 | PARTIAL / RULES UI V5/V3 FINAL BUILD WAIT / AUDIO TECH V2 10/10 / WPA 11/9/13/7 | v5/v2 pre-ordinal failure已冻结；v3完整preclaim parity source稳定但因官方球架/product session迁移需fresh build/readiness。§6(c)/§17(a) policy-independent telemetry、§6(f)/§10.4与§8/§9窄产品子事务已闭环；15行决策登记表 `66e105e9…a31ce` 已由主Agent复验29/29且全UNSELECTED | 旧授权不得重用；fresh exact16 machine+TA、真实输入/屏读30/30、Beta0/12仍待。13行需root政策/外部裁判决策，§12.3/12.5-6/12.7/16/17 off-table需新或实质扩展physics；音频人工/权属及外部QA未闭环 |

## 不可变证据

| 文件 | 预期 SHA-256 | 发布时要求 |
| --- | --- | --- |
| `src/Billiards/automation_protocol.cpp` | `fb79a523c1a8ec4432cb012110041a4ae61c9db2f79416644555b95cec3e426e` | 精确匹配 |
| `physics_models/promotion/phase3_candidates_v5.json` | `87bfe894bb31cf2edba73c3d97f46eb3899e1c9b3d0ab2497be506989110caf4` | 精确匹配；候选仍是 rejected/not promoted |
| `tests/e2e/test_headless_automation.py` | `8cacd49515027534ed8f12acd22282f54ab31dccb2d485efbdd5fb78fcc77e70` | 精确匹配 |

## Phase 2 正式候选要求

正式候选必须同时满足，preasset/smoke 结果不能替代：

- 场景 ID 恰为 `break`、`low_aim`、`ball_closeup`、`pocket_closeup`、`tactical_overhead`、`post_shot_observe`。
- `modern`、`production_gltf=true`、1920×1080、Medium、真实 modular/venue glTF。
- manifest 必须绑定捕获时实际加载的 modular glTF、球号 atlas 与 asset manifest SHA-256；共享资产重生期间缺任一绑定的截图均不可晋级。
- `4958d3b8443ddd3f9a3b559ca1e22b6130c169d733ec73c7b918aadcd14fd60e` upright atlas 已在修正 ball-identity 专用 V 采样后通过上下/左右双轴 probe；对应源 asset manifest 为 `b92968c7ec0a970225942432353ee05084283dafcc178af87536006ff54b8629`。下一正式候选仍须重新绑定捕获时实际六类哈希；全部方向 probe、pre-mirror atlas 与 v6-k 均为 transition/reject。
- P95 总帧时间不超过 16.7 ms，draw calls 不超过 100，纹理/环境资源在 manifest 预算内。
- 六景机器门和技术美术独立人工门均为 6/6 PASS。
- 球与台呢接触稳定；袋口、库边鼻口、台呢边界和物理比例对齐。
- 常用距离可辨球号；球体旋转没有明显 seam、tile bleed 或 UV 错误。
- 球杆 tip 不侵入母球屏幕轮廓；guide 的首个可见像素从母球前缘之外开始。
- 资源失败明确报告文件/字段；不允许静默白材质或不可加载纹理警告。
- 技术美术批准 golden 后才启用 SSIM `>=0.93`；失败图或过渡图不得自批为 golden。

## 产品门最低证据

### M2

- default/aiming/post-shot/foul/help 五态在 1280×720、1920×1080、高 DPI 下生成产品截图。
- HUD 数据只来自 PresentationSnapshot；球杆、UI、物理 input 共用同一 ShotControlModel 数值。
- 不支持的 cue elevation/chalk 在 capability 未晋升时完全隐藏。
- 自动首杆代理在 30 秒脚本预算内完成且不自动瞄准/击球；这不冒充真实用户研究。

### M3

- 12 个练习可从产品入口加载、完成/失败、评分、复位并保存 Replay v1。
- Recorder 只收录成功应用的语义动作，绑定 executable/content/profile/seed/final hash。
- pause、1/4×、1/2×、1×、2×、逐 tick、跳到击球、自由/电视镜头均可由产品控制。
- 同文件重复播放最终 hash 一致；profile/content 不匹配明确拒绝。
- 预测与实际轨迹颜色、图例和数据来源明确，preview 不修改 authority hash/tick/events。

### M4

- 产品入口仅展示已通过的 WPA Heyball 规则配置；从开球可完整走到合法胜负。
- 每种已实现犯规都有 reason code、玩家可读 HUD 文本和规则测试。
- AI 仅提交公开语义 shot action；主线程 poll 不阻塞，陈旧计划不应用。
- Rookie/Club/Expert 固定集成功率区间可复算且单调。
- settings 启动加载、缺失/坏文件回退、preview/apply/cancel、原子保存、上下文重绑定和冲突回滚通过产品 E2E。
- UI scale 85%--135%、色觉模式、字幕、assist、音量和动态效果开关可达；assist 永不自动瞄准或击球。

## 最终审计命令族

最终命令和产物路径在候选冻结后写入审计结果；至少包含：

1. 全量 configure/build 与 `scripts/check.sh`。
2. 所有非 rendered CTest、physics governance/release gate。
3. headless、legacy compatibility、modern rendered automation E2E。
4. Phase 2/3/4/5 全部资产 validator 与坏输入测试。
5. 六景和产品 HUD capture、性能报告、独立人工 review、golden SSIM。
6. 训练/replay/完整规则对局/AI/settings 产品 E2E。
7. `git diff --check`、三份冻结 SHA、无 P0/P1 崩溃/数据损坏/确定性问题确认。
