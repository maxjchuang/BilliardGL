# 试玩诊断日志

产品模式（`--product-mode`）默认启用本地 JSONL 日志，不录屏、不上传。
默认目录：`~/Library/Application Support/BilliardGL/playtest-logs/`。
可用 `BILLIARDGL_PLAYTEST_LOG_DIR` 指定测试输出目录。
启动终端会打印实际路径；写入失败会输出 WARNING，不得将缺失日志当作正常验收证据。
每个会话保留最近四个约 32 MiB 分段（单条记录不切断，所以上限会略超 128 MiB）。
轮转只覆盖本会话的旧分段，不清理其他会话；多次启动的历史日志仍会累积。

记录入口：

- `session_start`：构建时间、可执行文件名及可读取时的 SHA-256、固定步长。
- `input_before` / `input_after`：游戏窗口内的键鼠输入、坐标、位移、按键状态、缩放、处理结果与前后权威状态。
- `physics_step`：每次实际执行的物理步，无轨迹抽样；包含场景 ID/hash、步进结果与球状态。
- `presentation_prepare`：提交渲染前的球位置和可见性、实际相机、帧耗时、物理推进次数、积压时间；区分回放和实时权威。
- `swap_submitted`：同一 `frame_id` 的 render/swap 调用已返回；不等同于操作系统已将像素显示到屏幕。
- `session_end`：正常关闭标记。异常退出可能没有此行，之前已写记录仍可分析。
- `session_interrupted`：异常展开栈时尽力写入；强制结束或断电不保证出现。
- `ui_layout_changed`：训练/回放面板临时收起或恢复，含原因；每帧的 `presentation_prepare` 同时记录 `inspector_collapsed` 和 `layout_degradation`。
- `render_error`：渲染抛出异常时记录具体错误，再交给原异常处理；不把未知错误吞掉。

分析“目标球跳空中消失”时，先定位击球输入及实际参数，按球 ID 对齐物理步与显示帧：
检查高度/垂直速度、碰撞前后变化、物理域、入袋阶段与出界标记，最后检查渲染可见性。
不能仅凭球超出台面高度就认定物理错误，也不能仅凭显示消失就认定已经入袋。

用户无需录屏。出现异常后尽快正常退出以保留最新上下文，助手直接读取本地最新会话进行分析。
日志包含游戏操作；分享前应检查，不应上传整个用户目录。
日志是诊断证据，不替代训练课程人工验收，也不自动证明物理正确。

2026-09-14 剩余九课 v2：新增 `training_step_index`（从 0 开始）和 `training_step_count`，
用于对齐多球课的逐杆提示、Prepare shot 输入与各球实际轨迹。默认目录现为十三课；
当前构建与校准证据见 `training-remaining-v2-verification.md`，下文二进制为历史诊断记录。

2026-09-09 训练入门 v2 增补：上下文记录 `training_pack_id`、当前课程 content hash、
`training_score`、会话已完成数量、手动面板策略、推荐力度范围、目标圈中心/半径及是否启用。
首次白球与目标球接触后另记 `training_contact_cue_x_cm/z_cm`，可直接核对动态停球判定与画面圈的位置。
2026-09-14：新增 `training_cue_rail_contacts`，表示当前尝试中白球累计库边接触事件数。
跟杆入门要求该值为 0；不把接触事件数直接等同于不同库边数量。其前方目标圈固定，只有定杆圈跟随首次碰撞位置。
被课程锁定的力度/击球点输入记录为 `training_control_locked`，不会改变权威参数，也不会制造 Action unavailable 弹错。
训练与回放 UI 使用动态呈现，不再复用仅按菜单条目构成的旧帧缓存。
本轮交付二进制：`/tmp/billiardgl-pocket-pbr-tests.FmsTfY/Billiards`，SHA-256：
`fe153445b5e3a6a7f2b494341992361fe72622da831a667a28493db6d1b8f9c9`。
最终定向自动回归 10/10 通过，记录在 `/tmp/billiardgl-training-delivery-tests.log`；下文旧构建记录仅用于历史诊断。

2026-09-08 验证：输入控制测试中覆盖完整 JSON、16 球、非有限值、四分段轮转与失败目录；
真实 visible-drawable 产品测试生成 23 条记录，包含 16 个 physics steps 和两对 prepare/swap。
1200 条无碰撞完整状态日志约 165 ms；这是本机微基准，不是带复杂碰撞日志时的帧率承诺。

