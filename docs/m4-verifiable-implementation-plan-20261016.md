# M4 单机 Beta 可验证实施计划（2026-10-16）

本计划承接《2026-07-27 BilliardGL 视觉与玩法优化分阶段实施计划》Phase 0–5，作为 R1–R7 的执行清单，不替换原阶段退出门、视觉目标或规则规范。Phase 6 不启动。只有最后 G8 在同一冻结产品上通过，才完成 M4 goal；局部 PASS 不等于发布通过。

## 最新执行入口（2026-10-20，检查点55及后继）

下文“恢复时事实”和工作包表保留历史；验收条件不变。当前 M4/R1–R7 均未闭合，Phase6 OFF。逐项实际结果见执行日志后继与封存证据，不拼接不同候选的 PASS。

| 项目 | 已有后继证据 | 当前未完成项 |
|---|---|---|
| 袋口几何/画面 | 最终共同源081a158a…/PCK6ddb77ca…全域闭合、完整H0有限同域、三LOD及8个实际GPU局部帧已核；P4后缝改善，P5三帧与v14逐byte同。六袋INNER观察体已新绑定并独立闭合 | 鼻口9702/皮沿30176有限参数集合及4调试+12正常GPU补查已核，UNKNOWN保留；四角3072明确collar面材质候选444a24eb…另经10实际GPU核，旧cyan islands去除，P4/P5逐byte同；残余可见轮廓仍判读中；暗内衬/小角耳、真实产品相机/完整六景、同一新源运动未验收。旧cf7 receiver变化失败保留 |
| 低速真实击球 | Root100首次在当前081a/444/6ddb同源实际Controller击球完整76；152aux/hash、23978段/1216球步及全323774面raw/stored整球捕获间隙独立核过。旧6ee的95/96复用为历史局部证据 | 仅P2角袋低速切片；当前源新故障回滚/其余五袋/多球/108矩阵均未验收，108仍NOT_RUN。101已接pre-input case及actualOpening动态receiver，但central12全部在零输入初态严格检查拒：原默认静止rack浮点重叠。102另版确定性向外舍入rack构造准备中，旧初态/profile不改；严格ODE/材质校准/正式消费者未准入 |
| 性能 | v15 Root独立三运行8782172 F64/20875逐调用核同；高阶新场研究v1已封存实际失败和成本 | G4严重FAIL，Root100均2549.475789ms/h、P95 23702.758ms/h。v1发现literal时域绑定缺口与粗场漏spin相变，不能用PVW小差称物理通过；v2须真首事件/整时域门，108不靠旧D92优化外推 |
| 规则/入口 | R3裁判CPU与入口预演复用。Root已完整独立重编140 TU的普通现代相机候选，公共default4/candidate5 preset及原子拒/既有物理hash不变核过，8实际GPU帧实看 | 正式Billiards仍旧路径；新普通相机不含100 PCK物理准入。初始H0种子已修；Free220裁角/340大黑灯失败均保留。新v3 25°/275cm初始实帧六袋全入框且灯净空局部通过，启动配置未重author问题也已实证修复。Mac锁屏使真实键鼠/过渡未验；暗内衬/曝光/完整产品UI/ShotFollow/六景及裁判闭环仍OPEN |
| 声音 | 用户已选择新版v3五接触声作为可测候选，采用既有处置记录 | 默认未晋升；权属、最终发行、其余cue内容、外部盲听、两设备与loops待验 |
| 研究/发布 | 13课人工通过保留；用户可安排5新+3经验参与者 | 研究0/8，版本未就绪；冻结同源全回归、发行包及P0/P1归零证明未完成 |

证据入口：[执行日志](evidence/20261016-m4-execution/README.md)，本轮封存索引 `docs/evidence/20261016-m4-execution/checkpoint55-root-review-seals.json`。六袋新观察绑定 seal `b249c3bf80ae4240a631ca9c7d2bc68ba3f6fd225fdb1c58f2e020e4acac70d3` 仅证明几何/显式归属，数学cap不得进入CCD。已清理约816MiB旧可重建编译对象，保留源码/日志/证据及受保护程序。

## 1. 基线和证据纪律

- 共享脏工作树原样保留；禁止 reset、覆盖旧证据或 `build-product-session/Billiards`。其恢复时 SHA256 为 `44374e01d2b4eecadef773aa8775324047f4ceda42822cbbf4004aa50a6169eb`，执行前后复核。
- 主 Agent 独占主构建与 GPU；C++ A 负责数值性能，C++ B 负责完整击球/真实 mesh 接触与兼容，技术美术负责实体、六袋外形和资产。各自独立候选目录；集成后重新验证，不拼接不同候选的 PASS。
- 每项记录源码/编译对象/二进制/资产/profile/formula/规则/回放版本、命令、环境、机器、原始日志、退出码、失败与 SHA。状态仅用 NOT_RUN、RUNNING、FAIL、PASS_SCOPED、PASS。
- 候选输入与旧证据只读；输出写新目录。缓存只复用已证明的几何事实，实时位置、速度、角速度、phase、轨迹范围和响应必须重新取值。失败保留最后完整 tick，不切旧物理补完、不丢步、不隐藏未完成模拟。
- 现有 13 课人工通过、路标/整数采样器局部原生证据复用，不要求用户重玩或录屏。冻结产品仍需自动回归。13 课不替代 8 人用户研究。
- strict GMP/ODE 路径保留为离线研究；不再以长链猜修阻塞实时数值产品。数值候选另立身份，不复用 strict 证书或偷偷改变旧 profile。
- 恢复时磁盘不足 1 GiB：先盘点可重建编译缓存；仅清理确认不被当前工作依赖的编译产物并记录清单。旧日志、证据、源码、资源和保护二进制不删除。尽量 hardlink，不重复导出大证明。

## 2. 恢复时事实（并非新验收结论）

1. 最新技术美术 source-v9 / whole-context-v7 报告整桌 217370 面、非法相交 0；独立拓扑和三 LOD/PCK 对应复核中。它仅改过一个角袋外包，另外三角袋和两中袋的形态仍待完成；运动和新原生画面未通过。
2. 数值 v6 已在有限 cloth/flight 域完整运行至 351，之后非 cloth 域仍拒绝；不是完整击球、六袋或普通产品能力。
3. v6 两步前缀均值约 11.01367 ms，351 完整事务 178.247 ms，FAIL 2 ms；两种统计均需保留，不能用前缀掩盖首碰。
4. 原生 v7 partial 预览实际 tick1/2、第三步拒绝、日志/抓帧/来源一致性已通过独立复核；普通完整 Shoot 仍未开放，正式入口未切换。
5. R3 规则产品闭环、R4 音频内容批准、R5 冻结全回归、R6 真人研究、R7 发行仍开放。

## 3. 工作包与硬验收门

| 门 | 责任与先决条件 | 交付与可验证验收 | 恢复时状态（历史；后继见最新执行入口） |
|---|---|---|---|
| G0 基线/资源 | PM；立即 | 记录进程/脏树/保护二进制 SHA、可用空间、全部候选身份；无并发主构建/GPU；当前证据可读取且没有覆盖 | PASS，baseline.json已记录 |
| G1 整桌实体与开口 | 技术美术；G0 | 实际 native 坐标完整全域表面相交非法 0；每个闭合实体 directed-edge/vertex-link/退化 0、正确体积/方向；仅实际反向共有面可取消；held 面给完整区域等价；三 LOD、PCK 与源一致。另检查可见入口、实际支撑域和整球通行，不能用拓扑替代这些检查 | PASS_SCOPED：新角0源/连续自由球通道/整桌相交/三LOD与PCK已核；其余五袋只有采样截面必要门，六袋完整验收未过。旧v7窄喉FAIL保留 |
| G2 六袋视觉 | 技术美术＋PM GPU；G1 相应源冻结 | 原创中式外形：4 角袋与2中袋分别建模；木库/鼻口/包边/内壁连续，台呢与石板切口合理，接收结构藏于台体；无方形竖槽、外接木导槽、贯穿裂口。每袋俯视/斜视/外侧低视角实看，最终在正式六场景与实际产品相机复验 | PASS_SCOPED：新角0四姿原生静态已实看，其他五袋/正式六景/产品相机未过 |
| G3 完整生产物理 | C++ B；G1 对应 actual geometry；可先并行研发 | 持久 Controller/Runtime 经实际 Aim/Power/Shoot 产生完整固定步并提交到静止或真实收集；所有实际 kind 显式 law/所有权。六袋普通水平滚入必须先失去真实支持，经历连续重力/碰撞和 Entry/Channel，再规则入袋与物理收集；不得台呢仍完整支撑就隐藏 | PASS_SCOPED：Root独立v41角0母球真实Shoot→现场Entry/Channel→43实验收集；六袋/目标球/原生/正式规则/完整日志与产品仍未过 |
| G4 实时性能/确定性 | C++ A，B 提供代表路径；先对有限域优化，最终重测 G3 | 同一生产事务含固定步、规则、完整 guards、提交与详细日志：平均60Hz显示帧物理＋规则 ≤2.0ms；1080p Medium完整帧 P95≤16.7ms。报告 mean/P50/P95/P99、冷加载/首击、最重碰撞/收球、失败/回滚/丢步数；不删除351、后台未完成步或验证成本来过门 | FAIL |
| G5 现代入口＋规则完整对局 | C++ B/PM；G3/G4 产品路径 | 正式 Billiards 现代入口形成观察→瞄准→击点/力度→击球→结果闭环；普通完整新源可用，不再冒用partial。规则可信裁判边界与第一次非物理犯规具体原因可见，完整中式规则/换手/球权/胜负、基础AI完整对局可重复验证；设置/重绑定/可访问性及能力拒绝明确 | NOT_RUN，复用已有组件 |
| G6 视觉/音频内容与交互 | PM＋技术美术；产品候选 | 六景依原视觉目标全部通过；Low/Medium/High不丢关键信息；720p/1080p/高DPI HUD无截断；相机不改shot intent。解决旧EV/母球对比失败，SSIM仅在获准基线后判定。家具/接地阴影GPU实看；5类音频主观听审/权属/发布选择有记录 | FAIL / NOT_RUN |
| G7 真人研究 | PM组织，用户协助实际参与；G5/G6可测版 | 5名新用户＋3名有真实台球经验用户，按原§9.6及既有研究协议完成4类任务；5名新用户首次有效击球中位数≤30秒，至少4/5无需帮助找到力度与击点；3名经验用户三项解释全对；8/8镜头往返不改变intent且完成任务。真实8人记录，不模拟参与者 | NOT_RUN，0/8；用户已确认可安排，版本就绪后通知 |
| G8 冻结回归/发行 | PM；G1–G7 | 同一源码/二进制/资产/profile/rules/replay冻结版本跑全部必需回归、六袋与六景、完整人机对局、13课自动回归、坏资源/退出恢复/回放/旧数据隔离、性能；未决P0/P1=0，干净环境发行包启动/资源/字体音频权属/设置持久化/回退全部有证据 | NOT_RUN |

### G1 / G2 的具体检查

- 真实照片沿用已实际查看的 JOY、KNBB、Diamond 与 WPA 参考，保留中式/美式区别；不得把美式大口、厂商商标或素材搬进游戏。新角0只是方向样例，不能替另外五袋。
- 可见 cloth 边界与物理支撑边界从实际三角形重算；原生共源边应完全对应。浮点投影误差按实际坐标量化并记录，不能靠扩宽容差改变开口。
- 入口平滑与拓扑分别验收：原始轮廓共享端点/切向，渲染法线无可见断裂；逐三角面本身不宣称解析 C1。物理跨共有边不得产生重复冲量或虚构支撑。
- 球通行以完整半径球体对实际 face/edge/vertex 的连续扫掠验证；报告最小净空、首次接触 primitive/normal、是否有回台。不能只测袋心一条垂线或中心点空洞。
- GPU 必须记录实际资产 SHA、姿态、相机、二进制和帧；PM查看像素。静态源0相交不自动赋予物理准入。

### G3 的最小验证集与回执

1. 先一个角袋普通滚入从真实 CPU Controller/Shoot 到最终收集；必须完整提交每一 h，不用直接设置下降速度或垂直落袋 fixture。
2. 扩到六袋：各做低速/常用力度中央滚入、两侧偏口接触、擦鼻/沿库接近、明确未进球路径。初始布局、力度/方向与采用的速度范围写入版本化 scenario，在运行前冻结；不得跑后删失败路径。
3. 上述 CPU 路径再由同一二进制的实际原生键鼠完成代表六袋击球与一次多球开球；持久会话与 CPU harness 的结论分开写。
4. 每 tick 记录输入/解析动作/控球参数、16球 p/v/omega/phase、clock、trajectory generation、完整段与接触/支持 primitive、规则事件、capture/隐藏时刻及 before/after hash。capture 时必须能证明实际落球空间、深度和下降状态；母球落袋重置与目标球消失分开。
5. 未失支撑、不在实际通道、擦袋回台、未知 kind、geometry/profile/law 不匹配均不得产生合法捕获或成绩。任何域不支持必须显式拒绝并保持最后完整 tick。
6. profile/formula/mesh/material-law/content/source/replay/rules 全绑定；旧 Legacy、交互v5与Phase3 v5区别保留。旧回放按旧版本播放或明确拒绝，不静默重算；旧训练成绩不迁成新物理成绩。新回放以实际事件和固定tie-break重播，重复运行状态/规则/capture链一致，确定性范围写清编译器/平台。
7. 数值 penetration、支持高度、法向残差、能量/动量、TOI和零时进展预算沿版本化数学合同审核；优化不放宽。每个新物理 law 说明误差与校准范围，不把“完全同旧候选”当真实台球校准。
8. 原计划§3.2及既有physics release gate继续有效：默认选择服从`physics_models/promotion/phase3_production_default.json`。新mesh/数值模型必须有自身兼容、确定性、适用域、校准/回归及正式晋升记录，不能因局部球路或UI成功静默改默认；旧Phase3 v1–v5拒绝记录不改。未晋升的elevated/jump/masse/miscue能力继续门控。当前外包工作保留原运动口面是该切片的隔离措施；若真实参考或球路证明原运动口面仍错误，另立有界重做与新几何身份重新验收，不能为保旧点而接受错误袋口。

### G4 的执行规则

- 先基于冻结v6逐一优化：安全区域证书→跨内外层复用→实际计时再决定下一步。单次只改可归因部分，每次比较完整状态/数学结果并用原算法复核所有命中和边界fallback。
- 每次都列完整351与前缀，两者不互代；正式门用包含完整击球/碰袋/规则/收集的显示帧流，不用相邻175对前缀代替重碰撞。
- 超预算时保留FAIL和分类，不增加未获原计划批准的例外。冷成本单列，也不能从同一真实操作延迟中抹去。

## 4. 执行顺序和可交接成果

1. **立即执行**：G0；技术美术独立复核最新0相交源，PM检查新整桌候选四姿；C++ A优化数值性能，C++ B接真实有限支持和一般法向接触。
2. **第一工程检查点**：最新整桌源/三LOD/PCK独立几何检查通过；角0实际画面方向确认；一个角袋真实水平击球完成下降/收集；性能before/after如实记录。任何未过项继续修，不称R1完成。
3. **六袋检查点**：推广四角与独立两中袋造型，物理六袋中央/偏口/未进路径全过，新模型回放/训练/规则隔离有真实正负例；同几何性能重测。先CPU后原生，保留失败。
4. **产品检查点**：G5入口/规则/AI/设置，G6六景/音频/家具阴影，形成可供8人使用的冻结测试版；所有技术门通过不替代真人研究。
5. **发行检查点**：真人研究反馈修复后重新冻结，执行G8。只在全部证据对应同一版本且没有未决P0/P1时标M4/goal完成。

## 5. 人工/外部依赖

真实参与者、音频主观内容选择、权属确认、视觉参考基线批准不能由自动化伪造。PM负责准备可审查版本、任务脚本和记录表后再请求所需输入，工程工作继续推进；没有回复不视为批准或通过。若真机锁屏，仅窗口操作等待实际解锁，CPU工作继续。

复用现有 `docs/art/phase3-user-study-protocol.md`、schema、pending模板和validator，保持匿名、实际观察和双确认，不另造一套参与者结果。复用 `docs/art/m4-audio-v3-contact-review-20261012.md` 中已经冻结的4.48秒旧/新实际SDL对比音频及其权属/听审待办，不重制已存在候选。G6视觉数值/六景/镜头按 `docs/art/visual-target-and-acceptance.md`：主体/背景约2–3EV、同一曝光策略、台面中心/角部首轮约±15%，并沿既有验证器的母球对比等批准阈值，不因旧EV约1.907/1.919或对比约2.474失败而调低门槛。SSIM基线未批准时只报NOT_RUN，不写PASS。

## 6. 执行记录（追加，旧结论不覆写）

