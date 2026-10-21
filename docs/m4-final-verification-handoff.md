# M4 最终自动验证交接 — 2026-09-07

## 2026-09-08 人工入口反馈：启动白屏，验收重新打开

用户运行本文的 `--product-mode` 命令后，真实窗口持续空白。PID 30611 的进程采样
（`/tmp/billiardgl-user-startup-30611.sample.txt`）显示已经进入产品渲染及 blit 循环，
不能将其归为仍在加载资源。当前首要嫌疑是 Cocoa drawable 的 context 更新生命周期。
原 ModernApplication E2E 的真实渲染使用 `showWindow=false`，没有证明真实窗口能显示菜单。
以下绿色结果只保留其实际覆盖范围，不能据此声称产品启动验收通过或仅剩人工试玩。
可见窗口启动修复与对应回归正在实施；旧交付程序暂不覆盖，人工练习暂缓。

后续白屏修复验证：`/tmp/billiardgl-pocket-pbr-tests.FmsTfY/Billiards`，SHA-256
`b2198cebd36a5a85a541852e24534e8cffee0feaa37831a37d71ed7c9f9d6a58`。
修复使用 NSOpenGLView/AppKit 生命周期及显示交换前默认 framebuffer/GL_BACK 绑定。
单独 context.update 的候选未能通过真实窗口检查，未被误判为成功。
最终版本经 root CUA 真实截图确认主菜单、鼠标点击后的12项训练列表和训练页均能显示；
ModernApplicationE2E 与新增 VisibleDrawableE2E 2/2 PASS。未修改物理或renderer内容。
训练页出现 `input_unbound` 提示，另行诊断；新版本尚未宣称完整 M4 重验通过。

状态：自动验证通过；主 Agent 与技术美术六场景图审均通过。
原计划要求的 12 项人工试玩尚未执行，因此 **M4/Goal 尚未完成验收**。
本交接不新增人数、签名、设备矩阵或跨优化级别逐位一致门槛。

## 构建与验证

工作目录：`/Users/bytedance/Projects/github/BilliardGL`。
Release 程序：`/tmp/billiardgl-v19-closure-release.UFVB27/Billiards`。
程序 SHA-256：`b6aa17f80fb8d32542a25023a22790c16af546d93b0eb986e7f20c0d0cfd892c`。
Apple clang 17，`-O3 -DNDEBUG`；正式参考回放记录同一程序配置。

- 原始 `scripts/check.sh`：退出码 0；127/127 CTests，414/414 Python 物理测试。
- 六项相关真实渲染回归：6/6 PASS，8.32 秒；覆盖 automation、满力度开球/收停、production renderer、seamless cubemap、glTF 阴影和 modern application。
- Modular 资产测试：17/17 PASS；PBR 测试：11/11 PASS；资源 foundation/importer：2/2 PASS。
- 十二个练习均有生产物理配置下的真实成功操作和绑定参考回放；双遍 Recorder 一致，最终 hash 严格校验，动作/逐 tick trace/回放文件未漂移。
- 独立物理验证：6 通过、2 已知模型偏差、0 新失败。历史 Phase 3 候选仍 REJECTED/not promoted，未改写其证据。

## 最终六场景

实际图片及进程资产加载证明：`/tmp/billiardgl-m4-final-six.PfmMvT/capture/report.json`。
PNG 预览：`/tmp/billiardgl-m4-final-six.PfmMvT`。
与此前已验证阴影版本的 actual/diff：`/tmp/billiardgl-m4-final-six.PfmMvT/diff/report.json`。
六次加载均为 `loaded`、`load_errors=[]`；完整运行状态与对应旧场景一致。

主 Agent 已逐一查看六张 actual 和六张绝对 RGB diff。差异集中在袋口、鼻口连接及相关外框：
深袋底不再呈灰色盖板，闭合腔体不再露出柜底；球与台呢呈现未出现对应区域外的整体色调漂移。
开球布局、低角度瞄准方向、球体落地感、袋口开孔、俯视六袋和击球后球局观察均保留。
这些场景图不是完整产品菜单/HUD 的人工可用性证明，后者不能由截图或脚本冒充。
技术美术独立复核六项均满足原场景用途，并记录非阻塞限制：母球仍有灯箱/IBL造成的近水平明暗分界，
袋口近景下缘有裁切；前者留作后续视觉精修，后者不影响本镜头的 mouth/jaw/cloth-seam 检查。
本轮不将这些限制扩大为新的验收轮次，也不声称它们已消失。

1920×1080 Medium，每场景 120 预热帧、300 采样帧。
下表为 CPU 渲染采样 P50/P95/P99（毫秒），不包含显示交换等待，不能直接换算为整机帧率。
前后观察保留原值，不声称小幅差异具有因果或统计显著性。

| 场景 | 更新前 P50/P95/P99 | 最终 P50/P95/P99 |
| --- | --- | --- |
| 开球 | 2.248 / 2.378 / 2.410 | 2.199 / 2.366 / 2.445 |
| 低角度瞄准 | 2.326 / 2.486 / 2.562 | 2.252 / 2.352 / 2.444 |
| 球体近景 | 2.152 / 2.252 / 2.297 | 2.128 / 2.269 / 2.367 |
| 袋口近景 | 2.250 / 2.384 / 2.496 | 2.228 / 2.410 / 2.566 |
| 战术俯视 | 2.206 / 2.422 / 2.514 | 2.165 / 2.315 / 2.405 |
| 击球后观察 | 2.240 / 2.365 / 2.510 | 2.222 / 2.374 / 2.420 |

GPU 各 pass 的独立时间查询与样本数量保存在原始报告中；不可把不同 pass 的分位数直接相加当作完整 GPU 帧分位数。

## 默认资产及回滚

默认 glTF SHA-256：`c51da380b9a528a1f0671f4617e4f6e1a22e06e6a6a5f20e13cddedaf572a158`。
Manifest SHA-256：`dc3ebc3767dd2596437044d8818167715cc5c897e2de31b7dea8281c0f4b2bb6`。
8064 个三角形，29 材质，22 纹理；PBR 纹理 16,777,188 bytes，低于 16 MiB。
包含球图集的资产纹理总量为 27,962,016 bytes，不能将 16 MiB 误称为全部纹理驻留量。

生成器默认使用审定的 physics-aligned split 几何，显式 `--legacy-unsplit-pocket-facing` 可生成旧模式。
完整旧默认目录备份：`/tmp/billiardgl-phase2-default-before-pocket-promotion.IdHt3r`。
需要精确回滚时从该备份恢复整套资产及相配的生成/校验版本，不单独混换 glTF 与 manifest；本轮未执行回滚。
渲染器和物理实现未因这轮资产修复而改变。

## 待用户完成

请按同目录 `m4-training-manual-checklist.md` 亲自操作十二项练习并反馈结果。
从仓库目录启动上述程序，加 `--product-mode`，在训练菜单进入练习；可先看参考回放再操作。
人工清单保持“未验”，不会把自动实跑、回放观看或 agent 图审填作人工通过。
详细实现和较早的诊断证据见 `m4-runtime-integration-progress.md`；其中较早失败记录为历史，不代表本次最终自动检查失败。
