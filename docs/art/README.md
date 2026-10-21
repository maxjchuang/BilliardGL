# BilliardGL 技术美术交付索引

状态：Phase 0 规范已建立；M1 graybox 与 Phase 2 首批程序化生产候选已交付离线验收
日期：2026-07-27

本目录把视觉优化总计划中的美术要求落实为可审计、可生产、可验收的资产规则。Phase 0 文档不修改运行时代码；后续资产候选保持原创、可程序重建，并通过独立离线门禁。

## 文档

- [现有资产审计与迁移矩阵](phase0-legacy-asset-audit.md)：OBJ、MTL、BMP、房间实现、当前截图与授权风险。
- [原创视觉目标与验收标准](visual-target-and-acceptance.md)：球、台呢、木材、金属、袋口、球杆、灯光、场馆和四类镜头。
- [glTF 2.0 资产生产规范](gltf-2.0-asset-production-spec.md)：单位、坐标、命名、UV、切线、材质槽、LOD、纹理通道、预算和交付门。
- [球台可视 / 物理对齐签字契约](table-visual-physics-alignment-contract.md)：权威尺寸、六袋 frame、禁止遮挡区、socket、容差和待签字决策。
- [M1 Lookdev 美术验收清单](m1-lookdev-acceptance-checklist.md)：机器参数、graybox 准入、材质灯光、镜头和 LDR ROI 验收门。
- [M1 程序化 Graybox](../../assets/modern/graybox/README.md)：原创、自包含、确定性生成的 glTF 竖切资产及离线验收命令。
- [Phase 2 模块化生产候选](../../assets/modern/phase2/README.md)：球台、共享球、原创身份图集、球杆和矩形灯的 glTF LOD、loader manifest、预算与离线门禁。
- [Phase 3 产品 HUD 规格](../../assets/modern/hud/README.md)：双分辨率五态 HUD manifest、四组 SVG mockup、可访问性与离线门禁。
- [Phase 3 HUD 实现验收清单](phase3-hud-implementation-acceptance.md)：数据权威、safe area、DPI、键鼠、30 秒首杆、调试隔离和 capability gating。
- [Phase 3 Shipping HUD Token 技术美术验收](phase3-hud-shipping-token-acceptance.md)：GPU 可展开原创图标、双 palette、SDF 字体层级、flat proof 边界与资产 SHA。
- [Phase 3 ProductShell Shipping Token 技术美术验收](phase3-shell-shipping-token-acceptance.md)：九屏 43 个实际 shapeToken、strict source audit、双 palette、坏输入与独立资产 SHA。
- [Phase 3 GPU HUD / ProductShell Capture 验收契约](phase3-gpu-capture-acceptance.md)：594-case PENDING 矩阵、采集前 production-route/public-controller 可达性纠错、实际 GPU/SHA 机器门、独立人工模板与 flat-proof/user-study 证据边界。
- [Phase 3 真实用户研究协议](phase3-user-study-protocol.md)：5 名项目新用户＋3 名真实台球经验用户、四任务、匿名双签、PII 禁止、SHA 绑定与 NOT_RUN/PASS/FAIL 门。
- [Phase 4 首发训练内容包](phase4-training-content-acceptance.md)：12 个原创练习、能力门控、确定性评分、坏输入与工程接入验收。
- [Phase 4 首发 12 练习人工可玩验收](phase4-manual-play-protocol.md)：真实 manifest ID、Replay v1 双重重放、轨迹/authority、外部真人 QA 与 PENDING 证据边界。
- [Phase 5 / M4 产品 Beta 人工验收](phase5-beta-qa-protocol.md)：真实整局、WPA reason code、AI 三档、设置/输入、四项音频退化披露、实际 GPU capture 与默认模式授权。
- [Phase 5 WPA Heyball 规则反馈与中英文案](phase5-rules-copy-acceptance.md)：8 个现有 reason code、pending UI 隔离、HUD/modal 优先级、可访问性和非 GPU flat proof。
- [Phase 5 / M4 规则决策 UI 与 Action 实现规格](phase5-rules-decision-ui-actions-acceptance.md)：6 项 pending capability、7 条 exact state/action 流、权威 ACK、fail-closed、未决政策和非 GPU flat proof。
- [Phase 5 / M4 产品 UX 验收包](phase5-product-ux-acceptance.md)：规则双语 HUD、AI 档位、键鼠重绑定、可访问性和响应式样机。
- [Phase 2 / M2 Production Modern 独立审查](phase2-production-modern-visual-review.md)：六景 actual matrix、固定阈值实测、FAIL 证据与复验顺序。
- [Phase 2 正式人工复审模板](phase2-production-modern-manual-review-template.json)：仅供 machine 6/6 候选复制并绑定截图哈希；模板保持 `PENDING`，不能作为批准记录。
- [Phase 2 技术美术资产修复包](phase2-ta-asset-repair-v2.md)：袋口拓扑、球号 atlas、felt/walnut 修复、SHA、预算与离线门。
- [Phase 2 首批资产 Backlog](phase2-core-asset-backlog.md)：依赖顺序、优先级、交付物和逐项验收条件。

## 决策摘要

1. 视觉方向采用原创的“专注训练厅”：青蓝台呢、深胡桃木、枪灰金属、暖中性台灯、低曝光炭灰场馆。竞品只用于观察真实材质和可读性，不复制品牌、造型、商标、纹理、界面或场馆布局。
2. 新资产以真实尺度制作并交付 glTF 2.0。glTF 内使用米，导入 presentation world 时统一乘 `100.0` 转为现有厘米单位；simulation core 不读取渲染网格。
3. 新球台从物理尺寸契约反推可见几何。旧 `table.obj` 仅作迁移期对照，不能继续作为新资产拓扑基础。
4. 旧 OBJ/纹理缺少授权与来源元数据，且部分命名暗示第三方模型库。获得书面来源和发布权之前，只能视为 legacy bridge，不能认定为可发布生产资产。
5. Phase 2 的首要竖切是球体、台呢、库边、袋口、球杆与矩形台灯；远景家具不进入首批关键路径。

## 批准门

下列内容全部关闭后，技术美术工作包才可从 Phase 0 进入正式生产：

- 产品批准视觉方向与四类镜头构图。
- 图形开发确认 glTF 坐标、PBR 通道、clearcoat 支持方式和 LOD 选择策略。
- 物理负责人完成[球台可视 / 物理对齐签字契约](table-visual-physics-alignment-contract.md)中的台高、鼻高、袋口 profile 与容差决定。
- 项目负责人确定所有 legacy 资产的授权结论；无法举证的资产进入替换清单。
- 预算在 shader 球体、纹理平面和单个 glTF 资产 spike 上得到验证。