- 2026-10-16：新goal active，三名角色已启动；共享脏树和保护二进制待G0最终核对；独立geometry源已比昨日107失败更新至source-v9候选0，不能混报不同freeze。当前没有任何R1–R7整体关闭。
- G0完成：`docs/evidence/20261016-m4-execution/baseline.json`绑定git状态/HEAD、进程、磁盘及保护二进制SHA。未删除数据；随后实测空闲13GiB，原因不推断。
- source-v9独立复核：`/Users/bytedance/Projects/github/BilliardGL-evidence/m4/20261016-v20-independent-geometry-source-v9-review/README.md`；7019668实际候选相交0，217370面拓扑/原运动和P4保持/三LOD-PCK源多重集一致；独立held H0边链/面积等价。只关闭G1实体子项，不证明六袋通行。
- PM实际GPU四姿exit0并逐张查看：`/Users/bytedance/Projects/github/BilliardGL-evidence/m4/20261016-root-whole-table-native-exterior-v7-gpu/root-actual-pixel-and-binding-review.json`；角0包边/下柜方向可继续，旧槽和外接长receiver未见；材质尺度/亮度/腿与背景、另五袋仍OPEN。STATIC ONLY，未绑定生产物理。
- 新PCK全域正向H0实际3298面；1919是held重三角子集，不可当全域计数。C++ B重核旧6捕获点在新PCK中仍全部有H0支持，旧捕获策略保持FAIL。正在实现真实有限支持/一般法向接触，原语PASS不等于完整球路。
- C++ A逐次归因优化仍FAIL：有界扩大区减少351miss却使前缀更慢，失败保留；共享outer非cloth索引的新trial两步均值约3.825ms，cold约28秒仍FAIL，待reference/冻结后独立复核，不把未冻结trial晋升为性能PASS。
- PM独立复跑冻结v10/v11：分别58/35文件SHA全匹配、actual exit0、351行剔除仅CPU计时后与各自同几何冻结结果全同。旧几何实际两步均值4.27474ms/cold26.8485秒；新几何4.19763ms/cold1.84064秒，均未过2ms。不同运行值与开发报告并列，不挑更好一次放行。证据`docs/evidence/20261016-root-numeric-v10-review`及`20261016-root-numeric-v11-review`。
- PM独立复跑持久Controller低力度停球正例exit0，44个完整tick/hash链/16球/4次真实normalized输入与冻结一致，最后全部v/omega为0；源码确认Controller仅构造一次且Runtime地址不替换。仅CPU seam停止击球，非入袋/六袋/native。证据`docs/evidence/20261016-root-persistent44-review/independent-persistent-shot-review.json`。
- 用户已确认可以安排8名真人；等待G5/G6可测版本后提供冻结包/任务/记录表并通知安排。参与者并未执行，G7仍0/8 NOT_RUN。
- 六袋研发首个角0Shoot真实9个完整h后，由旧非cloth AABB保守门在tri130475/kind1处拒绝，尚不是实际接触。后继做有限face/edge/vertex连续无接触核证；保留真实拒例，不删guard。G3仍未过。
- 角1推广trial出现新帽与旧木翼/下柜穿插，另目录保留；局部帽+木翼新共有实体通过，正在接完整台体，未替换source-v9已验证基线或输出生产候选。
- R3已有后继复用核对：`20261014-r3-local-referee-product-v1`已有默认关闭本地裁判席/PIN/private issuer、proposal-confirm、首次/重复原因及不可移植本机回放信任候选；PM新目录实际CPU运行与独立新进程重开均exit0（2989文件SHA核对）。首次原因不是重写任务，native面板/产品集成和新物理规则仍未验收。证据`docs/evidence/20261016-root-local-referee-reuse-review`。
- G4无损日志独立复核：PM实际v13运行351并按IEEE位对比v12，动态树/物理/数学0差异；同时构造全SHA链合法的首TICK重标META反例，旧decoder错误接受350条并宣称complete，已保留FAIL。A的独立v15修唯一META/grammar及terminal首tick/初态hash/count等；等待PM复验，不把生产正例与格式抗损坏验收混为一个PASS。
- G3支持边细查：角0第18步前tri130484/kind1距离候选曾被称接触，PM与B从actual PCK独立核其与127465/kind0的凸共享边：报告球心相对edge的精确normal-cone系数一正一负，属内侧ghost候选，不应加微vY。旧第17步拒绝保留；后继在CCD候选生成阶段加入真实邻接/暴露特征所有权，不凭材质忽略接触。证据`docs/evidence/20261016-root-corner18-feature-review`。
- PM重新安排两名C++并行补完整物理：B负责scene/CCD/支持边/重力/receiver/持久Runtime，A在冻结日志工作后实现一般3D normal的球球/静态mesh联合冲量纯模块；不继续无限优化cloth-only前缀。随后在集成完整物理上重测G4。

- PM实际v15b producer独立运行exit0：351条decoded与v14逐IEEE位完全相同，17个单段坏例拒绝，Root首TICK重META反例明确拒绝；175+176真实日志段逐树还原351，internally-valid错误parent段在单段decode可过但join明确拒绝。Root全部350相关两h窗口均值2.55014ms、完整351含close/CSVflush60.189ms，仍FAIL；不把相关窗口或prefix当最终产品60Hz帧通过。证据`docs/evidence/20261016-root-log-v15-review`。
- 当前角0第17h拒绝仍保留，B正在CCD consider阶段实施真实scene邻接normal cone，而非加假微冲量。A一般joint模块的F64约束通过不等于F32提交约束：一个斜面fixture经F32后tangent residual约1.81e-6cm/s，必须记录实际存储误差预算或拒绝，不能清VY/ω来过门。
- 后继角0已从17推进到24个完整h：PM实际复跑corner24冻结bin得到相同24行及预期exit2，逐步核19–24的p/v与一次F32舍入后的重力抛物线完全相等；17球心Y89.341476→24 Y87.825737cm，第18步真实负反力释放，不是改动画或预设竖直速度。132实际object/depfile、418项目依赖SHA和9自有preimage均独立核对，工作树src依赖0。逻辑Entry/Channel/receiver/capture仍未通过。证据`docs/evidence/20261016-root-corner24-review`。
- 第25h未提交的neck Vertex199378再次证为ghost：PM全fan精确Voronoi检查与B同结论。PM另作全public surface点距诊断：24末余量约+0.223cm，而未发布candidate25位置距另一真实edge仅2.4559cm（R2.8575），要求处理更早真实撞壁。B确认旧按triangle索引Unresolved failfast会遮住更早候选，正在改为完整遍历并以时间窗排序，只在已证最早upper严格早于所有未决lower时返回；不能排ghost后直接穿过。`docs/evidence/20261016-root-corner25-vertex-review`。
- receiver归属与只观察体：TA从actual current mesh给出source→gid映射、6个INNER wall/floor及实际254/258点入口环构成闭合观察体；数学封口不进PCK/CCD。791 neck原语未归属仍保留，不能拿观察体代替整球通行、全neck或捕获证明。
- 中袋原始参考补查只实际看见原厂34:23外侧中袋和第二视频41:09画线片段；两鼻口同帧的中式中袋俯/斜近照仍OPEN，未把搜索说明/斯诺克85mm/播放器错误当证据。见`docs/evidence/20261016-m4-execution/middle-reference-followup.md`。

- 新scene分支实际性能仍严重FAIL：PM corner24 CPU报告135.244ms/h、270.488ms/两h（旧probe口径，非最终wall/帧预算）。不得拿不同branch的v15有限域2.55ms替代；完整物理接通后需同一branch再优化并测全部成本。
- B新遍历在未决窗之后返回了更早可证neck edge contact t≈.0029531078s（双incident211482/211701，需同edge去重）；仍未施响应/未提交25。kind11 e=0、µ=.2仅显式实验初值，不是经验校准或默认批准。将接A一般joint并处理撞后可能立即需要的持续force，不能改大e躲开支持。
- Root实际复跑一般joint纯组件与封存输出逐字同，Decimal80复核0；只条件代数组件，scene/完整h/F32提交准入仍须B实测。G2新木下柜局部共有实体虽0交，actual receiver里出现182交叉已保FAIL；须按真实receiver空间收薄，不能以局部0交推广。

- PM 独立执行冻结 retained-force v5：31文件SHA无变，actual输出与封存逐字节一致，另目录重跑Decimal80核11个真实attempt的力/力矩/功率账；只通过瞬时组件，不表示有限时间积分、完整25步或进袋。证据`docs/evidence/20261016-root-retained-force-review/independent-component-review.json`。
- PM 从 actual PCKTSOL2 独立核第25步已证候选的211482/211701：同一共享边、kind11、局部严格凸、名义球心的边投影在内且exact normal-cone系数双正/残差0。双面须同一物理约束去重。只核名义几何，完整TOI窗、全局排序、响应和剩余时间仍由集成验收；当前仍24个完整h。旧PCKTSOL1 reader在magic处明确拒绝后，按实际v2格式重读，保留工具失败。证据`docs/evidence/20261016-root-corner25-owned-edge-review/independent-actual-shared-edge-review.json`。

- Root独立核角1新v5下柜左右木结构与actual owner1 kind7/8 receiver：actual PCK4337面逐面对应source；exact半空间对2437receiver顶点与全部两木体native点证明至少1.36988mm间隙，满足1mm局部目标。只消除两下木体/receiver穿插，不代表upper skin/旧台体和mount/整桌或GPU通过。证据`docs/evidence/20261016-root-pocket1-receiver-clearance-review/independent-exact-lower-wood-receiver-clearance.json`。

- 首次完整角0接触步 v14：B同一持久Controller真实Shoot实际提交25，26因`retained_contact_next_h_resume_NOT_IMPLEMENTED`拒绝；原bin/源码/132对象/423依赖已封存。PM实际独立复跑同bin exit2/25；25行仅剔除exclusive_CPU_ms后全动态树逐IEEE64位一致，连续hash与16球guard、256微段正N/耗散摩擦核验。Root25 CPU61.633659秒（core41.3579、path_collection20.270721），严重FAIL，不能用排除odd25的12对均值替代。正式solver_events/contacts=0，Entry/Channel/Capture未接，无完整进袋/native/六袋批准。证据`docs/evidence/20261016-root-corner25-v14-review/independent-actual-run-review.json`。
- Root对25已提交9分量先验F32 bit后，用Fraction+200bit向外sqrt区间从actual共享边重算位置/法线/速度变化：raw vn约6.34178e−8、stored vn3.410126e−6 cm/s，stored gap−1.24344e−6cm。只允许此准确stateHash绑定的派生误差，不回灌raw、不清vn或投影。后继v15续持在实现，不能以裸2e−6门假称端点通过。源审查初读时finalω追溯缺陷已在实际冻结v14修复；早期snapshot绑定晚于live修复，另resolution明确纠正，不把旧发现归给后继hash。
- TA角1整桌新拼接首轮248634面仍FAIL：7530225实际候选中342非法，退旧wood/port、mount、bank、stone、H0交叉均保留；局部v5与receiver分离不关闭整桌。继续真实native截面共边和旧支架回接，不移动H0/nose/R。

- 后继v15 actual完整25状态核对：PM只读对照v14/v15两次开发实跑，25行全部16球p/v/omega和256个fine stage逐IEEE64位相同；不宣称整个stateHash一致（版本context/profile不同），也非PM独立v15复跑。25 CPU开发实测7.324s，比旧62.822s降低但仍严重FAIL。26已通过actual F32 witness续持入口，在后续midpoint `retained_v15_stage_edge_ownership_lost`实际拒绝回滚。见`docs/evidence/20261016-root-v15-motion-comparison/actual-motion-comparison.json`。
- PM独立重跑A supplied Fraction checker：14条actual run10/13全程query、197366排除记录/98683独立query-triangle proof核验，actual PCK与路径外包/全R严格分离，6输入前后SHA不变；不宣称独立checker算法或整个simulation等价。见`docs/evidence/20261016-root-euclidean-pruner-review/independent-checker-run.json`。
- v16补齐真实26失败midpoint后，PM从actual PCK用Fraction独立验证：旧edge parameter仍内部、cone alpha严格负、beta正；incident face211701的投影严格在面内且距离更小，211482投影不在面内。因此是实际edge→face归属切换；不能拓宽旧edge域。当前只核失败点，earliest切换时间/后继face积分及完整26仍未通过。见`docs/evidence/20261016-root-corner26-feature-transition-review/independent-exact-incident-face-transition.json`。

- PM已实际独立复跑冻结v15优化bin exit2/25：32封存文件SHA匹配，25行仅剔除CPU字段后全动态树逐IEEE64位同冻结，256stage和hash链一致，26同ownership-loss拒绝。Root25 CPU7.624651秒（core6.063042/path_collection1.557469）；保留开发实测7.324秒，均严重FAIL。A正在同一v15bin以FULL_QUERY_REFERENCE在独立目录跑完整检查对照，Root观察PID51824实际live，不重复启动。证据`docs/evidence/20261016-root-corner25-v15-review/independent-actual-run-review.json`。

- 同冻结v15 fullquery reference实跑已结束（A启动，Root观察live PID51824后终态并独立比较）；与Root优化实跑25行state before/after hashes全同，逐IEEE树仅三个明确计时/pruner观察subtree差异，26同拒。reference25 CPU65.110883秒，Root优化7.624651秒，均严重FAIL。h18 axis-kept残额96单列给A核原nonH0过滤，不把它称欧式排除。见`docs/evidence/20261016-root-v15-fullquery-reference-review/independent-actual-fullquery-comparison.json`。
- v18已将首个actual edge→211701 face事件receipt补齐。Root从actual PCK/native N0/N1、已收敛piece P0/V0/a逐Fraction重算α系数全同，local bracket左右严格异号且t>=0导数严格负，证该piece首个唯一正根；连续P/V/omega在F64解析运算误差界内，无额外冲量。仅coarse首事件，26后继另一个actual feature仍拒，未提交26/fine/进袋。见`docs/evidence/20261016-root-corner26-root-event-review/independent-exact-first-event-root.json`。
- TA角1整桌v10当前edge接界仅剩旧mount两组11条几何裸边，整桌全候选仍待完成；正材料分体/原运动面/LOD-PCK/GPU均未过，不资产化。C++ A下一单独v2尝试native triangle-plane slab严格区间排除，先测实际768微段剩余700473候选，不改变B narrowphase或v1封存。

- v19显式law范围独立核验：actual PCK全域kind11为37924面，22197只属owner0/component294，恰等gid195173..217369；Root逐面核该22197 source/PCK native坐标一致，791未归属全部在外，其他组件未获该law。实验e0/µ=.2未校准，原UNBOUND/默认隔离保持。见`docs/evidence/20261016-root-component294-registry-review/independent-registry-boundary-review.json`。
- v19 native chart循环内核h26两档产生5相变/259fine pieces，但完整new-wall检查后仍拒而回滚，不能算提交26。Root独立解析actual后段211918/211919：两native共享面是严格局部凹折，双方另一点有向plane值均正，TOI盒上两面的投影均在面内，不能当凸edge ghost/近共面忽略。进一步从实际piece P0/V0/a对211918满R plane根精确二分：start gap正、end负、全段严格接近，首个唯一根t≈2.424459271376595e−6秒、投影严格面内，与query_owned名义更早命中吻合；应处理新面接触及旧支撑约束释放/联合响应。pending更晚名义1.0614e−5的min_lower0诊断一致性待核，未直接忽略。见`docs/evidence/20261016-root-corner26-late-face-review/independent-actual-piece-second-face-root.json`。
- A平面过滤v2冻结并具Fraction审计：剩700473再排117253(16.739%)、额外filter CPU38.15ms；未接Runtime、未称全步改善。下一重点改为低扰动细分profile；首轮逐函数clock对约1989万递归根调用带来大量观测扰动，保留诊断失败并重做低扰动，不把19.8秒observer当原7.3秒成本。
- TA角1整桌v10实测7530438候选/86非法：旧mount×wood62、右bank rear/underside×backing24；短mount局部22面闭合且winding入弧柜不替代整桌/receiver/正材料审计，全部仍FAIL。

- v21修正candidate来源feature与face U+V相关范围后，actual26 query首次明确报告211918 Face，hit=true/ambiguous=false/initialPen=false，无后假根Unresolved遮挡；Root对照先前独立exact root，时刻差上界4.72306e−15秒，clearance3.55e−15cm。仍未施冲量/提交26。v22接joint与旧211919支撑释放，保已有负gap和派生pointTol，不把mesh点改成sphere臂伪称贴面。见`docs/evidence/20261016-root-v21-real-contact-review/independent-actual-hit-vs-exact-root.json`。
- Root独立读三次observer实跑并与自身v15实跑比较：25步仅CPU观察字段外全IEEE树一致、输入回执逐byte相同、26同拒；共同416计数项全同。coarse比旧observer多一个batch metric/26行，不能声称整个CSV dictionary同schema。逐call clock19.802秒有明显扰动，counts-only7.644秒、低扰动batch7.804秒；测得root/feature流程exclusive约2.767秒、query1.938秒、exact face box1.460秒。下一query-local full-bit root memo已实跑首25但仅微弱收益，待samebin reference/完整state对照，不推广。见`docs/evidence/20261016-root-observer-profile-review/independent-actual-observer-data-review.json`。
- TA来源修正：旧v3/v6等平背接口诊断遗漏 frozen new_complete_cushion_sections 对shoulder.profile的56点覆盖；本轮actual bank rear是圆化曲线，不能从旧G两点默认或whole exterior缺失的内部面反推私有体。这些平背诊断始终未资产化，保留FAIL；先前局部面交/clearance只表示那些几何的局部属性，不证明实际bank共有接口。新候选必须绑定实际覆盖后的native rear源重新作有限面积接口，禁止删原bank/拆点/epsilon。v11真实短mount共源union已edge0但整桌vertex link2和右rear相交仍FAIL。

- v22首次实际提交bounded26，limit26时仍moving；Root同冻结bin独立复跑exit2、26行仅剔CPU字段后全IEEE树一致、hash链连续、16球h guard和末9个F32 bits一致，132实际.o/.d+425项目依赖SHA匹配。二次211918真命中joint后新Jn3.95893e−5Ns，旧919 J=0且post vn+.0232854752cm/s，新vn约−2.36e−16；旧gap不投影、单调改善，实际mesh点保留，joint能量耗散。Root CPU25=8487.514ms、26=5727.782ms，严重FAIL；该branch与v15 timing不可混作相同开销。正式contacts/solver_events仍0、Entry/Channel/Capture/native未接。见`docs/evidence/20261016-root-corner26-v22-review/independent-actual-bounded26-review.json`。
- 26的组合整h coarse/fine（含二次TOI/joint）仍明确NOT_MEASURED；目前preimpact chart与tail分别两档不替代组合误差，旧trial二次hit后的部分标为UNPUBLISHED，未当真实路径。下一步持久Controller从真实F32 active-face/旧释放状态继续27+到真实下一边界，不把bounded成功称整次shot/进袋。