## 首杆卡顿优化（2026-09-08）

用户会话 `session-1788847838844703-0` 在 tick 1942 首次球球接触后，
physics_step 到 presentation_prepare 相隔约 425 ms；物理步已经完成，音频首次同步初始化是强嫌疑。
产品现在在接收游戏输入前显式 `prepare()`：打开设备并预加载全部 10 种 cue 对应的 4 个 WAV，
不播放任何声音。失败仅警告，缓存失败结果避免每次碰撞反复初始化；兼容模式仍保留懒初始化。

实时循环复用固定步调度器：每帧最多执行 8 个 1/120 秒物理步，丢弃多余墙钟积压而非放大步长。
这意味着长卡顿期间模拟不会追齐全部现实时间，换取后续恢复正常节奏；日志明确记录 dropped_wall_seconds。

新计时：audio_prepare 的 device_ms/assets_ms；presentation_prepare 的 physics_ms、
authority_consumers_ms（含音频）、audio_ms，以及补算截断标记；swap_submitted 的 render_ms、
swap_ms、log_ms_before_swap_record（仅 logger.record 耗时，不含调用端组织 details 的耗时）。

自动验证：设备/资产预热、重复预热不增缓存且不播放、预热后静音首碰撞排队、
1 秒卡顿后只推进 8 步且下一帧恢复 2 步，以及原输入/音频会话/应用渲染回归。
一次独立设备测试：预热后首碰撞 consume 为 0.003875 ms；实际产品启动日志的设备准备约 91.94 ms、
资产准备约 14.35 ms，均发生于接受试玩输入之前。这不是整体首杆耗时，仍待用户复试确认体验。

## 预览后退出与拖动卡死修复（2026-09-08）

用户会话 `session-1788855381520354-0` / `session-1788855621915588-0` 对应的终端错误为
`modern_renderer_product_ui_submit_failed:shell_protected_corridor_unresolvable`。
轨迹预览保护区域与左右两侧面板同时冲突时，训练/回放面板现在临时收起为状态条，
移除所有隐藏控件热区；调整视角、空间允许后自动恢复。其他渲染错误仍明确失败。
输入命中测试使用与绘制相同的受保护视口，避免点击不存在的控件。
此降级不表示正常面板完整内容、布局或无障碍验收通过。

真实窗口回归按日志参数执行停球训练击球、生成预览、设置记录中的视角，
在 2048×1536 视口确实触发降级，再连续改变视角并提交至少 8 帧。
该回归还发现虚线生成的浮点边界死循环：反复 fmod 得到边界之前的值，cursor 不再前进。
修复改为显式切换实线/空隙状态，不再重复计算同一边界。
已通过布局、指针会话、轨迹绘制、应用和真实可见窗口 5 项定向测试；不替代整体人工验收。
修复构建：`/tmp/billiardgl-pocket-pbr-tests.FmsTfY/Billiards --product-mode`，
SHA-256：`fe3f9527225524c17e06d6bbda7c1d99968e53b79b68c769c37397891c26669c`。

## 训练结算后重复击球（2026-09-08）

会话 `session-1788858204619490-0` 有 4 次左键释放返回 `training_attempt_inactive`。
训练结算后输入层现在将击球处理为 `training_attempt_finished`（已处理、无物理动作），
不再把这个正常生命周期状态当成操作错误；正常面板和收起状态条均提示结束后重试。
收起面板保留可见、可点击的 Retry shot，高级详情中也保留该动作；重试不会自动出杆。
观察与原有 R 重置绑定仍可用，不放宽权威训练状态或评分规则。

输入日志的 `resolved_action` 来自实际输入处理结果或点击的控件 ID；
上下文新增 `training_state`（TrainingProductState 枚举）、`training_shot_count`、
`training_terminal_reason`、`training_succeeded`、`training_failed`。
回归覆盖重复 4 次结算后点击、无权威状态变化、Retina 收起按钮的按下/释放间视口同步、
重试无点击穿透以及新一轮击球。此修复不替代十二项人工验收。

## Shift 拖动平移方向（2026-09-08）

