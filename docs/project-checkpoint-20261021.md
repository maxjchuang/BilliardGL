# BilliardGL 维护检查点 — 2026-10-21

此文是空间整理与整批提交的恢复入口，覆盖旧交接文件中“run15 正在运行”等过期进程状态。M4 未完成；R1–R7 均开放，Phase 6 未启动。维护开始时三个子代理均已停止编辑/编译/运行，未启动游戏窗口。

## 当前成果与问题

| 项目 | 已有证据 | 尚缺验收 |
| --- | --- | --- |
| R1 袋口 | 厂商/协会照片像素对比、候选整桌几何与三 LOD/PCK 对应、部分真实 GPU 近景；v132 局部支撑/旋转停止/离面根窗证据 | Face→凸 Edge 连续运动、全步提交、真实 Controller 整杆与六袋中央/偏口/未进路径；同版本六袋三视角、产品相机与六景 |
| 性能 | 原始失败计时和采样保留；完成逐段严格证明热路径的源码诊断 | 实时数值生产路径；含规则、守卫、提交、完整日志的平均物理规则 ≤2ms/60Hz 帧，1080p Medium 完整帧 P95≤16.7ms |
| R2 正式入口 | 独立现代入口预演已构建/无窗口验证 | 正式 Billiards 入口切换及完整交互验收 |
| R3 规则 | 已有裁判/回放/重新开球与若干规则组件 | 普通产品可信裁判输入、首次非物理犯规具体原因、完整对局结算 |
| R4 音频 | v3 合成候选技术复审、用户选择和逐文件来源包 | 具名权利人/许可未定、剩余听感及发行材料；用户原答“还定”不作为批准 |
| R5 回归 | 大量局部 CPU/GPU 证据与失败记录 | 统一冻结版本的全量回归，不能拼接候选 PASS |
| R6 真人研究 | 13 课人工通过有效，用户可安排参与者 | 5 名新用户＋3 名真实台球经验用户，仍 0/8；版本尚未就绪 |
| R7 发行 | 构建/链接/材料的若干局部证据 | 发行包及无未决 P0/P1 证明 |

袋口物理不能宣称已修好。冻结 131 的 P0 真实击球接受 50 tick 后，第 51 tick 拒绝；P4 局部收集不能替代全 16 球自然止球与规则结算。v132 run16 私有诊断仍拒绝 `hybrid132_actual_face_owner_exit_physical_transition_unproved`，完整 physical/aux 未变，没有 h51 发布。真实离面模型窗口宽约 6.12046e−11s，保持原 1e−10s 精度；这仅是局部组件。

最新局部 Edge 参考力调用成功，但只在旧 Face 虚拟参考点求值：N≈1.133739265N、normal-spin 模式 Static，未证明整个误差盒/两域切换，不是物理过渡通过。首次大诊断输入触发 parser 大小上限的失败与后继小 fixture 均保留。

v132 run16 wall≈96.86s，不能用于实时产品。原计划要求 strict GMP/ODE 留离线研究。下一产品候选应独立建立数值 factory/profile/formula，复用实际力学、有限域/CCD、事务与日志，用严格工具离线核验；不得直接关闭旧守卫或给旧 profile 换算法。该生产候选尚未实现。

## 精确恢复入口

1. 总计划：`docs/m4-verifiable-implementation-plan-20261016.md`，读取后继检查点73及本文，不机械执行靠前旧恢复状态。
2. 执行台账：`docs/evidence/20261016-m4-execution/README.md`。
3. 物理目录：`docs/evidence/20261021-physics-P0-curved-stick-successor-v132/`。
   - `FACE_TO_EDGE_REUSE_AND_REALTIME_BOUNDARY_V1.json`
   - `producer-build-v2/RUN16_ACTUAL_COMPONENT_SCOPE_SUMMARY.json`
   - `producer-build-v2/run16-owned-source-snapshot/MANIFEST.json`
   - `local-face-edge-reference-field-v1/actual-reference-field-process-02.json`
4. 实时路径评估：`docs/evidence/20261021-real-time-product-performance-boundary-readonly-v132/PM_DECISION_BRIEF.json`。
5. 原生静态三分辨率：`docs/evidence/20261021-root-native-formal-orthographic-v5a-v1/ROOT_NATIVE_STATIC_THREE_RES_SCOPED_SEAL.json`；暗部 ROI、720 球径、SSIM 等失败仍有效。
6. 外部资产/美术证据：`/Users/bytedance/Projects/github/BilliardGL-evidence/m4/` 原样保留。下一同版本执行卡位于 `20261021-R1-pocket-next-same-freeze-verification-card-TA-v1/`。

保护程序 `build-product-session/Billiards` SHA256：`44374e01d2b4eecadef773aa8775324047f4ceda42822cbbf4004aa50a6169eb`。冻结 131、旧回放/训练成绩、13 课人工结论不改。主构建和 GPU 仍一次仅一人持有。

## 下一执行顺序

1. 先阅读维护结果与恢复包说明，核保护程序/源/输入/实际依赖。
2. 分离实时数值生产候选与严格离线 oracle；完成 Face→Edge 的真实模式、事件和误差检查，保 Unknown/失败原子恢复。
3. 真正生产 Runtime 从原 Controller 输入 h1 跑过 h51，到自然止球或真实收集，再同版本 P4；新 profile/factory、回放/训练/规则兼容必须显式绑定。
4. 原六袋中央/偏口/失败矩阵、完整逐球日志与性能；同冻结版本再跑六袋三视角/产品相机/完整六景。
5. 收敛 R2–R7，形成可测试冻结版后通知用户安排 8 人研究。全部证据对应同一冻结版本且无未决 P0/P1 才标 M4 完成。

## 提交与空间整理范围

提交主源码、测试、脚本、资源、物理模型、计划/设计文档，以及研究源码恢复包。大型原始运行日志、截图、二进制与外部资产不强塞 Git；它们在原本地路径继续保留。远端源码备份不等于完整原始证据异地备份。

空间整理优先采用 macOS 文件系统透明无损压缩：保持文件路径和可读字节，逐文件验证 SHA，保留原失败。Git 对象进行无损打包并保留不可达对象。被封存引用的构建物没有已证明可整目录删除的范围，继续保留；不运行 `git clean/reset`，不删除 `.worktrees`，不覆盖保护程序。

维护后的实际空间、压缩清单、远端提交与验证结果见 `docs/recovery/20261021-maintenance/` 以及 `docs/recovery/README.md`。