- Root从actual native edge211918/212137以Fraction+200bit向外sqrt区间独立证v24最终accepted_midpoint残差：vn≈−8.3814099949e−6cm/s，严格越原预算5.9730375186e−6；edge投影内部/cone双正。不是未收敛trial当真实事件。128拒保留，同入口更细网格重算才被允许，trial独立1cm/s代数allowance标NOT_PHYSICAL，最终物理预算不改。见`docs/evidence/20261016-root-step27-normal-residual-review/independent-exact-normal-residual-review.json`。
- v25实际提交27，Root同冻结bin独立复跑exit2/27：36封存文件/132.o/.d/425依赖SHA匹配，27行仅CPU字段外全IEEE树同冻结/hash链连续，16球h guard/末9cast bits一致。128失败入场P/V/W与真实26末完全同，256/512 maxvn2.727026e−6/6.002857e−7在原预算内，fine516pieces。Root27CPU10.158318秒严重FAIL；28同真实force phase拒。旧919无限plane已与实际finite face分开，不能以plane负gap单独认碰撞。见`docs/evidence/20261016-root-corner27-v25-review/independent-actual-27-review.json`。
- TA actual56 source绑定独立复核：4个主源SHA与override赋值匹配，两侧各64 rear有向面逐native坐标命中source-v9，每个反向私有面确实在绑定的新正body中。只证明来源与私有facet，不证明wood/backing union或whole/GPU；带lip的4/12交叉仍FAIL。见`docs/evidence/20261016-root-actual56-bank-reference-review/independent-native-rear-source-binding.json`。
- 下一物理28隔离单接触edge所需reaction首次0，与native feature事件竞争；事件前后保持实际状态和完整gravity/new-wall核验，不把unpublished trial当释放时刻。27通过不替代组合26含二次撞击refinement，正式events/rules/audio及Entry/Channel/Capture仍OPEN。A仅quartic memo候选old-v15单次6.614s/ref7.907s仍FAIL，缓存分支停止、后继在新路径需独立对照；下一结构优化只针对实测高成本immutable facts/interval→exact fallback，不删guard/log。

- v26首次提交28含自然脱离；Root同冻结bin独立实际复跑exit2/28，36封存文件/132.o/.d/425依赖SHA匹配，28行仅CPU字段外全IEEE树一致、hash链/16球h guard/末9cast bits一致，29同scene未决拒。Root28CPU6849.171ms，严重FAIL；正式events/contacts仍0，非进袋/native。见`docs/evidence/20261016-root-corner28-v26-review/independent-actual-28-review.json`。
- Root由actual PCK shared edge212572/212791及accepted piece独立重构reaction S六次多项式：全7系数逐Fraction同，根左右exact值正/负，完整nominal span的S'六个Bernstein系数严格负，证此piece首个唯一零反力根；释放全h时刻.00568142771595598秒。之后.0026519060519945552秒仅gravity的raw P/V逐F64与实际fine终态完全同，ω不变、末9F32同实际state，无新冲量。不是下落动画。这里只独立核该piece事件，不冒称完整shot/全六袋。见`docs/evidence/20261016-root-corner28-v26-review/independent-exact-release-and-gravity-review.json`。
- 后继v27移除显式SourceVertex经严格Voronoi证据的假根后，29pending6→2，仍28发布/29拒。余SourceEdge141156须以actual有限最近edge整TOIbox严格距离更近证明后才可排；未知slate真命中保留，不按kind跳过。kind1不自动套用kind11实验law，需要另有绑定actual primitive/author solid/namespace的新候选law与兼容隔离。
- TA whole v12：248998面、edge/link/degenerate0、1 boundary component/Euler−40/正体积，7550513实际候选仍24非法全属旧右平背接口；不出GPU。旧lamina signed-chain删除路线因实际outside winding0而未推广，新的真实有限wood回接无删lamina且无改bank/cloth/nose；局部/拓扑通过不关闭whole相交及正材料门。A继续实测data-bound lazy exact affine facts，原谓词每次保留，后续interval filter尚未实现。

- v28 actual29已露真实slate shared-edge140897/141152、t=.004510696187869142秒，因kind1未注册law保真拒，仍28发布。Root独立核全部53个SourceEdge根排除：四端点均actual native三角边；alternate edge在整TOI centre盒中投影严格有限；二次距离差的exact Taylor lower严格正；盒本身包含actual28末F32输入到整个[t−tol,t+tol]自由飞行二次路径。仅排sourcefeature、不删原triangle/其他roots。见`docs/evidence/20261016-root-step29-edge-exclusion-review/independent-all53-edge-root-exclusions.json`。
- Root另从actual28末P/V/g和actual slate两面共享edge重建满R quartic：整个h导数Bernstein严格负，首唯一root≈.004510696187869142与实际报告差上界6.36e−20秒；native finite edge内部且normalcone双正。真slate碰撞不能按kind忽略。v29新工程law只给这两gid并绑定actual payload/namespace，µ取冻结profile原float32值double(.20000000298023224)、e0、NOT_CALIBRATED，原UNBOUND/旧默认/回放成绩仍不改；未验收law/冲量/29/产品。见`docs/evidence/20261016-root-step29-edge-exclusion-review/independent-exact-real-slate-edge-root.json`。
- A exact affine Facts独立候选已on/off/shadow完整25/26失败树与128k shadow0差并封存，old-v15单次5.782s/ref6.768s仍严重FAIL；保B新direct(U+V)/Voronoi逻辑，不能整旧owner头覆盖。下一最大残余freshquery约2秒，独立同builder BVH节点级Euclidean排除实测：先比actual768 query IDs/顺序和每node actual descendant包围/严格排除；不先materialize16M轴候选，leaf仍原predicate、原R/guards/log不改。未测前不作提速声明。
- R2复读正式迁移提案：现有EXCLUDE_FROM_ALL只完成CPU/link预演；正式源/测试重绑、真实modern启动和R1六景门仍要求后继证据，未切正式入口、未覆盖保护binary，也不将预演重复当R2关闭。


## 2026-10-16 新阻断：袋喉小于球径，重排实施优先级

Root 从冻结 actual PCK `84ddd0f116d3…` 全部217370 native面精确重切Y=86.22cm，6629截段全部degree2、13闭环。角0 probe嵌套2层/solid parity0，最内自由孔508点（kind1/kind11各254段）；其沿(1,1)的总投影宽仅5.521614916728812cm，小于不变物理球径5.715000152587891cm。**这是完整球经该喉口下降的严格几何阻断**：球心下降到此高度时赤道圆必须位于该闭合自由孔，任意移动圆心都不能弥补该方向的总跨度不足。此前surface/topology局部通过不关闭此项；应更早先完成这项整球通行检查。B和TA分别用自己的actual PCK截面程序复核，点/GID集合、嵌套、宽度严格平方不等式均同。

证据：`docs/evidence/20261016-root-pocket0-throat-section-review/exact-horizontal-section-review.json`；精确截面多边形、源码和比例图同目录。B独立复核在`docs/evidence/20261016-numeric-six-pocket-domain-v1/actual-throat-section-independent-B-v1.json`；TA独立复核在`/Users/bytedance/Projects/github/BilliardGL-evidence/m4/20261016-pocket0-independent-native-throat-section-v1/independent-exact-native-throat-obstruction.json`。

立即执行顺序：

1. **TA优先重做角0喉部共享几何**。角1全部候选/24交叉暂存，不在已证不可通旧孔上继续磨rattle。追到fore slate bevel与rear neck两端的真实作者源；不能只磨单一碰撞点。按原创中式目标修改实际窄轴，不移植美式大袋。若必须改H0/运动口面就以新geometry/profile身份重做，不为保旧点接受错误开口。球径不变，旧资产/失败/default不改。
2. **G1新增前置门**。候选工程目标为喉部自由内部存在连续整球通道，半径至少实际R+0.1cm（径向1mm裕量）；该值是工程门槛，不是厂商喉径声明。各相关高度最小投影宽达到2(R+0.1cm)只是必要门，不当充分通过。须给连续3D swept-sphere到receiver的实际全实体/全R无侵入证据。合法台呢/鼻口支撑接触段不要求悬空1mm；自由通道与接触段分别验，不能用垂直袋心fixture代替普通水平滚入。
3. **C++ B保通用引擎工作，旧孔仅作负例**。冻结已有响应/假t0 no-root数值证明；独立完善whole-h剩余时间协调器，在TA新payload/gltf/canonical contract身份冻结后重新建manifest、explicit law registry、cache/context并运行实际Aim/Power/Shoot。旧孔不得通过改R、e/µ、提前capture或隐藏来造进袋。新law仍需校准、旧profile/回放/成绩/规则兼容隔离及正式晋升记录。
4. **C++ A继续几何无关性能候选**，但旧v15前缀只算参考性能，不是新geometry球路通过。节点级BVH排除先actual查询ID/顺序/后代包围与strict distance证明、再on/off全状态对照；新mesh不跨旧identity复用事实或树。
5. **新几何与产品门**。整桌表面/正材料/边/vertex-link/体积、三LOD/PCK/source一致性全部重验；Root独占GPU查看真实俯/斜/低位。随后新版本六袋普通滚入/玩家完整击球、Entry/Channel/真正收集与规则/音频正式回执、整步与全shot误差/能量/确定性/性能全部验收；R2–R7与8人研究要求不缩减。13课人工通过继续复用，不让用户重玩。

袋口与M4均仍未完成；新证据证明旧孔失败，不撤销旧数值前缀的局部数学结果，也不将其晋升为可通喉口。


- 新角0 canonical source v1（054aa2a28939…）完成整桌7,027,430 exact相交候选0非法；TA三段连续sphere候选最小距离3.0145168/2.99800665/3.38282399cm大于R+0.1cm，尚待新三LOD/PCK身份/真实水平Shoot。Root独立逐源核14874点/30542面改变，所有Y、H0及以上、kind3/7/8 native点完全不变，索引/kind不变且无新distinct-point collapse；起点exact +X ray交6次/parity0、该Y无native顶点。Root对三段全217370面精确AABB排除，候选21807/20143/1704完全同TA。Root独立重切旧阻断Y86.22：同13闭环/508点自由孔，对角宽由55.216149mm变为60.062169mm。该截面仅必要门，未证明运行球路/六袋/正式events/默认推广。证据`docs/evidence/20261016-root-pocket0-canonical-throat-review`，比例对比PNG为源几何截面、非GPU或实际球轨迹。G1整体仍OPEN，旧资产几何FAIL保留。

- 新角0三LOD/PCK已隔离打包：glTF da1932c5c1f7…/PCK f748973a5922…/manifest b4bac3392275…，新namespace billiardgl.original_chinese_throat_engineering.v1、所有law UNBOUND_<semantic>。Root独立stdlib decoder对全部三LOD各217370有向(kind,triangle)与实际新source/PCK逐一multiset全同、无节点transform；并核TA handoff全部文件SHA。Root新4实际GPU视角已实际看像素，asset hashes全匹配、tick0/ordinaryfalse，只有角0静态方向局部通过，亮度/木纹/腿/其余5/玩家相机仍OPEN。证据`docs/evidence/20261016-root-canonical-throat-asset-review`及`/Users/bytedance/Projects/github/BilliardGL-evidence/m4/20261016-root-canonical-throat-v1-gpu`。B准备实际新OwnedGeometryData与新law registry/真实AimPowerShoot，A准备新mesh tree绑定验证，TA诊断其余5袋通行前置门。
- Root独立比对A v9实际on/off两次25行完整非observer状态/hash/轨迹/误差/能量和26失败树逐IEEE bit全同，80封存文件SHA通过；771计数逐字段与768+3 Fraction报告一致，冷source核6.471ms/副树33.592ms实际记入18步。第25步3815.096ms/ref5566.898ms仍严重FAIL、仅旧堵塞几何参考；未重复整条simulation或独立重写所有node几何proof。证据`docs/evidence/20261016-root-node-BVH-v9-review`。

- 六袋通行前置已扩展：TA新包P1/2/3/5实际整表exact直线自由球通道、P4偏心4段通道均>R+0.1cm且起点free；五袋未改变几何。P4 canonical中心竖线在source9272/PCK GID2568 kind1相交，Root独立exact点面距2.75138649cm<R、重叠约1.06114mm，但偏心自由通道存在，故不把单线失败推广为整袋堵、不无据扩大中袋。所有6袋仅“几何自由通道存在性”，非实际水平Shoot通过。TA证据`/Users/bytedance/Projects/github/BilliardGL-evidence/m4/20261016-other-five-pocket-native-throat-preflight-v1`；Root点面证`docs/evidence/20261016-root-middle4-path-review`。
- Root实际独立新OwnedGeometryData loader复跑：217370actual native glTF世界三角形精确match，new identity8e444e4c…/namespace/source/payload正确，全部源law仍UNBOUND；同独立loader对旧v30资源实际exit2 namespace拒。此为隔离消费，不是全部旧回放/训练兼容通过。证`docs/evidence/20261016-root-new-throat-loader-review`。
- 新真实Controller首v31完整24、25单Face199552被旧双incident入口拒；Root由actualPCK精确核该Face projection bary严格内、incomingV·N<0、nativeNy负。v32已允许真实单Face实验joint并负反力自然release，完整25。Root实际同bin独立复跑exit2/25：输入逐byte、25完整非CPU树/连续hash/26失败逐IEEE同；postjoint余.004329426386573146s纯gravity9raw逐F64与真实终态全同、9F32cast bits全同，无投影或清速度。Root25CPU655.148ms仍FAIL；正式events/Entry/Channel/capture未接。26在同edge零时残差假incoming处拒，最后25保留；B正做绑定实际joint witness的精确incoming-root分类，原预算不放宽。证`docs/evidence/20261016-root-new-throat-face25-review`、`docs/evidence/20261016-root-new-throat-v32-review`。

- 新v34从真实水平Controller击球完整39，40actual receiver_floor87856未绑定law正确拒。Root同bin独立实跑exit2/39：769封存文件SHA全核，39完整非CPU树/40失败逐IEEE全同、输入逐byte、hash连续；全程未隐藏/capture/outofplay，39整球已在原H0下方。Root25CPU659.752ms/max726.749ms/固定tick均121.0127ms，仍严重FAIL。18–39显式cast receipt actual/expected bits一致；1–17为原schema，未冒称有该receipt。证`docs/evidence/20261016-root-new-throat-v34-review`。
- Root从actual新native edge208371/208813与失败26postjoint P/V/g独立重建fullR quartic：q0严格负且全剩余h q'' Bernstein三项严格正，任何正root仅outgoing、t0非exact根；不豁免初始穿透门/其他feature、不改P/V/epsilon。v33原新增CCD编译却因旧对象非stem命名漏替换而未消费，失败/构建纠正保留；v34唯一CCD链接对象已正确。v35实际query即时导出的2条branch receipts全部5个exact系数和3个Bernstein与Root独立重构全同，779seal文件SHA核过；39个16球motion/phase与Rootv34实跑逐bits同，仅此motion scope，不称跨版本hash全同或Root另跑v35。证`docs/evidence/20261016-root-new-throat-edge26-root-review`。
- Root独立核新receiver0 named provenance map实际3521行：新source/PCK/namedauthor有向native三角逐一全等，509个inner floor全positiveNy且含actual87856；实际是斜底，不能当水平面，也不能全kind7赋law。该3521来源集合不等于2112观察体physical表面。证`docs/evidence/20261016-root-new-receiver0-rebind-review`；观察caps不进入PCK/CCD，law尚实验未校准。
- 后继v36开发实跑完整42：40floor一次冲量后正N/sliding Face持续力/两档收敛，41/42真实续持；43旧floorFace仍active时碰actual receiver_side edge96862/97568，旧单tail限制拒，未capture/Entry/Channel。v36 seal9c3ac615…保失败；Root尚未独立重复42，不能与Root39混作同bin验收。下一须真正联合floorFace+wallEdge冲量并判断释放/双约束持续力，不复制edge两incident成两个约束、不丢旧floor。

- Root独立实跑v36冻结bin exit2/42：791seal文件SHA全核，42完整非CPU树/43失败逐IEEE全同、输入逐byte/hash连续、无capture/hidden。Root max2949.080ms、均308.5498ms/固定h，仍严重FAIL。另用实际joint receipt m/R/I/g/µ和实际Face中点状态以Decimal80独立重算40–42全部768 fine stage：N正、摩擦功率负，N/a/alpha/power最大差约2.651e−16N/3.226e−13cm/s²/1.563e−13rad/s²/5.176e−17W。仅阶段代数与独立同bin复跑，不冒称独立全ODE或材质校准。证`docs/evidence/20261016-root-new-throat-v36-review`。
- v37实际43原TOI联合冲量组件accepted但整步回滚：floor与新wall各一个约束仍都active，post KE1.1355867683e−9J；剩余h双约束mode未实现。B后备持续力研发继续，同时研究符合G3真实收集的明确吸能collector合同，不以低速直接hide。Root对actual raw/prospective F32的2364观察边界独立exact审计：strict fullR containment两者都FAIL，全部负gap仅actual active wall96862/97568，无inactive面；已有joint点预算加实际cast位移有向上界能覆盖，未改R/P或新增固定epsilon。原strictFAIL并列保留，不能重命名PASS。独立exact raw K1.1355867683e−9J与首次任一球体部分到inlet势垒.0863945193443J；能量论证还须包含数值预算/被动静态/无外部冲量假设。候选numerical-contact-contained collector仍需显式新law、完整h/真实EntryChannel/旧profile隔离、日志和规则桥，尚未capture。证`docs/evidence/20261016-root-receiver43-containment-review`。
- 技术美术corner1 v23终局248968面/7550279全exact候选0非法；已合流全部原P0 canonical14874变点为source-v24（05839d75…），248968面edge/link/zero0且7558041全exact候选0非法，同一源六袋连续R+1mm通道通过。它是另一独立geometry身份，B稳定f748物理不动、不能混PASS；v2三LOD资产初次native/metre UV导出失败保留，未GPU/未默认，待修导出并复核。