旧输入将逻辑点 dx/dy 直接当作世界 X/Z 厘米，因此视角改变后方向不再对应屏幕。
鼠标平移现在使用渲染器相同的 right/up 基向量（含近俯视 stable-up），
逆解水平平面的 X/Z 位移，让画面跟随鼠标；比例取当前视距、FOV、逻辑窗口高度与鼠标灵敏度。
不乘 Retina backing scale，不使用环绕的反转 Y 设置，不修改键盘平移、瞄准或击球参数。
保留目标的台面边界限制；几乎侧视、无稳定逆解时安全忽略平移。
回归用实际投影验证固定台面点的屏幕位移：3 种俯仰预设、4 个朝向、2 个窗口高度、
1×/2× backing 共 48 组，包含正负拖动和变更视距。此为定向回归，不是完整人工手感验收。

## 回放 Play 鼠标入口（2026-09-08）

用户反馈点击 Play 后画面不动。现场取样仍在渲染，日志回放停在 tick 0；
代码发现 ReplayPlayback 未加入 shell 鼠标命中分发，旧测试甚至把鼠标释放返回 input_unbound 当成预期。
现已接通可见按钮的按下/释放捕获，空白点击无操作，移出按钮后释放取消激活。
正常与 Retina 回归覆盖 Play、Pause、单步和取消；键盘入口不变。
真实窗口从训练预览打开参考回放时还复现了 inspector 收起导致 Play 不可见，
现收起布局保留与绘制区域一致的播放/暂停及逐帧按钮，逐帧仍仅在暂停时启用。
真实窗口测试提交至少 17 帧，并断言点击 Play 后回放 tick 增长、点击 Pause 后保持；
本轮 5 项定向测试通过，不代表完整回放或 M4 人工验收。
日志上下文新增 screen_id、replay_tick、replay_paused、replay_finished，
结合 resolved_action 和 prepare/swap 可区分点击未激活、暂停和渲染阻塞。

## 窗口拖动与游戏输入隔离（2026-09-09）

原生输入先验证事件所属窗口、实际 key-window 状态、非 live-resize 状态与内容区范围。
内容边缘 4 个逻辑点保留给窗口管理；拖动须由内容区内按下建立所有权，
标题栏起始后进入内容区的拖动不接管。拖出内容区取消所有权和 shell 按下状态；松手不会击球。
NSWindowDidResignKey / NSWindowWillStartLiveResize 通知立即取消手势，观察者在会话退出时移除。
被拦截的事件仍交给 macOS，但不会产生游戏 input_before/input_after 或修改相机。
纯输入测试覆盖窗口、焦点、边界和取消；原生事件合成测试验证标题栏事件被过滤及内容拖动可用。
测试环境不能保证桌面焦点，合成测试注入 focused=true，正式路径始终读取 NSWindow.isKeyWindow。
5 项定向回归通过；不把合成事件验证当作人工标题栏拖动验收。

### 菜单点击回归修正（同日，取代上面的焦点硬过滤）

上一版本过度过滤：isKeyWindow 在手动事件泵中不可靠，且 ApplicationDefined 等非输入事件
也会取消鼠标所有权，导致菜单只有按下、没有激活。现在仅处理列举的键鼠事件，
依据 Cocoa 事件所属窗口与内容范围接收，不再使用 isKeyWindow 硬门禁；
失焦/缩放通知和内容区域、拖动起点限制仍保留。
删除合成测试 focused=true 绕过。新增原生菜单按下 → 插入窗口 bookkeeping 事件 → 松开，
完整走生产转换与分发，断言 Help 页面实际打开；标题栏过滤和正常拖动同时验证。
5 项定向测试通过，当前构建见人工清单；不替代用户实际菜单和窗口操作验收。

## 未达标但 active 的重试入口（2026-09-09）

此前 Retry shot 仅在 AttemptEvaluated 显示，单杆进球但白球超出停球区时可能仍为 AttemptActive，
导致入口缺失。现在实际完成过击球且停稳后也显示 Retry shot，高级详情和收起面板同样保留。
正常面板用暂不可用的 Save replay 行容纳重试，避免增加普通面板行数；预览与真实结算后的保存不变。
Evaluation 增加仅用于说明的 feedback，列出未满足条件（球未进袋、偏离目标距离与上限等），
JSONL 上下文对应 training_feedback。不改变 succeeded/failed、分数、最大杆数或重试的前杆状态恢复语义。
实际停球训练进袋但白球过远回归验证 active 状态、outside target 反馈、普通/高级收起面板点击重试、
恢复球位、清空反馈与杆数、无点击穿透；人工验收仍待用户复试。