- Root独立新P0/P1合流v2资产：actual source05839d75…、glTF1058ce25…、PCK04f55111…、namespace billiardgl.original_chinese_clear_throat_own_corner1.v2；三LOD各248968有向(kind,triangle)与source/PCK全同、节点无transform、laws全UNBOUND；17项TA交接SHA核过。Root实际生成并查看P1内斜/俯/外低/whole、P0内斜/俯六帧，metadata绑新asset；两角外包方向一致、未见原外方槽，但小端部转折/光照/木纹/内衬/腿/其余4/正式产品相机与六产品场景未验收。这六静态视角不是六产品场景，v2资产PASS不能与f748物理PASS拼接。证`docs/evidence/20261016-root-p0-p1-merged-asset-review`及`/Users/bytedance/Projects/github/BilliardGL-evidence/m4/20261016-root-p0-p1-merged-v2-gpu`。P2接入须自身native bank/receiver，P3预检发现receiver不对称不允许照搬。
- 补充实际像素查看星牌官方袋口文章和35周年中式球桌产品图，新增可核中袋正外侧皮套/篮体及角袋下方结构；仍没有同时看清中式中袋两鼻口和内包边的俯/斜近照，不关闭该参考缺口。不复制品牌/材料/纹饰图片，无媒体下载。具体URL/逐张可见范围与拒作入口证据的品牌图已记`docs/evidence/20261016-m4-execution/xingpai-photo-pixel-followup.json`，临时browser tab已关。
- collector proposal已审可继续隔离工程候选，不是产品/材质校准晋升。v39 actual动态predicate已从当前context重新验证观察实体/native physical facets并核raw与prospectiveF32，strict false保留、仅active两wall face数值容纳true，但缺真实history仍拒capture。v40接现场receiver inlet±R实际二次root cuts、generation/trajectory分段；Root要求history与state/clock整h共同rollback、session隔离，明确receiver Entry/Channel不冒充H0失支撑/Mouth真实开口，H0旧释放证独立。尚未完整43/捕获/正式规则。
- A新实际42低扰动profile拆清：h41/42各768 SceneCCD、512FreshGuard、769swept；owner各5次构造约697/703ms，force各2304次仅约6.9/7.1ms。主成本是重复wholemesh CCD与immutable拓扑构造，不是force数学或ownerAffine（41/42=0）。v13共享immutable拓扑在新39单次完整语义对照通过但仍严重FAIL；先独立迁入新42 on/off，动态判断保持fresh，再研究全实际curve certificate且miss必原CCD fallback，不凭旧25收益外推。

- v40现场receiver Entry/Channel已接并由Root独立同bin实际复跑exit2/42：844seal SHA全核，42完整非CPU树与43失败逐IEEE同、输入相同/hash连续，仍无capture。Root精确重构h31 Entry offset .004797418107200244与h37 Channel offset .005930774159327753：PV与前一真实提交F32完全同，二次式lo>0/hi<=0、整piece严格下降证明首唯一root；generation各加1。Channel raw事件点对全部2364观察边界exact距离≥全R，inside exact ray=1，min差与producer逐Fraction同。42×16×9运动分量与Root无event v36逐bits同，phase/generation/hash版本不同不声称跨版本全树同。history收据存在于18–42并逐行next_state_sha匹配，1–17旧cloth schema没有该收据，不冒称有。它是H0−5cm的receiver inlet事件，不替代h18真实H0支持丧失证；观察cap不进CCD。证`docs/evidence/20261016-root-live-receiver-v40-review`。
- Root查出当前原始轨迹日志交付仍有缺口：ticks中的contacts/solver_events仅数量、all16_complete_h_partitions仅bool；实际生产telemetry的逐ball segment/offset/dt/generation/PVA/end与typed contact/departure/solver/capture必须在后继新目录完整输出，不能用summary当原始日志闭合。已把既有验证通过BGLL15 codec准确路径`docs/evidence/20261016-numeric-log-grammar-v15/src/lossless_log_v13.h`（magic确为BGLL15）和decoder/已过grammar证交B复用，不重复开发旧wire。
- A新42 v15 immutable topology同bin on/off已冻结，开发实跑published42约9953/13098ms，完整cold/析构/进程以及失败43分别记录，均仍严重FAIL；旧/新ctx与生命周期负例已过，未改B主源。下一v16先actual accepted h40–42 whole-fine几何证书实测排除比例/构核成本，43真wall接触作不得删除负控，任何unknown/identity/子path包含失配fallback原CCD，不先声称可达2ms。

- 首个新P0完整物理终止切片v41已由Root独立同fault env实际跑通exit0/43：858seal文件SHA核过、43全树/输入/Entry故障回滚除CPU逐IEEE同、连续hash；31故障staged Entry未发表，恢复原published30/hash/history后同Runtime重试，retry31终态hash与原未发表31相同。先31receiver Entry、37Channel、40斜floor、43真实floor+wall联合耗能后，collector在43 offset .0011887237372234395执行明确新实验吸能law。之前42无隐藏，最终cue原样F32 P(−64.8364105,73.4442520,−128.3337555)、v/ω由law归零、pocketed/Captured；行政复位未做，目标球收集尚未实现。严格fullR containment仍false，已有active-contact数值容纳true，未改R/P/固定epsilon。
- Root用Fraction独立重构terminal动量/绕world origin角动量吸收、raw/cast KE、原joint能量allowance及实际cast L1接触预算：Kupper1.2483215669e−9J < 保守任何球体部分回inlet的能垒.08639421893J，保守高度5.182210988cm；全数值逐Fraction同receipt。law仅当前passive固定receiver/其他15静止/无外部冲量切片，旧profile/规则/成绩/回放/普通产品仍拒。Root accepted-h max2881.204ms、43CPU1751.457ms、均344.4434ms，Entry注入失败另8.122ms，均严重FAIL。旧result scope尾句No capture success inferred未覆写，独立核实际terminal/receipt纠正其文案范围。证`docs/evidence/20261016-root-first-collector-v41-review`。
- 为原生接线完成真实CPU准备：Root源码`docs/evidence/20261016-root-native-runtime-seam-v1/render_seam_smoke.cpp`仅include已冻结v41 probe重命名main、链接同physics对象；实际正常43中45次PresentationSnapshot/CameraFrame读取前后hash相同，可见球位置/全部16pocket可见性正确，43全部state hashes和motion与Rootfault参考全同。只是CPU presentation seam，没有窗口/GPU/实际硬件输入，不作native证据。后继使用冻结v42 physics/headers重新编译graphics，避免混旧partial v7 ABI；保护正式binary不改。
- v42完整原始日志开发正常/双fault均exit0/43并seal00f6bf1b…：正常BGLL15 1META+43TICK+CLOSE；双故障30tick+FAIL/CLOSE→parentSHA12tick+FAIL/CLOSE→parentSHA1tick/CLOSE，真实capture后guard故障恢复42 seq0/flags/history/hash再重试43 seq1。1513生产analytic segments逐球h分区、6真实Support/5solver及Mouth/Throat/Capture各1、canonical state/rawF64 provenance已写wire；Root尚未独立复跑/逐段审计这版，不能凭开发报告关闭G3。B继续bodyIndex/真实目标球入袋，当前cue-only/15静止边界保持。


Root 后继独立实跑 v42 正常和 Entry/Capture 双故障均 exit0/43，核过898封存文件。沿用未修改的 BGLL15 decoder，323304个F64叶逐bits（含signed zero）正常/重试/wire/sidecar全同，仅精确排除exclusive_CPU_ms。实际1513 analytic segments、43×16完整h分区、688首尾位置、canonical状态SHA连续、6 Support/5 solver/Mouth-Throat-Capture各1全核；时间用逐项F64运算舍入界，不加固定slack。h43两Support与Capture同generation7→8为原子组合事件；双故障恢复30/42与history/flags/sequence/hash后同Runtime重试。与Root v41全部43×16×9 motion及终态hash相同。证 docs/evidence/20261016-root-full-telemetry-v42-review。该单角cue-only CPU完整日志切片PASS_SCOPED；目标球、六袋、native、规则校准与M4仍未通过。正常accepted h CPU均347.3017ms/P99 2940.421ms，双故障失败CPU2521.575ms另计，性能严重FAIL。


Root 后继首轮原生完整链实际验：独立v42 native driver重编30图形/桥接TU并链接冻结math，普通发行二进制SHA未改。保留unbundled工具绑定FAIL、Cocoa缺finishLaunching的AX FAIL、两次offscreen有图而实际窗口白屏FAIL；按实际Cocoa drawable prepare/update+default FBO swap修复，CUA实际像素已看到球桌。独立review显式 [ ]10度/J L1度键通过真实NSEvent归一化到生产MoveX；不称正式键位或产品相机修复。真实Tab/27次jog/Wheel1/Leftrelease Shoot/Space1/Return实际完成24h，25整步回滚。原始BGLL已用旧decoder独立核24×16=384分区/401段、32原生输入与Controller/hash关联、sidecar全bits同；失败25 hash恢复24，无capture。初始母球在画外，产品相机仍FAIL。证docs/evidence/20261016-root-native-full43-v1。
Root独立从该原生25实际ABC/PVA/R重构满R平面q五power系数全部严格正，且GID199552 native三角与冻结f748 PCK逐字绑定，余段无任何精确plane root；不是新真实碰撞，不能仅按old additional-contact失败文案判断。不改P/V/R或初始penetration门。B后继v48 CPU精确重放32原生输入已43通过，但actual face分支receipt未带出，后继v49补后再接原生；该CPU重放不冒称原生43通过。
Root独立v46同bin实跑真实目标1全52 exit0，973seal文件SHA全核；原始1873段/832完整球h分区/canonicalSHA链，258854正常对参考及sidecar浮点叶逐bits同仅排exclusive_CPU_ms，h1真实BallBall0/1，receiver Entry40/Channel47/Capture52均实际ball1；cue从未捕获，target到52才pocketed。strict fullR FAIL/原active-contact预算数值容纳实验law保持，未校准/正式规则未准入，六袋和native整体未通过。证docs/evidence/20261016-root-object46-full52-review。


Root 真原生 v49后继已完成并独立验收单角cue43物理链：核969 SHA并重编30图形/原生TU，同一32真实NSEvents/前后controls与先失败原生会话逐字同；真实Shoot before hash接首h，1513实际解析段/688球h分区/canonical状态链全部核，215840 F64叶actual/CPU参考/sidecar逐bits同仅指定CPU排除。窗口frames覆盖0..43且43捕获帧绑终态hash，CUA实际像素看到场景和tick43；原生正常Escape退出exit0/BGLL完整CLOSE。Entry31/Channel37/Capture43仅cue0，其余15未捕获。Root另核实际执行Face199552五power/五Bernstein与前失败原生实际PVA和f748 PCK三角精确同且严格正。证 docs/evidence/20261016-root-native-full43-v2。
该证据只闭合此隔离review单角cue全h native输入链。`[ ]`/J/L是显式检查桥接而非正式键位；初始cue和入袋过程仍在画外，真实产品相机FAIL；严格fullR FAIL/数值active-contact容纳实验law/未校准隔离保持。重步使AX一次noWindowsAvailable后恢复，性能仍严重FAIL；正式入口/规则/六袋/目标球native/M4均不因此通过。保护正式binary SHA未改。
P2 source v31开发报告280456面单闭体edge/link/zero0、8061118 exact候选非法0、六袋R+1mm通路过；Euler−40→−38的具体handle归因尚未独立审清，未资产化/GPU，不能混B stablef748。A v18纯几何fresh参数cache已封211972eb…，on/off仍旧36前缀且严重性能FAIL；后继先补adaptive负例，再移同49完整43/52，不把旧前缀性能外推。


检查点30后继证据：Root真实wallclock相机v4，仅调用既有m4CameraCandidate/AdvanceCameraPresentation并逐子步断言物理hash/tick不变；848 camera/frame配对、3592子步。真实同32键鼠输入43完整h/1513段/688球h，215840 F64叶vsCPU49及sidecar仅CPU排除逐bits同。9帧实际GPU关键帧中母球和P0入口可见，31部分球可见、37实际台体遮挡而43才pocketed；选帧整球投影均在viewport。只此候选单角低瞄/下降取景PASS_SCOPED，正式入口/相机控制/六景/性能OPEN。v3零tick预演closeLogs标complete是冻结49的未Shoot生命周期缺陷，不能算物理通过。证docs/evidence/20261016-root-native-camera-v4。
Root同bin P1v53实跑50 exit0，1083seal核过，1332生产段/800球h、202412 F64叶normal参考/sidecar除CPU逐bits同；真实BallBall0/1，typed PocketId1 Entry40/Channel46/Capture50。仅终态/原日志复现PASS，H0因果后继FAIL：Root全PCK4336个+Y/H0 native面独立Fraction足点核，P1已提交27在kind0边界后0..10µs仍有kind11平台118481支撑却按g落下；P2被拒27在0..4ms仍有121354/121356等支持，原fullsegment guard正确拒。B整段exact进一步量化P1平台约33.99µs/P2约5.431ms，P0也重新审。不得因50捕获关G3或扩大guard/排更多ghost绕过。证docs/evidence/20261016-root-pocket1-v53-review/independent-H0-platform-causal-failure.json。
第三角新common资产Root独立PCK/glTF全3LOD各280456有向kind面同源，source d952c400…/gltf f3014f6a…/PCK63c6aa5b…；Root实际看P2内斜/俯/外低/whole并复查P0/P1，方向连续但小齿/暗部/木纹/端部硬转保视觉FAIL，非六产品场景，不和stablef748物理拼PASS。证docs/evidence/20261016-root-p0-p1-p2-merged-asset-review及E/20261016-root-p0-p1-p2-merged-v1-gpu。
TA射线把小齿绑newGID115199/source113005/kind11旧SHAPE_FAIL面；完整exact finite clip证明P1/P2共512 H0 kind11面都完全被原author cloth_support覆盖107.381316mm²，未覆盖/重叠0，不是仅质心或可直接删的额外平台。已授权新namespace恢复真实cloth身份及平面UV，保几何点/切口/receiver；更大形态如仍FAIL另建模。B/A合作actual全H0 finite support atlas+明确law/material/真边release，未知材质拒；旧f748临时neck law仅研发，不永久维持错资产。P3小正体源冻结优先此修复。A v19 component压力封61f99ef7…：spin+.5小grid失败后16/32过，phase回128/256，cap coarse1..4096失败/fine8192未执行；不称native/性能过，性能栈延后先Atlas。磁盘1.8GiB，旧CMake.o只读统计62MB，未清理旧日志或证据。正式binary保护SHA保持，Phase6 OFF，8人研究0/8待版本就绪。


检查点31：Root独立Fraction全512有限面重裁切，完整覆盖107.38131642283406mm²、53060个positive clipped-polygon对overlap0，不复用TA算法或中心点推断；原体e4085350…与current d952c400…SHA精确绑定。证docs/evidence/20261016-root-H0-source-identity-review。TA新恢复candidate de395920…/gltf73ceb6f4…/PCK6ee23284…/namespace original_chinese_H0_cloth_source_identity.v1 已冻结8a40aec6…；Root实核64handoff SHA，XYZ及triangle_indices完全逐字同d952，仅且恰已独立覆盖证明的512面11→0，三LOD各280456有向kind与source/PCK逐面同。证docs/evidence/20261016-root-H0-restored-asset-review。
Root实际GPU看新身份P2俯/斜、P1俯/斜与P0斜五帧，meta逐帧绑73ceb6f4…，旧P2俯同相机(489,412)褐色小齿已不见，绿色台呢衔接改善；这是恢复真正材质/支持域而非扩大孔/改XYZ/R。仍不整体视觉通过，6个material0域pinch需与全actual H0支持union区分，TA逐点原生扇/边/上下高度定位中。证E/20261016-root-H0-restored-v1-gpu/root-actual-restored-pixel-review.json。
A NativeH0SupportAtlas v1封bea68b06…，29files/6.51MB，旧4336 H0面/oldP2 fullsurface quadratic真实union出口.00651052792460；此前B.00650718548来自错误过早停滚阻后的constantV/gravity反事实piece，不是同实际曲线，不混比。UNKNOWN121354守拒，几何symbolic正控不作law准入；新4341面/新ctx/明确cloth binding必须重跑。B新L55 namespace loader正消费de395/6ee，仅显式适配cm/semantic材质元数据，旧namespace/重放/训练/rules仍隔离。B接连续新H0 law：同surface regime一次提取a/alpha，不在材质界重新施加末ω投影，P/V/ω左右连续，真实phase/rolling误差预算超限仍拒，真有限支持全离开才gravity；新公式/内核身份独立。新版完整h/六袋/性能尚未跑通。P3小源暂冻，优先此因果修复。

## 7. 2026-10-19 后继执行切口（不替换前述验收门）

表格中的恢复状态是历史记录；当前以检查点32及后继证据为准。G1–G8仍没有整体通过，R1–R7均开放。用户已确认可以安排5名新用户和3名有真实台球经验者；G5/G6形成可测版后再通知，现为0/8。