## 手动面板展开/收起（2026-09-09）

训练/回放页右上角增加固定 Show panel / Hide panel，独立于面板内条目布局；
绘制和命中使用同一个 inspectorToggleRect，完整按下/释放才切换，不传给瞄准或击球。
inspector_mode 日志：0 自动、1 手动展开、2 手动收起。手动展开忽略轨迹保护区域冲突，
允许面板覆盖轨迹；不取消字体、窗口尺寸等其他布局约束，也不声称通过原严格无覆盖布局门禁。
手动状态在当前页面持续，页面切换恢复自动；收起时重试/回放控制仍保留。
训练与回放的 Retina 指针回归覆盖自动收起→手动展开→下一帧保持→手动收起→空间充足仍保持→再展开，
验证没有球或相机状态变化；UI 回归明确验证固定按钮是面板外新增的 chrome 及其一致命中范围。

## 手动展开后的高度溢出（2026-09-09）

会话 session-1788956446135586-0 已收到 inspector_expand，但仍因 shell_layout_item_too_small 收起。
手动展开现在按可用高度保留可读完整行，剩余内容由滚轮逐行访问；不缩小字体，不再要求所有内容同时塞入面板。
firstVisibleRow 与 maximumScrollRow 限制滚动范围，绘制按同一源行偏移取条目；隐藏条目不参与命中。
面板内滚轮事件由 inspector_scroll 消费，不下传给相机或击球力度；原生滚轮记录正确的内容区坐标。
日志 inspector_scroll_row 记录当前请求偏移，内容变少后布局与下次滚动均限制到有效范围。
手动展开同时允许占用原自动模式的游戏避让区域；其他无效尺寸/字体错误仍保留，不把此模式当成严格无覆盖布局验收。
900×430、1×/2× 的高级详情绘制与指针测试覆盖初次展开、滚动到末尾、末行激活、内容收缩后限位，
并验证滚动不改变球和相机状态。4 项定向测试通过；真实桌面手感仍需用户复试。

## 原生整帧计时边界（2026-09-22，原生日志验证通过）

`swap_submitted.details` 新增 `timing_scope=event_loop_through_swap_completion`：

- `loop_through_swap_ms`：当前事件循环开始至 Cocoa `flushBuffer` 返回，包含输入、模拟、消费者、渲染与交换。
- `swap_to_swap_ms`：上次交换完成至本次交换完成，额外包含帧间日志等开销；第一帧不产生虚假的零值。
- `frame_timing_valid`：同一单调时钟的时间戳有限且顺序合法；非法边界不生成耗时。
- `includes_simulation=true`、`includes_display_swap=true`、`measures_scanout=false`：不是屏幕扫描显示测量。

原 `render_ms` / `swap_ms` 保留。纯时间边界回归和读取当前运行 JSONL 的原生 E2E 均已通过。
实际 session `1790076908726817-0` 含 59 帧、58 个相邻交换间隔；原件已保存到稳定基线证据目录的
`logs/native-frame-timing-session-1790076908726817-0.jsonl`（SHA256 `618552ed47331adfb82e2a35e9897f66e61c6915b7a4aa4e31791301cbe1851a`）。
自测截图、合成输入、日志及启动帧均会影响耗时，不能把这类日志直接作为稳定场景的 1080p/60 FPS 验收。
正式性能报告仍需固定机器/分辨率/质量档、预热与采样区间，并报告完整帧 P50/P95/P99 和各 pass 数据。

`scripts/summarize_presented_frame_timing.py <session目录> --warmup-frames 120`
可汇总单次完整会话的 P50/P95/P99，输出绑定原始 JSONL 的 SHA-256。
它拒绝轮转丢失早期帧、重复/缺失帧、旧计时字段、非有限值和不合法时间关系；第一帧不加入 cadence 统计。
统计使用 nearest-rank，三组序列使用相同帧集合。输出始终保留 `qualifies_as_product_frame_budget=false`，
不能以该工具运行成功替代独立的机器/场景/采样窗口与各 pass 验收。
每帧另记录实际 drawable 像素宽高、质量枚举和请求的 MSAA 样本数；统计报告列出各配置样本数，缺失配置显式为 null。
同次会话发生窗口缩放时，汇总不是单一 1080p 场景性能；请求 MSAA 也不冒充硬件最终生效样本数。