| 次序／责任 | 下一交付 | 明确验收与失败处理 |
|---|---|---|
| 1／C++ B、PM复核 | 连续真实H0支持的新源实际击球与启动预检 | de395/6ee同一冻结源；真实Controller击球→完整固定步→真支持边界→下落→receiver阶段→收集；逐步16球/完整轨迹/来源和hash链。预检在任何玩家输入前实际读取绑定资源，状态/历史不变；不击球必须0输入/0tick并关闭为`closed_without_accepted_shot`。Entry/Capture两处故障同Runtime回滚后重试，全部非CPU数据逐IEEE位相同。P2 v59已由PM独立通过局部；v60后继独立复核，不推成六袋。 |
| 2／技术美术、PM GPU | 四角自身原始结构＋中袋身份和形态 | P3新312230面候选独立核三LOD/PCK/source全等并实看俯/内斜/外侧近景；与B稳定物理源不同，不拼通过。P5剩余272面必须完整有限原作者台呢覆盖后恢复，不能按kind批改。全桌拓扑/相交/原生通路仍重验。内壁黑、鼻口硬转、木纹/包边接缝继续记失败；像素可看不等于真实击球已过。 |
| 3／C++ B | 新源P0/P1及后续六袋相位覆盖 | fresh native receiver映射/源/材质注册，不按旧GID复用事实。每袋复用前述中央/偏口/沿库/擦鼻/明确未进路径矩阵；低速停止、NoExit、Rolling/Sliding/自旋停止及相位转变必须真实分段，当前不支持即拒绝，不以均值角加速度冒充整段自旋轨迹。矩阵预先冻结，不靠事后删失败凑通过。 |
| 4／C++ B、PM规则 | 真袋嘴与receiver事件正式合同 | 真H0支撑出口必须动态绑定原作者袋口／PocketId，并区分袋口、出台和未知。当前carrier `mouth/throat`实际位于H0下5cm的receiver入口，不能直接供正式rules/audio/replay解释为台面袋嘴；新命名/enum或版本迁移须有明确映射、旧版本拒绝／保持原解释和正负例。完成前普通规则／成绩／回放准入继续关闭。 |
| 5／C++ A、PM | 可归因性能优化 | 已测边界链减少求根，但完整单查询2.336ms、冷构建13–18s仍失败。先拆分冷构建、精确谓词、分配、证书和写盘，再按测量选择优化。不可变预计算资产须绑定源/材质注册/builder，运行时验证和读盘成本照算；未知/冲突/非共形回退原算法。最终完整生产事务仍用G4的2ms／16.7ms门，不采用单查询替代。 |
| 6／PM | 新源原生窗口复验与产品闭环 | 先把同一冻结新H0物理接入隔离窗口，以真实键鼠击球并核完整步、窗口帧/相机与CPU真实输入重放；相机每步不改变物理hash/clock。此结果不替代正式现代入口、六袋/六产品场景及R3–R7。技术条件就绪后再形成8人研究包，随后同一冻结产品全量回归／发行。 |

所有strict full-R receiver containment失败、候选数值接触容纳吸能law的适用边界和未校准状态并列保留。不得更改球径／强行投影／击球后改球状态／只加下降动画来获得通过。保护正式二进制和旧证据不改；Phase6保持OFF。

## 8. 2026-10-19 数值摩擦相位实施切口（检查点43后继）

本节落实第1节“strict GMP/ODE保留离线；数值候选另立身份”，不替换G1–G8或原Phase0–5退出门。当前低速P1仅完整53步；第54步尚未通过。D79三个网格仅对各自实际数值二次片段证明静锥函数单调且存在唯一根，严格物理ODE误差、整个h的首事件和真实滑动准入仍未证明。

1. **C++ B：冻结数值相位合同再运行。** 另立formula/law身份，明确实际数值轨迹、相位参考与raw状态的关系。预先写出根区间、细化、方向初生、关联误差和F32存储的预算及累加规则，不能事后把实测差塞进界。旧profile、规则、训练成绩和回放不迁移；新合同尚不晋升默认。
2. **C++ B、A辅助：受检的关联根对象。** 对象必须由实际verifier构造，强绑定geometry/source/material law/profile、片段P/V/W/a/alpha、clock/state/generation和完整根区间；证据包含端点异号、全片段导数符号、此前片段有效性以及native owner/法向失联/其他接触竞争顺序。失配或未知拒绝；根字符串、近似M值或任意token不能替代这些检查。
3. **C++ B：实现incipient到Sliding。** 使用明确关联的数值根参考和完整几何/RR的一侧导数，传播root/reference/raw不确定性。fixed-piece函数导数与后继物理场jet分开记录。不得将原raw滑速、角速度或球心投影成理想状态，不得扩大已有支持/碰撞/法向/能量等门。完整h与全部附加相位历史原子提交；任何失败回滚后仅保最后完整tick。
4. **PM独立验收：完整路径优先。** 同一冻结候选真实Controller击球，逐步核16球/轨迹/来源、有限支持、整球碰撞、相位连续性、功/能量及原有误差门；正常与相位/入口/收集故障重试全日志逐IEEE一致（仅指定CPU字段除外）。低速中央路径到静止或真实收集后，按G3原定六袋中央/偏口/沿库/擦鼻/未进矩阵扩展，不以单一相位组件或候选根替代。
5. **C++ A：实时结构路径。** 当前v8和v9分别有独立完整语义证据，但未叠加且仍远超G4。先对新72完整事务低扰动归因，再实现可归因的数值筛选与原算法fallback；原完整guard/规则/详细日志成本照计。数千微段的研发成功不作为实时版本验收。

数值相位实现通过仍只是工程候选。误差适用范围、收敛与物理校准、六袋/多球、正式入口、旧数据隔离、同源全回归及真人研究必须分别达到原门，才可申请晋升和最终M4。严格ODE未证继续标NOT_PROVEN，不能借新命名冒称严格证明；也不以无限追逐解析精确零阻塞数值实现。


## 9. 2026-10-20 执行状态与下一可验证交付（检查点56）

原G1–G8/R1–R7退出门不变；靠前状态为历史记录，当前以执行记录检查点56及其独立证据为准。当前107 P4中袋真实Controller完整62步已由PM独立核日志和终态，但严格raw/stored整球间隙均负、性能严重失败，不能标袋口整体完成。v3五类声音已由用户选择为可测候选，最终发行/权属待确认，不再重复询问此候选选择。

| 责任 | 下一交付 | 可验证验收 |
|---|---|---|
| C++ B、PM | 当前107自身Entry51/Collection62故障回滚及其余中央路径 | 同冻结107/新102初态/原速度带，真实Controller到完整静止或收集；全部16球原始轨迹/aux/来源/hash连续。故障恢复最后完整步的全部状态/aux，同Runtime重试非CPU逐位同；新面→边50未有故障hook要明确未覆盖。P0–P3角袋，P4/P5中袋，不借旧版本局部结果拼六袋。 |
| C++ B、PM、TA只读协助 | 107负间隙最早来源与数值容纳边界 | 由真实raw与cast收据定位首次floor接触和首次负间隙，精确重算所有active/inactive有限面、实际接触预算与被动收集能量。严格fullR失败并列保留；不得末端挪球、缩R、增epsilon。新的严格候选需独立表示/版本和每步raw/cast实际guard，未过则拒，不能借名称覆盖失败。 |
| C++ A | 高阶有限特征v5首事件竞争 | 全981预声明曲线，fresh exact参考与fast分支全比较，未知owner/边界/接触必须fallback；事件排序、首事件与原相位正确后才考虑Runtime接入。实际完整guard/日志/提交计入G4，不能用局部查询代替。 |
| PM、技术美术 | 显式4315台呢mask消费与六产品景 | 真实C++ CPU draw/node/mesh/primitive/index/nativePOS digest验证后，三LOD各4315；实际GPU normal/mask绑定同构建/资产/相机/状态，未列TABLE=Other，母球/球/场景身份保留。旧固定ROI门和SSIM基线批准独立，不按RGB猜分类、不把静态五景替代post-shot。 |
| PM | 原生相机与v3候选声音可测集成 | 桌面解锁后真实键鼠1°/P-O/视角过渡、实际加载并播放所选五类v3声音，保独立设置与详细日志。相机普通旧physics候选不与107 CPU拼产品通过；完成同源新物理接入后才形成8人研究版本。 |

资源门：磁盘真实可写预算先验证，保退出收据reserve；现约0.3GiB，待用户释放至少5GiB后再安排新的大型主构建/GPU/完整长日志。主构建/GPU仍仅PM持有。仅清已授权、可重建编译产物，旧证据/日志/截图/资产不删除。R1–R7全部开放、研究0/8、Phase6 OFF；只有同一冻结产品全部退出证据齐备才完成goal。


## 10. 检查点57后继（2026-10-20）
G1–G8/R1–R7门不变。当前444显式4315台呢分类消费者已actual CPU/独立现代候选GPU通过局部验证，仍只有两次独立调用，不是同帧MRT；felt Y=.087265<.12、EV=1.940397<2保留FAIL，SSIM/正式六景/新物理postshot未过。旧95性能v5组件Root已实跑复核，但0 Owned正根覆盖、冷加载约2秒，不准入当前Runtime。当前107双故障未启动（真实空间预探失败），P5尚未物理击球。108细粒度exclusive计时仍SOURCE_ONLY。
下一交付继续：先恢复真实可写预算并补当前107自身故障/六袋中央路径及108计时；接当前GeneralFixed/prephase快图；复用原8300lm候选与当前444独立组合做同源曝光诊断（未组合，不改原阈值）；把同帧分类、正式六景及原生键鼠/新物理产品接入闭合。连临时脚本/小seal已发生ENOSPC，已授权可重建缓存只释放1.11MB用于保全证据，不能当大型构建恢复；等待用户释放至少5GiB与桌面解锁。0/8真人研究尚不通知版本就绪。


检查点57再后继：现已把原8300lm rig与当前444正确组合并实际渲染一个overview（仅rig通量与manifest rigSHA两项文件内容改变，几何/PCK/其他资产不改；所有源与主程序保护hash保持）。feltY .1226165、EV2.06424、cue/felt2.78293只在该对象诊断范围过参考门；cue比6200下降17.66%、低aim/袋近景/六景原ROI/同帧分类/SSIM仍待验。不改原退出门。3个旧无引用测试编译物共27.8MB已核后清理，只支撑小工作，不等于完整构建/长日志磁盘恢复。当前整袋物理及同版本矩阵优先，108真实计时与保全部原始字节的流式日志存储后端可行性作为小源码切口，禁止用更少日志/关闭guard冒充性能或验收。


## 11. 检查点58后继：磁盘恢复与真实窗口复验（2026-10-20）
原G1–G8、R1–R7退出门不变，全部仍开放，Phase6 OFF，8人研究仍0/8，不通知版本就绪。用户确认释放空间后，实际约17GiB可用，Root以128MiB随机非稀疏写入/fsync/SHA回读及64MiB保全reserve通过容量探测；旧“仅0.3GiB/等待释放”的阻断已解除。旧证据保留，不再为此继续清理。

- **已取得的相机局部事实**：独立原生capture-guard v4同bin `b3d79d47…`、当前444/8300资产，真实窗口run03正常退出0。训练Observe中Right一次实际方位角+0.999984°，Left恢复到F32舍入范围；对应capture游戏状态SHA一致。训练P被原有play/aim边界明确拒绝；进入单机Aim后P精瞄、O俯视、O返回低瞄均有实际稳定backbuffer及同GameStateSHA。原始NSEvent日志独立审核待回执。O过渡中实际看见灯具短暂遮挡，最终稳定俯视清除；过渡门继续FAIL/OPEN，不用最终截图反证。仍是旧interactive v5物理，不能与private107/109证据拼成产品通过。
- **存储和计时v109已从SOURCE_ONLY推进到有限实际证据**：同mainbin `83cb861c…`，资源 `e19621bb…`；plain/gzip零输入、2段raw父链、容量/错误拒绝与3完整步Controller击球通过局部审核。plain/gzip/计时ON-OFF的23,580浮点叶逐位一致仅覆盖前3步。Root正在以同bin执行P4 Power40完整normal-on，64MiB真实保全reserve持有到进程结束；Entry51/Capture62双fault尚未跑，不得把3步结果替代完整62步及36–43热点等价性。
- **当前107有限特征组件**：h62真实正接触查询两条壁面，原路径/快路径/fresh MPQ shadow逐位相同；2,358严格排除、393未知保留原路径，两个无效绑定全部回退。Root实际重跑只读验证器，28源证据、133旧对象及原BGLL重解码通过；不是完整Runtime性能通过。完整球体局部R的纳米级负间隙失败仍保留，不放宽验收门。

下一执行顺序：完成109正常路径与原107物理字段/全aux/日志父链比对，随后同bin双故障；针对36–43真实热点推进当前StageOperator保义提取及少片数数值候选，保全原guard/fallback/计时成本；使用已预声明两景normal/mask配对与原F32指标适配器补低瞄/P4袋近景，并处理相机过渡遮挡。其他11中央及96偏口/沿库/擦鼻/未进矩阵、同源新物理产品接入、正式六景、正式入口、规则/音频内容与权属、冻结回归/发行和真人研究仍分别验收。

检查点58更正（原始NSEvent与操作回执复核）：前述“单机P/O”仅表示画面切换事实，不是功能通过。seq74648 Tab、74673 P、77345/83938 O的input_after handled=true，但ok=false、error=product_match_replay_initial_authority_mismatch。截图相机改变及同GameStateSHA不能覆盖这项真实失败；相机/回放初始权威状态边界继续OPEN。训练Right/Left各一个KeyDown/KeyUp、camera_orbit回执ok=true。保留初步观察与本更正，待定位报错后仍改变presentation的事务原因，不放松authority门。


## 检查点59（2026-10-20）：完整109故障重试与开局产品边界

同一109冻结mainbin已经由Root实际运行P4 Power40完整normal及Entry51/Capture62双故障，分别退出0（172.573s/181.200s）。Root原BGLL15 decoder独立核62完整步、124完整aux/combined SHA、992球步分区、18,485生产解析片段；三段50/11/1容器/raw双SHA及raw父链通过，883冻结引用及保护主程序hash不变。B进一步核normal/retry全accepted非CPU typed全树与1,803,756 F64叶逐位同；107→109全62 canonical物理状态及数值/Qscalar比较同，身份/schema并未冒称等价。Capture有立即完整aux回滚证据；Entry立即时点仅physical+完整nativehistory，完整aux在retry.before核同，导出缺口继续保留。Root结论封在`20261020-root-current109-normal-and-dualfault-conclusion-v1`。strict fullR仍FALSE、普通规则/回放/训练/成绩/产品准入关闭。真实热点归因WholeInspect GMP53.867s/GeneralFixed29.597s，性能仍FAIL；并发时段明确只开发归因非隔离benchmark。

原生run03发现training→Match规则revision/effect遗留造成四项可信初态不匹配，后续P/O相机已动但成功回执被粘连错误改写。私有canonical新会话修复保profile+四项严格绑定，真实ProductSession CPU四正四负通过，并保物理/semantic generation递增/音频去重/AI与输入训练新会话边界。Root生产3TU -O3构建b44ba9…后，原生run02实际从训练返回新开局并执行Tab/P/O/O；Root已读到8个按键down/up回执均ok=true/error空，六backbuffer SHA核同源，match四帧GameStateSHA同。全原生日志链/语义仍独立复核；这只修开局边界，旧v5物理不可与private109拼产品通过。run01因Root设900超过原600s上限而被正常guard拒绝、CUA随后额外启动的无参进程已单独停止，两者保留不并有效证据。极限附加测试发现v2遗漏protocol resetGeneration maxguard；独立v3已补并实际19负控及同CPU正例通过，尚未并入Root原生二进制。视角过渡灯具及俯视powerHUD遮袋仍OPEN。

视觉继续保真失败：两景normal8300/mask6200四调用已实际闭合，随后单窗口孤立v2复采；3帧raw字节相同，P4normal48像素最大1级差保留。低瞄object cue/felt3.60229仅诊断通过，原固定及F32局部ROI失败不抹；两景object EV .39231/3.23964非正式break固定ROI，不能替代正式六景。实际旧returnlight命中无mesh的placeholder，真正P4 body16/4871面未消费。标准有效克隆材质保factor[1,1,1,1]、原纹理/几何/PCK，仅有限P4三LOD绑定；Root显式线性gain1.5实际GL读回已证，但口框contrast只1.00789→1.00871<2、仍暗，未推广。后继有限18cm/max.4单显式锚点返光仅独立候选；默认及物理不变，必须再实际GPU/像素验证，不能声称真实GI。

G1–G8/R1–R7门均不变；全部仍开放，Phase6 OFF，8人研究0/8，尚不通知版本就绪。下一实际交付：StageOperator保义提取/热点少片数、v3会话guard与原生复验、有限内壁返光独立A/B、俯视HUD真实尺寸布局及过渡逐帧。SSIM基线、六袋/偏口等完整矩阵、同源新物理产品、正式入口/规则/音频权属/冻结回归/发行继续分别验收。


## 检查点61（2026-10-20）：完整回退、原生遮挡复核与当前阻断

原 G1–G8／R1–R7 验收门不变，全部 OPEN；Phase 6 OFF，真人研究 0/8，尚未达到可通知版本就绪的条件。以下更新替代旧恢复检查中的进行中状态，不覆盖旧证据。

- **完整物理回退已实际复核，尚无密集轨迹生产准入**：Root 已跑 current113 的完整 P4 Power40 路径，62 步退出 0，实际 wall 163.896789 秒；原 BGLL15 解码器独立核 124 个完整 aux、992 球步分区、18,485 原生解析片段及容器/raw SHA。B 核 1,803,756 个非 CPU F64 字段与109逐位相同。仅 h37–43 进入候选检查，最终全部回退；denseChosen 全 false，不能写成新轨迹已接入或性能通过。snapshot／fingerprint／restore／sidecar 成本尚未分项。证据：`20261020-root-current113-full-live-fallback-v1`、`current113-live-readonly-audit-v1`。
- **数值组件有局部进展**：当前输入 F64 三门筛选 468 区间中 464 严格通过、4 保留 GMP 回退；676 个负控及原算法 shadow／独立 Fraction 复算无差异。Root 已实际重跑，仍只覆盖径向、正反力及法向速度，不能据此跳过完整 whole、phase、energy、动态球和 sceneCCD。旧111的 incipient 节点曾复用记录中的 midpoint q，不能作为自洽积分证明；116须从真实段起点和 qdot 自洽收敛并重新验整步。
- **严格整球空间与收球能量失败保留**：当前109最终捕获点的严格 fullR 为 false（约12–23nm负间隙），未改半径、坐标、阈值。h57–61提前收球可行性只读复核为否：h57–60动能仍高于同源逃逸能垒约0.1616J，h61碰地后仍高约0.02743J。不能用提前隐藏、任意耗散或动画替代修复。
- **70帧相机证据的范围纠正**：v4真实窗口取得 Tab 12、P 7、O进入26、O退出25，共70帧，全部 raw／expected SHA 和280项实际权威初态字段一致；三次窗口均退出0。但该版曾在全部候选过渡隐藏灯具，超出原资产显式 tactical_overhead cutaway 策略，因此不是可信的无障碍相机路径通过；当前 v5 已恢复原027渲染策略。真实相机与过渡遮挡仍 OPEN。证据：`20261020-native-v4-three-runs-readonly-review-v1`。
- **小屏遮袋仍需 GPU 复验**：v4 的1280×720/uiScale1.35原始帧里，顶部 player/context 分别遮挡远端角袋保护区1248／1326像素²，保留 FAIL；后来变大的窗口截图不充当1280通过。技术美术已做完整顶部侧槽候选，两TU私有构建 v5 `966d7eb5…`；Root 实跑18组实际 Menlo FreeType SDF 字体/图元检查通过，仍不是原生画面通过。底部tip原未画出的padding1px越界和旧过渡布局范围均明确保留。
- **v5第一次原生尝试仅启动与空闲**：Mac锁屏，未成功输入；程序303.161181秒退出0。Root 独立核完整日志父链／SHA／连续序号共71,342事件，全部球速和角速为零、无input事件，只有startup原始帧。该尝试标 NOT_RUN_LOCKED，不算三分辨率或袋口验收。实际检查桌面仍锁屏，已请求用户手动解锁；CPU工作继续。证据：`20261020-root-checkpoint61-reconciliation-v1/ACTUAL_V5_LOCKED_ATTEMPT_AND_PROTECTED_BUILD_AUDIT.json`。
- **光学候选失败与鼻端判断收窄**：有限内壁返光实际GPU改变3860像素，但口框contrast仅1.00973<2，未推广；不再靠弱增亮变体声称通过。当前P4可见鼻端同源量测证实为曲面，native弧长3.567470cm、鼻口高3.630005cm，转弯集中在入口前几毫米；隐藏接缝不能判为可见硬缝。中式中袋内侧双鼻的可靠实物近照仍缺，不拿角袋或美式WPA尺寸直接套用。

后续执行：先完成116自洽节点与完整h37全部门、保守快筛实际消费与完整回退成本；桌面解锁后用同一v5执行三分辨率原生稳定帧，分别核实际viewport、字体/所有HUD和六袋保护区。之后仍须完成真实相机路径、严格整球收球／实时性能、其余11中央及96偏口/沿库/擦鼻/未进矩阵、同源新物理产品与正式六景，再闭合入口／规则／音频权属／冻结回归／真人研究／发行。保护主程序 `build-product-session/Billiards` SHA `44374e01…`本轮核同，未覆盖；旧日志与证据未清理。


检查点61补充：Root已实际重跑 whole-needed-fields v2 冻结组件，exit0/wall5.246698s；468区间464严格通过／4原算法回退／728负控／零差异。旧v1的+Inf质量域漏洞原证据保留，v2明确拒绝；四个剩余精确门的Q端点复用尚不等于生产完整步通过。Root另从当前glTF完整祖先变换实际解码21灯具／832顶点，48个预声明终点组合中13个横置长台构图满足灯具下方半空间分离和完整台体4%边距；仍仅CPU终点设计，当前旧Overhead距离下限不允许该姿态，必须新身份、实际StateMachine与完整过渡／HUD验收，未改v5。见 `20261020-root-visible-fixture-underlamp-overhead-endpoint-v1`。


检查点61物理后继：B已实际完成current116真实h37目标球128段完整PVA链（7.451271 CPU秒，过程wall12.021110秒），仍UNPUBLISHED。Root从原始cubic系数/m/I/g用独立Fraction复核640个能量字段、1143个段间P/V/W位字段以及完整h分区通过；不冒称独立重导native、q/ODE、orientation、cast或全16球。代码复核发现other15静止门漏验vy；实际母球vy=1.0034132949420223e-16，不能改成0或称严格静止，旧128正例仅保目标链，完整被动球更新仍待补。原profile/parser拒例保留。证据 `20261020-root-current116-target128-readonly-audit-v1`。

Root再次actual重跑冻结116/128链exit0，CPU7.729494秒；除唯一exclusive_cpu_s字段外8192 F64位、53703字符串、8689整数、1411布尔全同。新v117仍在实施，不将此目标链证明升级为完整Runtime；桌面锁屏等待手动解锁，新的2560-after-unlock01输出计划已准备、未启动。


## 检查点62（2026-10-20）：解锁后三分辨率原生局部门与118真实回退

G1–G8／R1–R7退出门全部保持，M4未完成、Phase6 OFF、真人研究0/8。用户解锁后已实际继续，不再把旧锁屏启动当有效验收。

**稳定俯视四HUD／六袋保护区局部门通过**：同一v5二进制 `966d7eb5…` 在实际1280×720/uiScale1.35、1920×1080/ui1、2560×1440/ui1完成真实训练返回→新单机开局→Tab→O，原始GL_BACK稳定帧、真实窗口像素及自然退出0齐备；wall分别102.064065／77.375981／81.073120秒。Root核三运行连续序号、完整segment父链和raw SHA、全部input_before/after同源且ok、零球速、capture相机与actual presentation逐位同，合计70,662事件。每次最后Quit的Enter KeyDown后窗口结束，未虚构其KeyUp；input前后回执配对与按键down/up是不同概念。三个manual capture的GameState SHA均 `564c1393…`，但未采实时authority hash／四项INITIAL字段，未回填。1280初始窗口一度2704×1630，经真实AX zoom恢复1280后才采有效稳定帧；大截图排除，不推断缩放原因。

TA独立按同source/F32/stableUpZ投影六袋guard，4panel×6guard×3景共72交叠均0；最小留白81／117／157px，分别≥24／36／48px。实际九字段、10格力量条、1280高缩放分行均保留；18组realSDF CPU证据与GPU画面分列，未谎称GPU逐primitive glyph/scissor读回。旧tip未画padding1px与原过渡布局边界仍保留。这只关闭稳定Overhead三res的四HUD局部门；暗内衬／相机过渡／原tactical cutaway／真实完整六景／新物理／正式产品入口／M4不外推。Root seal `20261020-root-native-v5-three-resolution-actual-review-v1/ROOT_NATIVE_V5_THREE_RES_LOCAL_GATE_SEAL.json`，TA seal `20261020-native-v5-all-HUD-three-res-TA-readonly-review-v1`。

**Root原underlamp终点建议必须更正**：Root只读48组合使用worldUpY；真实sharedLookAt在abs(forward·worldUpY)>.98改stableUpZ。按实际投影48/48均裁台，首个NDCy约±1.617225；旧13/48仅是尚未实现的显式worldUpY/roll合同诊断，不是当前可消费构图。未改旧报告／seal，已追加 `ROOT_UNDERLAMP_ENDPOINT_SCOPE_CORRECTION.json`，不集成该建议。若以后另立横置构图，现侧槽也会挡四角，须sceneViewport／HUD／guide／pointer／ROI同一合同，不凭yaw改动宣称修好。

**完整生产入口仍明确拒候选，真实回退已扩至118**：117首次实际因小JSON资源上限exit74，原partial gzip经实际解码EOF拒绝、保留失败。118沿用同物理，候选完整typed载荷改专用CheckedGzipSink（独立64MiB cap，原BGLL另64MiB cap），不扩大smallJSON限。实际kernel CpuTransaction/LocalFlightScope内完成target128/256、原15球支撑／H0与非H0CCD／105对fullR及9个prospective F32 cast；母球非零vy保留，超原预算vy负控拒。新cubic directed-cast／radial／sigma aux账本与typedouter消费仍未实现，候选明确拒并提交旧完整h37，dense未发布。

Root已另用461冻结引用核同后实际运行118 bounded37-on，子进程exit2（37步仍moving的预声明停止，不是完整shot成功），wall62.623117秒、candidateCPU23.611226秒；原BGLL decoder闭37，RootON与冻结同bin OFF所有非CPU typed树322,016 F64叶及74份aux逐位同。候选raw34,581,611B／gzip11,308,880B双SHA/close核同，before/after完整aux与主wire绑定。只释放本轮预声明自建8MiB临时收据reserve，旧日志／证据未删。Root seal `20261020-root-current118-bounded37-live-review-v1/ROOT_ACTUAL118_LIVE_REPRODUCTION_SEAL.json`。119正在补真实guard与消费，不把实际回退成功写作新物理提交成功。

**性能仍FAIL**：A实际whole消费者off/on/shadow目标128链7.19525→6.73677秒；完整childCPU11.410710→10.690196秒，实际8192 F64／1024原GMP shadow及独立Fraction核同，但剩余精确四字段仍4.290301秒。它是冻结116目标链对照，与118真实Runtime不同ctx、未集成；禁止拼接成整步加速或实时通过。

**袋口光学取样已定位但旧失败保留**：TA逐像素/真实source/body/材质确认旧P4 mouth ROI有约74.8%落台呢／覆呢面，内衬仅约9.2%；返光让内衬Y .00233476→.00400332（约+71.5%），但内衬原仅贡献inner总亮度约.2518%，故原全框contrast只1.00788954→1.00972637。不是GL未消费，也不据此废弃旧门。后继建立可追溯author aperture／liner／nose／collar／felt分类测量与旧ROI并列，未注册区域Unknown，阈值和SSIM批准基线不擅改。新的7张厂商实际照片仍未找到可信中式中袋双鼻内侧近照，不把角袋／全台／美式照片当此几何尺寸证明。

下一实际工作：119全cubic误差/方向/径向/sigma账本及typedouter准入，A剩余四字段保守快筛实际消费；TA袋口ROI目的与source分类/原创材质光学方案。严格fullR最终收球与实时性能、其他11中央及96偏口矩阵、同源新物理产品、相机完整过渡、六景/SSIM、R2–R7仍分别有待闭合。默认构建及资产未推广，不能因三张清晰画面认定袋口整体修好。


## 检查点63（2026-10-20）：真实密集步、完整P4终态与仍失败的产品门

G1–G8／R1–R7仍全部OPEN，M4未完成，Phase6 OFF，真人研究0/8，尚不通知版本就绪。旧失败、旧profile及保护主程序不覆盖。

- **新轨迹已实际提交但只在私有数值候选**：119起真实h37提交256段目标球cubic＋15球原支撑更新；120完成typed外层拒绝后重算，但首次coarse诊断有覆盖缺口，Root未把120故障当通过。121改为每callback立即保全coarse128／fine256／ledger／aux，Root实际normal59.505218s、故障107.312227s；全37步162204非CPU F64叶逐位同，完整aux/native/typed history恢复、原始父链、第一失败及第二真dense提交均核。Root独立重构74份aux/combined、typed carrier、全16球h分区、9个实际F32 cast及Fraction存储能量。证据 `20261020-root-current121-normal-and-outerfault-v1/ROOT121_FINAL_SCOPED_SEAL.json`，仅h37事务门通过。
- **完整P4真实Controller击球已跑完，仍严格FAIL**：122不再硬编码h37，Root实际62步退出0，wall242.526899s。h37–39真dense，h40因static friction cone耗尽明确拒绝并走原完整步；原decoder复核992球步、13109真实片段、124完整aux/combined、4callback原始父链和38,459,179B压缩session预算。实际Opening/Handoff36、Departure43、Entry51、Channel57、Collection62；终态到达不能覆盖strict fullR=false。B因果复核首个碰底61的F64末端尚在合法侧，首次F32存储引入约−1.223e−6cm，62延续并再次舍入，最终约−2.296e−6cm。尚未修复，不减半径、不放宽全网格门、不提前隐藏或任意投影。Root封存 `20261020-root-current122-full-P4-real-controller-v1/ROOT122_FULL_P4_SCOPED_SEAL.json`。
- **性能仍远未达标**：A4目标链组件7.55794→2.52880s不等于Runtime。后继123真实h37 off45.500734→on31.146510 CPU秒；fresh GMP shadow、故障重试及物理字段全树复核通过的报告仍在封存，完整实时门FAIL。数学证书及其派生hash合法变化不能声称所有aux字节相同。
- **相机/HUD局部门与失败并列**：v6实际70帧、42组输入前后、280项INITIAL及camera/rawSHA核同；18帧采用安全四HUD布局，但ord38、52–57仍被灯具严重遮挡，fallback部分力量条/tip仍有交叠，完整过渡FAIL。新Match初始Observe刷新修复已在私有candidate完成4正例＋defaultOFF正例＋4负例CPU验证；旧首次OFF fixture缺version拒绝保留。真实新Match为legacy_v1，实测275cm/25°/FOV48，台体留白4.50%、相机低于灯具18.18cm，仅稳定Observe局部门；Tab只3个实际采样不能证明连续无障碍路径，不能与旧interactive物理拼产品通过。
- **同帧表面取证已构建，GPU尚未运行**：有限P4作者分类覆盖class／sourceFace／PCK GID，未注册区域UNKNOWN；Root私有MRT原生候选96cb285d…已实际编译链接，通过8个来源门CPU例。真实frame serial／相机／姿态／Runtime权威状态及LDR SHA绑定尚需GPU验收；新OFF/ON独立计划已准备。最新窗口工具明确锁屏，已请求手动解锁，未把准备或派发当完成。

后续优先：首次F32存储穿透的物理/表示边界修复与严格全半径复验、实际Runtime热点消除；解锁后MRT同帧来源和OFF/ON像素等价、实际MSAA分类/深度读回；连续相机灯具避障和旧ROI/语义测量并列。其余11中央与96偏口矩阵、同源产品物理接入、正式六景/SSIM、入口/规则/音频权属/冻结回归/真人研究/发行仍各自开放。只释放121/122预声明自建reserve，旧日志证据未删。


## 检查点64（2026-10-20）：解锁后真实GPU取证及126整杆失败因果

M4／R1–R7仍全部OPEN，Phase6 OFF，真人研究0/8，未通知可测版本就绪。仅继承用户已批准的13课与v3声音候选，不要求重玩，不推广当前私有候选。

**真实GPU同帧取证取得局部证据，完整精确对照仍FAIL。** 原整数MRT实际退出2：GL_MAX_INTEGER_SAMPLES=1而原产品MSAA4，draw/attachment上限均8；保留失败。新有限Float32整数传输绑定全部实际IDs≤323773（<2^24）、sentinel=-1，仍4x逐sample读取不平均；随后实际定位GL_INVALID_ENUM于indexed GL_SAMPLER_BINDING，改为当前active unit的glGetIntegerv。v5真实Observe/Aim两帧取得class/GID、fragmentZ、原始backingDepth、UI/chrome及真实Runtime的同rendererInstance/currentserial/精确camera与geometryPose/LDRSHA绑定。TA独立核60实际draw（球桌+同CpuScene的120灯具三角）、四sample类别与原冻结companion、UI强UNKNOWN；最初auditor漏灯具draw的拒绝保留，v2严格补全冻结193精确keys，未放宽world bits。fragmentZ为原per-fragment广播，不能当每sample原始depth，也不能当DOF/bloom后颜色唯一owner。

Root独立审核五个有效运行135824事件、七个raw帧、四个真实native/surface绑定，全部零球速／无Shoot、输入前后成功、原始日志父链及camera/source核同。但同bin同camera同GameState下v5 OFF/ON Observe660像素、Aim1314像素变化，max18/23；保留D24原renderbuffer的v6及original-color函数先执行的v7未解决，v7同binObserve仍660差异。不能升级为可靠正式无扰动测量或袋口通过。差异分布由TA按实际源ray独立归因，主要球体投影/venue声学板边；P4类2–5没有改变sample，但不因此废弃全图exact门。Root seal `20261020-root-MRT-native-valid-runs-review-v1/ROOT_MRT_LOCAL_SOURCE_PASS_EXACT_PIXEL_FAIL_SEAL.json`。

v7第一次按Return前未核实菜单，原有参进程退出后CUA连接到无参额外窗口；无有效采样、后续设置遮罩/尺寸变化均排除。CmdQ未关闭，随后实际AX close关闭；原错误记录及更正保留。v7第二次实际核菜单焦点/参数后获得有效采样，不混原失败。

**125/126真实Controller整杆继续保留失败。** 125零输入/负控不能覆盖首次真实shoot在h35后map::at异常；Root实际exit2/wall8.510891s并封存。126修私有头__FILE__推registry错误：显式原冻结registry出处/hash与原资源180引用，不补造键、不吞异常。短36实际跨旧报错但新floor/representation均0，只是局部修复。Root后续126真整杆62步exit0，outer wall363.488983s，target1自然collection，但strict fullR仍false。原decoder/canonical/aux/combined链audit已实际执行；61真实safe-side root数学通过后因stale nominal proposed-contact点与fresh safe pose不一致，原finiteFace门拒绝，完整aux/physical恢复并整oldh重算；62从旧RNE已负fullR起点进入，rawcurve又拒绝并完整回退。实际representation提交0。不得将自然终态或安全回退写作新strict算法成功；后继127B正在分开真实root-interval owner与fresh safe-pose Voronoi witness，保持原各误差帽。

**性能独立分支有实际组件收益，仍非实时。** A127四模式640fresh exhaustive shadow同、640强制失配完整回退；203164非CPU F64与全部非证明字段/派生SHA无差，native CPU12.136→5.429s。A128再将2048各自精确lo/hi的directed七字段检查接保守快筛，原expanded raw、phase、能量、outer都保留，freshGMP shadow全部包围原值、精确PVW同，2048 key失配全whole回退；audit CPU13.980→2.193s。不同证明上界/逐项派生SHA明确列清，不称所有aux字节相同。它们仍只有bounded37，且与Root GPU并行，observed wall不作隔离性能预算证明；未并入126、没有整杆/实时/M4通过。A127 seal97ee629c…，A128 seal5693fbd2…。

**相机下一步已有可实施合同，尚未Runtime通过。** 新灯具下方端点D125cm/FOV72°/screenUp世界X，实际C++ F32矩阵投影全161851源顶点与138348band顶点，1280/1920/2560、scene411/617/822三景最低留白5.92165%、灯具净空9.39999cm。D120/FOV70只有2.2897%失败，不采用。TA列明连续roll/反向、sceneRect/layoutGeneration、HUD/guide/pointer/ROI和8消费者；C++图形代理正在独立全源闭包+统一ABI实现，新身份，原027renderer，无MRT类别shader扰动，不扩大tactical隐藏。CPU端点不替代完整状态机/中途retarget/真实路径和三res原生验证。

下一执行：127B从原完整初态再跑raw与stored严格全R；新相机完整上下游合同实际消费及连续路径；在严格物理通过后再整合性能分支，扩其余11中央/96偏口矩阵。正式六景/SSIM、R2入口、R3可信裁判输入与原因、R4音频内容权属、R5冻结回归、R6八真人、R7发行仍分别开放。保护主程序SHA44374e01…再次核同；旧日志证据不删。


## 检查点65（2026-10-21）：单中袋真实整杆通过、三档灯下相机原生验证及保留失败

R1–R7 仍 OPEN，M4 未完成、Phase 6 OFF、真人研究 0/8。以下是独立候选的局部结果，未推广默认程序、物理 profile 或资产，未通知真人测试版本就绪。

**袋口物理：130 的单 P4 / Power40 中央工况完成真实 Controller 击球后的 62 步。** 母球实际击打目标球，目标球经真实有限边轨迹、入口与通道后自然收球；主链捕获前实际执行 raw 点、表示桥与 stored endpoint 三项完整源几何门，全通过，无完整旧 h 回退。全局 `strict_trueAST_or_full_mesh_domain_pass=false` 是 harness 仍未批准总体准入的固定声明，不能误写为这次单案例几何失败。Root 重新用真实 glTF importer 导出三档 LOD 的全部 4315 台呢三角形，逐个有向 F32 顶点与冻结源一致；实际捕获球心垂直投影不在任何台呢三角形内，距水平支撑边界约 3.6526107 cm。含斜切台呢与 XZ 退化线，未仅测抽象袋口。证据 `20261021-root-current130-fresh-gltf3LOD-projection-v1/ROOT_CAPTURE_SOURCE3GATE_AND_FRESH_GLTF3LOD_CLOTH_PROJECTION.json`。

同一 130 binary 的 Entry51 / Capture62 双故障运行完成，独立重算全部物理、完整 aux、combined 与父链；实际即时恢复与真实新 kernel 重试成立，62 accepted 全非 CPU 树逐字段一致，只排两项明确 CPU profiling 字段。另有数学 Unknown 拒绝与原完整 h 独立重算对照，不与 outerguard 重试机制混为一项。双故障证据 `20261021-physics-current130-samebin-retry-audit-v1/SEALED_MANIFEST_V1.json`。正常 / 故障外部运行各约 239 秒，属并行开发取证，性能仍 FAIL。其余 11 中央与 96 偏口/沿库等尚未通过；当前仅 P4 边注册、单有限运动体域，不能据此称六袋或普通产品已修好。

**相机：同一 c8e4 冻结 binary 三档真实窗口完成无击球操作。** 实际 framebuffer 为 1280×720（ui1.35）、1920×1080、2560×1440，稳定俯视 scene 高 411/617/822，FOV72、眼高211.483978 cm、world-X screen up、projectionPan=0；三档均能实际显示完整六袋和台桌下方操作面板，并返回瞄准，各窗口 exit0。Root 核对原始帧 SHA、实际资产 SHA、日志全段父链与连续序号、全程记录球速0、受保护主程序 SHA 不变。1920 共18个有限实际过渡取样，闭合无尾部残缺；取样扰动时序，不能替代正常性能或未采样连续轨迹证明。两次快速 O 的第二个实际 keydown 已到零速端点，**没有覆盖 active-segment 队列反转**。Root 证据 `20261021-root-native-underlamp-camera-v4-v1/ROOT_THREE_NATIVE_RUNS_SCOPED_AUDIT.json`，TA 逐帧独立复审尚在进行。

**明确保留视觉失败：1280 大字号返回 Aim 时，左上 HUD 仍遮住远端库边区域。** 稳定俯视局部门不能替代完整 C0–C5、真实六景光学、手动相机/指针、击球、SSIM 或正式入口。MRT 旧 OFF/ON 像素差失败仍保留，新候选使用原 renderer，无 MRT 合入。

**普通交互物理另有阻断，已做局部事务修复。** c8e4 的真实 LeftButton release / power110 在交互 v5 post-Shoot 第66步触发 event_budget；失败后3个 observer 状态及更广规则 authority 哈希泄漏。后继私有候选只改一个 Runtime 实现 TU，去除失败时额外清理；真实同输入仍触发原 event_budget，但完整公开 GameState、公开 aux 子集、authority/hash 绝对前后不变，三次重试也一致、诊断序号正常推进。未把公开 aux 子集冒充全部私有 aux。467引用独立核同，seal `20261020-underlamp-product-composition-cpp-v4-private/runtime-failed-step-aux-transaction-v1/SCOPED_AUX_TRANSACTION_REPAIR_SEAL.json`。这修好的是失败回滚，**没有解决 event_budget 本身**，未构建/推广后继原生包。

下一步：泛化中央12真实有限边注册并按原输入/域验收，保留96 typed adapter和拒绝边界；定位原交互 event_budget 的实际碰撞事件链，不放宽预算或降低力度；修复小窗口 Aim HUD 遮挡并补真实 active-segment 反转/指针/手动相机检查，再整合同源物理与性能。旧 profile、回放、训练分数及规则消费者仍未批准迁移；R2–R7 原缺口继续按对应门收敛。


检查点65后继逐帧补记：TA实际查看全部25帧（18自动+7手动），原共享C++矩阵48×25 bits完全一致、pan0；稳定三档全源顶点留白5.9216549%，九字段/98glyph均完整。**过渡仍FAIL**：epoch2/frame1751右上context HUD盖住目标球堆，且近/左台体裁切；epoch3/frame2967明显倾斜裁桌。实际HUD fallback计数1280=15、1920=8、2560=11，含真实SDF clip与力量文字撞10格条。不能用终点局部通过反证转动帧。所有O KeyDown之前transitioning均false，KeyUp不能计新请求。Root补记 `20261021-root-native-underlamp-camera-v4-v1/ROOT_POST_AUDIT_VISUAL_FAILURE_ADDENDUM.json`；进入路径与HUD修复，未宣称整体验收通过。


检查点65相机验收范围更正（保留上条原始观察）：经TA核原计划§9.1/§9.7，没有“低Aim出发的每帧必须六袋全入框”的明文要求。转动裁桌是实际观察及后继路线建议的诊断，不能倒推为原计划全桌过渡硬门FAIL。明确未过项为HUD盖rack、真实SDF截断、power文字撞10格条等可读性门。原lookdev tactical_overhead另要求orthographic、span3.0..3.3m/height3..5m/8..12%边距；当前perspective D125/FOV72/5.92%只过局部4%探针，未过正式俯视标准。后继路径可声明Acquire阶段再要求全桌，不能事后给低Aim加条件。更正证据 `20261021-root-native-underlamp-camera-v4-v1/ROOT_ORIGINAL_CAMERA_ACCEPTANCE_SCOPE_CORRECTION.json`。


## 检查点66（2026-10-21）：交互支持抖动的独立新公式局部修复与正式相机实施合同

M4/R1–R7仍未闭合，默认与旧证据不变。Root独立复跑失败事务修复exit0/1.456s，原event_budget仍明确保留；继而核支持判定候选509引用与binary df98dbc8…，实际独立重跑exit0/3.401s。新私有profile `chinese_pool_interactive_support_manifold_120hz_v1_private` 同Controller power110后608个成功120Hz tick自然止球，16球v/ω逐项精确0、无捕获/隐藏，整个原始生产NDJSON与开发冻结运行逐byte相同；旧profile仍第66步失败。Root证据 `20261021-root-support-manifold-independent-reexecution-v1/ROOT_ACTUAL608_STOP_AND_FROZEN_STREAM_IDENTITY.json`。这是CPU生产Controller接缝，非真实原生玩家鼠标、非新mesh六袋、非性能/发行/M4通过。

根因实际预算64：密集支持域在1e−6..1e−5 cm/s向上残差下反复切换Support/Airborne/ClothReturn，29 scheduler批次/28共享岛，consumed63后再需1次预留而拒。部分dt0、其他仍前进，不能误说128死循环/简单off-by-one；存在27.254423 cm/s真实母球跃起，不能全阈值剪vy。候选只对真实已解SupportPlane、现有残差/slop/耦合/能量门通过的支持流形残差归一，保真hop和异常拒绝，不加预算/改步长/减力度。

单变量及兼容证据：新旧同输入首次全16PVW和完整GameState同，前65tick完整14类PhysicsStepTelemetry及嵌套contacts/events同；tick66前三次真实solver输入/约束/结果同后才第一次支持归一，真hop保留。旧两profile全部712原回执树同。7项实际求解边界/负控保留。第一个仅改profile id就绕开旧scheduler/击球映射的假通过已REJECTED；后继明确继承2处旧id dispatch与6处legacy公式分支，不注册工厂/生产/训练/回放选择。当前成绩仍scenario SHA索引、保存训练仅profile id，未来允许新公式进入训练前必须复合profile hash/旧成绩归属；13课已批准记录保留且不要求重玩。seal `support-manifold-residual-profile-v2/SCOPED_SUPPORT_MANIFOLD_CANDIDATE_SEAL.json`，新版本广力度/tip/support-edge/跨域与晋升仍OPEN。

正式相机下一步采用原明文orthographic，而非将灯下125cm透视推广。TA合同 `20261021-camera-HUD-acquire-and-formal-ortho-contract-TA-v1/ROOT_FORMAL_ORTHOGRAPHIC_AND_HUD_ACQUIRE_IMPLEMENTATION_CONTRACT.json`，seal d70eefd3…：原socket眼高392cm、verticalSpan315cm、原既有tactical天花板/灯具裁切且光继续发射，实际源码同时已有venue与table lamp分支，不臆造未裁缺口。统一VP/inverse、平行pointer、depth/DOF、正交PBR常量viewdir、guide/ROI、版本和四authority。实际全源CPU投影playfield边距9.6834%，外框边距5.1809%另列；1080球径19.59px，720仅13.06px，范围不混报。HUD Acquire保护可读cue/target与完整九字段，不倒加低Aim每帧全桌硬门；正式稳定overhead才验原全桌六袋/8..12%。C++正在独立实施，尚未构建或GPU验收。

六袋边注册131已完成小CPU真实全mesh扫描与负控，正在独立集合复核和Root单角袋运行准备；未跑131整杆，不把注册3793条凸边的数量当进球验收。原108矩阵和输入SHA不改。


## 检查点67（2026-10-21）：131首角袋真实失败、同版本P4局部通过及下一修复

Root在198资源/runner/bin/binding/protected-main全部预核后，实际执行冻结131原P0Power40完整Controller工况。进程自然exit2/wall36.714635s，已发布到h50，真实Opening速度29.80844077 cm/s在原10–40带内，但h51失败 `native_chart_v108_maximum_precision_budget_exhausted`，Entry/Channel/Capture均未发生；原预声明中央验收明确FAIL，未增加预算/改阈值/隐藏球。Root `20261021-root-current131-first-P0-Power40-v1/ROOT_ACTUAL_P0_FAILURE_SUMMARY.json`，全部原始日志保留。

独立因果核：typed128先因native-owner/cubic-mesh Unknown拒并完整恢复，后备oldchart在同真实cloth_support0/cloth_covered_slate1切口边100793/103650的Stick曲面midpoint门耗尽；不是新receiver_side8凸边注册错误。入口slip约2.0749001548e−6 cm/s，原carry门2.0759065601e−6；128至4096各层首midpoint仍超门，最后超约6.7138763e−10 cm/s，coarse/fine pair均未完成，8192仅计划未执行。条件N/cone可行但不作为实际force准入；不提高预算/容忍度，不做强投影。失败51未发布，完整physical/fullaux与最后已发布50逐位一致；100份physical/aux/combined SHA重算闭链。后继独立候选修真正曲面约束积分/残差演化及typed几何证书缺口，原131冻结不动。

Root同一131 binary/resource另从原P4Power40初态实际跑完62步，exit0/wall235.417396s，目标进source4袋，母球未收，Opening31.31229133 cm/s、中央局部验收通过。Root从原BGLL独立核实际raw/bridge/stored三全源门；以同不可变几何/glTF的已核真实三LOD导出结果复核，捕获投影在各档全部4315台呢三角形之外，距H0支撑边界3.6526107 cm，最终物理SHA d33e483f…与130相同。**本次复用已核导出，不假称又新跑importer**，证据 `20261021-root-current131-sameversion-P4-gltf3LOD-projection-v1`。不能以P4成功盖掉P0失败，其他中央/96及完整轨迹/故障/多球/校准/实时/普通产品仍未过。

Mac原生工具再次报告锁屏。Root没有启动无参或不受控新窗口，先继续CPU/正式正交源码工作；旧c8 OFF画面对照仅预备于 `20261021-root-c8e4-legacy-off-native-reference-v1`，NOT_LAUNCHED。正式v5八消费者仍在实现，不能称相机完成。


## 检查点68（2026-10-21）：正式正交静态候选完整构建与实际Runtime，GPU仍待解锁

Root核正式v5交接/封存和全部源码后，独立fresh全TU全库构建成功（exit0，62.600998s），未混用旧对象；源码前后不变、保护主程序44374e01…不变。新原生候选binary SHA `d8f95a1368b43250162befc400e732e79a78718eba05e8cdd809f83581e04b5d`，证据 `20261021-root-formal-ortho-v5-full-build-v1/actual-build-terminal.json`。旧mesh/tone shader字面逐byte保留，新正交使用独立显式kind程序，尚需真实OFF/v3像素对照，不能靠CPU旧状态同宣称旧画面不变。

以本份新ABI对象/库实际链接Runtime探针后，Root另用独立设置路径复跑exit0：真实Runtime accepted kind1、eye[0,392,2]、upZ、verticalSpan315cm、near2/far1800，真实Snapshot→共享VP有效、官方Observe初态四authority保持。O/P/Tab显式静态scope拒绝且authority/frame原子不变；生产power110/Shoot接受并moving=true，维持静态安全视角，但**没有完整杆轨迹通过**。Root `20261021-root-formal-ortho-v5-runtime-reexecution-v1/actual-process.json`，开发绝对hash/112新链接对象封存 `runtime-static-scope-attempt02/FRESH_ABI_RUNTIME_STATIC_SCOPE_AUDIT.json`。初态Observe原生界面与Aiming九字段真实字体组件检查分列；不能改初态凑截图或把组件通过扩为原生九字段通过。

新包和三档独立计划已准备于 `20261021-root-native-formal-orthographic-v5-v1`，**未启动、GPU NOT_RUN**。Mac工具再次实际报告锁屏/自动解锁失败，Root在可审查新包及Runtime闭合后仅发一次集中解锁请求。旧c8 OFF对照计划同样未启动。未切换正式Billiards入口、未推广资产或新物理。动画Acquire、退出/手动相机/回放/训练等仍明确未准入；只是开发分段，不能缩减产品要求后宣称R1/R2/M4完成。

P0后继132仅有第一typed cubic片段新几何证书局部成立，全部原三角保留、无R/积分预算/force/PVW改动；整h51在piece39实际Edge→bevel Face owner域交界又拒绝，正在实现真实feature TOI与分域物理。没有132完整Runtime杆通过，不用第一片段证明替代普通角袋进球。


## 检查点69（2026-10-21）：Root发现并闭合真实UI接入CPU缺口，后继原生包待解锁

Root代码链复查发现已冻v5的Coordinator仍将所有sceneViewport.enabled送入旧bottomlane，full-window正交必触发不足/fallback；18个侧栏组件不能证明此生产接点正确。旧d8f95原包未启动，缺口记录 `20261021-root-native-formal-orthographic-v5-v1/ROOT_REAL_COORDINATOR_ROUTING_GAP_BEFORE_LAUNCH.json`。独立v5a只改Coordinator与HUD两实现TU、ABI不变；真实Runtime→Reducer→Coordinator涵盖Observe6字段/0bars与真实Tab后Aim9字段/10bars。原版错误日志和guard/glyph失败保留；修复无该fallback、实际字符/glyph/scissor守恒、panel对实际glTF台体/六袋与16球guard无交。typed坏帧/身份明确拒；旧OFF/v3同Coordinator输出逐byte同。该CPU生产链与未来原生Observe/Aim仍分列，不外推完整玩法。

Root核57refs、原449源码和精确link差异：只替2个本次v5同ABI实现对象，保原main和其他fresh全构建库，115实际对象/库/动态库全部预/后哈希同。实际relink exit0，输出新binary `6be9578726d9e79e5bacbd7fc75d9e4327a85419d58ae4d1fcbecfb1a8fd7e5e`，不改d8f95/c8/保护主程序。证据 `20261021-root-formal-v5a-ui-seam-relink-v1`。新唯一包/三档计划 `20261021-root-native-formal-orthographic-v5a-v1` 已备但NOT_LAUNCHED，仍等原有一次解锁请求；GPU/完整相机/M4未通过。

P0真实曲面hybrid producer及mixed-piece consumer/完整aux rollback接口由两名C++分别继续；第一edge证书/真实feature根/正gap飞行CCD/真实joint及spin根均仅组件，尚无132完整h51或完整角袋通过。不扩探针数量充当进球结果、不提高原预算、不投影PVW。TA等待实际新帧后才运行独立oracle；Acquire及性能新代码暂停，先推进该物理关键路径。


## 检查点70（2026-10-21）：三档真实正交静态取证完成，视觉失败与角袋完整步阻断保留

本条接续69的待解锁状态，不改旧记录。Root在用户解锁后，用同一v5a binary `6be9578726d9e79e5bacbd7fc75d9e4327a85419d58ae4d1fcbecfb1a8fd7e5e` 实际运行1920×1080、1280×720/ui1.35、2560×1440，均自然关闭exit0。三档均实际显示完整台桌与六袋，Observe侧栏无台桌/球遮挡；真实正交315cm、原战术天花板/灯具裁切且光源继续发射。1920实际Tab/O/P拒绝前后frame与GameState SHA相同。Root核全部原生日志段哈希/父链/连续序号、原始帧与资产哈希、球速全程0；因此只是无击球静态验收，不代表完整相机或玩家整杆通过。保护主程序44374e01…不变。

Root独立重核TA两份封存全部32引用，联合局部封存 `20261021-root-native-formal-orthographic-v5a-v1/ROOT_NATIVE_STATIC_THREE_RES_SCOPED_SEAL.json`，SHA `ffa9db506a5148d96e49e743480e7367d12faf3912a7da1d83330fa8b6885719`。四实际帧共享矩阵192个F32/64个inverseF64位独立重建一致，Observe真实6字段/73非空glyph；Aim9字段仍只有另列CPU接点证据，不能将Observe改为Aim凑验收。原生camera语义2与CPU探针postShoot语义0属于不同阶段：native预Shoot显式设AimOverhead，真实Shoot内部observeFromCurrentPose转ObserveFree，TA最初阶段误配拒绝已保留及更正。

**原视觉门仍FAIL。** 1920原固定ROI的full_playfield线性亮度.063131<.10、ball_field .083562<.09；720球径13.06px<18px。TA只读诊断显示原full ROI约50.51%落在CPU台桌投影bbox之外，外部亮度.006729、bbox内部.120699，足以解释很大一部分混合亮度；台呢补丁.159240只是CPU选区诊断，非GPU精确语义遮罩/正式物体曝光通过。不能由此放宽ROI或直接断言台呢太暗。DOF实际OFF，启用路径NOT_RUN；SSIM无批准基线。两字母SDF源padding越clip、可见ink仍完整，软件模型不足以证明GPU抗锯齿零裁切，保留AA_NOT_PROVEN，不谎报可见截字或完全通过。

旧OFF实际OS窗口对照只有匹配Aim的client像素0差；Observe旧图是瞄准返回的母球中心视角，新图为开局台桌中心视角，1,402,486差异像素属于未匹配pose，不能判旧画面回归。补拍return时Mac再次锁定，未执行菜单操作，声明300秒截止后正常exit0；此运行不计视觉通过，不为补充对照反复请求解锁。原生所有窗口现已关闭。

**P0角袋仍FAIL/未修好。** 冻结131原P0的h51精度耗尽失败及P4局部成功不变。后继132实际producer已走原fine256支持边前缀79段、真实owner出口、完整源gravity CCD、原joint，run05闭合owner到gravity的误差传递；碰撞后impact时钟/重置误差、后继spin/face及整步提交仍待闭合，没有h51实际发布或完整P0进球通过。两名C++分别推进生产链与单Face真实reset/cone证书，未增原预算、改R、投影PVW或用下降动画掩盖支撑。新helper定位到GMP未约分表示的相等/序列化差异，所查有序比较与有理数值仍正确；仅规范新helper入口，不据此宣称旧131数值界错误、不改旧证据。

下一验收顺序仍为：132真实完整h51事务及几何/动力学误差闭合→原Controller P0整杆与同版本P4→原六袋中央/偏口矩阵及完整日志/性能→同源普通产品兼容、正式相机/六景、R2–R7。R1–R7仍OPEN，研究0/8，未通知版本就绪，Phase6 OFF。


检查点70物理后继补记：Root对actual06碰撞重置做独立Python Fraction复算，从冻结原PCK字节及实际C++ctx九个F64坐标位唯一映射原JSON面41014（物理gid103649是PCK记录号，不是原JSON数组号）。首轮错误按同号索引的复核拒绝与脚本保留；正确源绑定后的normal、四矩阵36区间、Jn、Coulomb内部余量及actual post偏差全部精确重算一致，速度/角速度每分量偏差上界最大约2.10534e−14 cm/s与9.90575e−16 rad/s。此处preBox仅actual输入点，无继承事件扰动，不冒充完整误差盒。

原生产run08已有actual impact事件误差组件complete，但整h仍真实拒绝 `hybrid132_actual_face_dynamic_continuous_error_unproved` / `face132_dynamic_spin_sign_crosses_event`。碰撞后normal-spin零根约3.31446微秒，声明cubic根与ODE模型根误差继续分列，不能越根继续原符号或直接将W清零。完整physical/fullaux未变、whole_h_published=false；尚非fresh Runtime h51全BeforeBinding接入。Root记录 `20261021-root-actual06-face-reset-fraction-audit-v1/ROOT_SCOPED_COMPONENT_AUDIT_SEAL.json` SHA `9684da248913c6ff84813d8fd8b27bc69e86698e63e35df93c3cfe5d8358d00c`。第二C++接手独立fresh Runtime Unknown/完整rollback接点，主物理继续真实spin后继，默认与旧131不变。


检查点70接入/参考补记：fresh Runtime实际boundaryMode为Unbounded，即关闭旧analytic矩形边界，实际owned mesh仍承担有限世界碰撞。132旧导入诊断手填ProductionTable，不可当作完整真实BeforeBinding；先以冻结旧源记录真实Unsupported与完整事务恢复，再用明确新版本只支持强ctx/profile绑定的native-mesh Unbounded＋empty observations，不改标旧证据。主物理与接入C++已确认该边界。

TA另外仅核3来源候选并实际看8张图，仍未取得安装中式中袋内侧双鼻与cloth/slate切口同帧可信实物近照。星牌赛事远景、C17中式型号产品视觉及裸石板分图不得拼成实物依据；论坛原图DNS失败未得像素。新增只读封存 `BilliardGL-evidence/m4/20261021-primary-middle-three-candidates-pixel-review-TA-v2/TA_THREE_SOURCE_PIXEL_REVIEW_OPEN_SEAL.json` SHA aa39db23f18ea7df6d7402e5c4eafb1b3d03460b5a861f58976bb66dbd36d073，Root重核引用一致。参考缺口保持OPEN，不新增无照片支持的量测轮廓，不据此升级R1。


## 检查点71（2026-10-21）：真实Runtime接入负路径独立重跑与相关约束拒绝定位

Root核fresh08封存639引用后，独立重新编译接入mainTU、链接不可变08对象，binary仍为0e6b6c3bbf60c72a57bd1cae203276dc7d17937e1771188ea1a32096295d4328，实际exit0/wall37.757756s。原Controller真实P0Power40击球后50步全部接受，BGLL为metadata＋50tick＋footer共52记录，无h51 accepted；Root独立对50步all16 PVW、domains/capture、完整numeric aux及aux/combined/physical前后hash与开发冻结运行逐项相同，另逐byte核原Runtime副本只增1条friend声明。实际Unbounded/planeRequests=[]送入08 producer，h51为明确Unsupported，正常拒绝与单列受控stage/sharedaux损坏均恢复。不是导入旧轨迹替代fresh，也不把受控故障称真实物理前进。

实际D5B active=true但hasFrozenObservation=false，其他三规则条件false，故实际请求为空；第一轮仅看public active即保守FAIL_CLOSED的原失败与50步日志仍保留，后继通过只读提取真实observer字段而非禁用规则。h50旧119 record真实NULL，132source/cursor未产生，非空/nonzero回滚及allocation/write故障、穷尽私有Runtime语义序列化均NOT_RUN，不扩充通过范围。Root独立证据 `20261021-root-fresh-runtime08-independent-run-v1/ROOT_SCOPED_FRESH_RUNTIME08_SEAL.json` SHA018f65d6061bbb1d3eaa3f87edce9f7288b6041126c4ed8267438015ee033d27。新NativeWorld版本fresh接入仍另跑，不静默替换08。

后继生产10真实诊断仍Unknown：原同span法向上界6.2081765e−6cm/s低于原2.0038894e−5，切向独立component box上界1.5955416e−4高于原2.07590656e−6；F64项仅约1e−14。Root对导出的原Dense21 F64bits/原hi/实际native面独立做Fraction区间Bernstein：先逐系数组合T(V−RΩ×n)，nnᵀ用NNᵀ/N²精确有理、保持公共法向相关性，再包围同一轨迹；保相同16ev＋48R ew后上界2.4033786549e−12cm/s低于原门。明确是原component box丢相关性的保守拒绝，没有放宽预算/改轨迹/缩span；只证明source切向约束组件，不替代连续ODE、能量、全源域、SpinStop或h51提交。证据 `20261021-root-run10-correlated-constraint-audit-v1/ROOT_SCOPED_CORRELATED_CONSTRAINT_SEAL.json` SHA072c77213a5ef30b25fc36be46204e9ba6992e96263a63b1a94247000b7845db；主C++正接回生产11验证。

P0完整角袋、六袋与R1–R7仍未通过，性能仍未准入，M4未完成，默认程序和旧profile/回放/13课记录保持。Phase6 OFF。


检查点71后继：NativeWorld10 fresh私有接点首轮因完整诊断超过原4MiB metadata上限真实exit74，原失败保留；后继不加上限，完整诊断用既有CheckedGzipSink旁件，CRC/raw/container SHA读回成功才提交小元数据，并先耐久关闭50步物理前缀。实际后继exit0/wall122.20s，两次h51为MathUnknown而非Complete，各raw9,810,756B、gzip3,044,051B，两份字节完全相同，总6,088,102B在原64MiB候选总cap内。Root另做只读独立全646引用重哈希、完整gzip/CRC/字节/cap核查、所有前后exact canonical与Root08前态比较；实际碰撞pre/post/normal/mRI、Dense21位与导入10相同，证明该局部诊断能从真实Controller后态复现。正式GameRuntime::step→updatePhysics仅跑到50，51仍friend私有CPU调用，无正式kernel51/状态发布。证据 `20261021-root-native-world-fresh-diagnostic-audit-v1/ROOT_SCOPED_NATIVEWORLD_FRESH_SEAL.json` SHA376bb51e519cf0a6830d4506bec33c3e78d5dcea3c0f2592c8e834d664e560a7，不把同一Unknown重复运行成本当实时性能通过。

生产11的同参数导入诊断已实际通过连续运动/约束/能量组件，现仅声明轨迹＋gamma16的全源几何查询在真实支撑Face103649为Unknown。该查询尚未纳入累计模型位置误差，更不能外推其他表面全ODE域已过；后继需真实impact origin、根窗前gravity/根窗后支撑两分支、正反力/摩擦/有限owner及累计tube下全其余源表面排除。保留zero-tube诊断false，不简单跳面，不改实际R/PVW。


检查点71并行R4补记：TA复用既有生成/技术复审/用户v3候选选择，整理可审查来源包；Root独立核30个v1/v2/v3 WAV与25个生成脚本/清单/后继证据共55文件SHA一致，未重新合成/试听/技术测试。v3五类新声音与其余五项从v2逐byte复制明确分列；原生成源与安全wrapper分列，受审链依赖为标准库/本地模块，不等于权属保证。来源包 `BilliardGL-evidence/m4/20261021-R4-v3-content-source-package-readonly-TA-v1`，TA seal bd2fb2e0b3ab273b009828783246d9415db8d7421f2cb65238735b6342d54167，Root核对 `20261021-root-R4-v3-source-package-review-v1/ROOT_SOURCE_PACKAGE_PROVENANCE_AND_USER_REPLY.json`。对具名权利人/适用许可的唯一补充信息请求，用户原答复“还定”原文保留；没有提供具名权利人和许可，登记UNRESOLVED，不解读成许可或发行批准，不重复追问。v3可测候选选择继续有效，默认音频未改，R4与R1/M4仍OPEN。


## 检查点72（2026-10-21）：真实支撑与SpinStop组件推进、15球原law独立复跑，完整h51仍未提交

原12实际exit0但数学Unknown，wall175.751s：真实首次Face/时间/normal-point绑定、实际F64自身面不穿透及模式邻域前门已过，累计pre-gravity管在邻面103650的depth14空间box排除失败。独立同源7有限特征精确多项式根排除complete、初始无穿透、possibleRoots=0；这是原box过保守的局部定位，不采样当连续证明、不增14深度/改R/删面。13把该方法接回真实主链并提取不变box，实际wall83.528s仍Unknown：首Dynamic支撑组件全323774源覆盖闭合，实际SpinStop身份事件carry通过，model root pad约7.32589e−9s，raw normal-spin约7.07268e−13rad/s保留；下一真实Static首曲线连续force/error/energy也通过，但尚无其origin/native tube及后续完整分段/提交。所有这些仍是同参私有组件，不是正式Runtime h51或角袋整杆通过。

主开发已将真实root/reset/Dynamic/SpinStop/fullsolve/exact-prefix/PVW/quaternion/carry保存在CPP私有ActualFaceChain，非caller JSON complete布尔。14实际wall82.151s仍Unknown，真实Face firstExit完成且found=false；不能据此取消其他实体竞争。独立几何helper以同13原fullspan发现103904 expanded(R＋累计tube＋FP)有限Edge0根，而仅physicalR精确全有限特征无根；原Unknown封存保留，不为阳性缩span。Root另从实际C++全native导出独立Fraction核103649/103904：sharedEdge incidence恰2、方向相反、双方第三点严格内侧（均−434269865/4503599627370496），源JSON同绕序唯一映射41014/40982。conditional halfspace/Cauchy证书可在真实source/model支撑平面约束与严格finite-owner前提下证明邻面分离；不能套103650 concave，不能以本几何条件授origin/force/整步。后继由第二C++实现、主C++以真实typed链消费，尚待实际通过。Root证据 `20261021-root-convex-neighbor-halfspace-audit-v1/ROOT_ACTUAL103904_CONDITIONAL_HALFSPACE_GEOMETRY.json`。

被动15球原law已由Root实际独立复跑exit0/wall4.288403s，结果SHA bcd18f503e36635b03153a34a31855cf275193e9865c5e24133532cb03842125与开发封存逐byte同；实际before全GameState与Root fresh08同，active1完全不推进。保cue0微Vy/speed与14rack零角零轴，不用Airborne/强零替代。原peer只更新Y=float(double(entry.y)+double(entry.vy)*h)，其他实际字段保持；以原支持/高度表示界、非H0实体与15球105pair及完整存储桥核验。Root核470refs当前均匹配；继承-MT目标误收集的一份foreign entry.o非实际link/runtime依赖，另存同SHA归档，不修改旧seal，后继修pin收集范围。Root封存 `20261021-root-passive15-independent-run-v1/ROOT_SCOPED_PASSIVE15_SEAL.json` SHAd34f5cdd0e90e799a6973258d9e5fbbbbecd60aea1a6e71145f51df965da8f08。不是全16主动演化、Runtime51、自然止球或M4通过。

**止球风险范围更正。** 通用anyBallMoving查speed>0是实际源码事实，但当前numeric link在physics入口直接return localFlight，绕过通用聚合；localFlight、finiteRelease与typed回调有各自moving/shotEnded策略。因此不能从cue微残差断言当前131必然永不终止，也不据未实测故障盲改peer/normal策略。中央捕获门本来不等于普通产品全16自然止球/规则结算；新formula最终仍须实际调用链实证。只读后继 `20261021-stop-semantics-actual-consumer-readonly-v132/READONLY_STOP_CHAIN_AND_SUCCESSOR_OPTIONS.json`，原计划没有“所有F32分量必须exact0”的新硬门。

R1–R7仍OPEN，M4未达成。下一仍实际连续分段/全部16球/真实issuer与new-profile factory/verify-eval-publication，然后正式Runtime整步、原P0整杆及同版本P4，再其他六袋/偏口、性能和原R2–R7门。旧131/默认主程序/旧回放/13课记录不变，Phase6 OFF。


## 检查点73（2026-10-21）：第二段支撑面的真实离面根窗闭合，面到边过渡仍拒绝

实际run15保留Unknown：第二Static裸源出口不能代表带累计模型误差的真实出口。后继run16实际exit0/wall96.859604s、CPU95.269413s，原时间精度1e−10s未改；真实模型根窗pad约3.057197345e−11s，定向F64窗口宽6.120457989e−11s。仅接受模型safeBefore之前有证明的面支撑前缀，两段Static的局部几何/15球对/姿态组件成立。下一拒绝明确为 `hybrid132_actual_face_owner_exit_physical_transition_unproved`：Face到凸Edge的真实力学、跨根窗两域误差与几何联合仍未闭合，无整h51发布。完整physical/fullaux未变，32.8MB原诊断保留，不用裸源根、投影、强置零或缩短整步掩盖失败。

Root只读重核run16不可变快照27项、实际结果SHA、原进程记录；以精确Fraction复核两个实际F64端点之差小于原精度，不将该算术复核冒充模型根/力学独立证明。另补核凸邻面条件组件封存全部455项一致；此前Root独立实际运行10.496665s、结果与封存逐byte相同仍仅条件几何，physical_manifold_premise_verified_by_this_helper=false。Root新证据 `20261021-root-run16-clock-and-source-audit-v1/ROOT_RUN16_SOURCE_AND_CLOCK_SCOPE.json` SHA847ef8a2d9fdd4d0473b081e29d6f6ae731dc9bd83603b116db4c422fce04ec9。

第二C++另只读评估已有采样/源码中的实时运动、精确证书/全网格及日志成本，遵守原计划实时数值产品与离线严格研究边界；不得移除生产完整校验/日志后宣称G4通过。当前约97秒开发诊断远未达到原实时门，不能以组件数量代替产品。保护主程序SHA44374e01d2b4eecadef773aa8775324047f4ceda42822cbbf4004aa50a6169eb未变，磁盘现约18GiB，不清理旧证据。R4具名权利人及许可仍UNRESOLVED，用户“还定”不作批准。R1–R7 OPEN、研究0/8、M4未达成、Phase6 OFF。
