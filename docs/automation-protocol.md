# BilliardGL 自动化协议 v1

## 启动

```bash
./build/check/Billiards --automation --transport stdio --headless
./build/check/Billiards --automation --transport stdio --rendered --renderer legacy
```

`headless` 不初始化窗口、OpenGL 或音频，适合快速物理测试。`rendered` 创建真实 OpenGL 上下文并支持截图。两种模式都只在收到时间控制命令时推进物理。

`--renderer` 接受 `legacy` 或 `modern`，迁移期默认为 `legacy`。macOS 图形模式会在任何 GLUT 初始化前进入 OpenGL 4.1 Core modern application；当前非 macOS 构建选择 `modern` 会明确失败，不会静默回退。headless 接受该启动配置，但始终不创建图形 backend。

## 消息格式

stdin/stdout 使用 JSON Lines，每行一个 UTF-8 JSON 对象，单行上限 1 MiB。协议消息只写 stdout，诊断只写 stderr。

请求：

```json
{"id":1,"version":1,"command":"step","params":{"ticks":3}}
```

成功与失败响应：

```json
{"id":1,"ok":true,"result":{"tick":3}}
{"id":1,"ok":false,"error":{"code":"invalid_argument","message":"ticks must be between 0 and 100000"}}
```

进程首先输出 `ready`，其中包含 `protocol_version`、`mode`、`transport`、`renderer_backend` 和已排序的 `capabilities`。rendered 模式的 `renderer_backend` 为实际选择的 backend；headless 固定为 `none`。客户端应以能力列表为准，不应根据模式猜测命令。

## 命令

生命周期与查询：`ping`、`get_capabilities`、`get_state`、`get_events`、`reset_game`、`clear_events`、`quit`。

物理 trace：

- `start_physics_trace`：从后续物理 tick 开始记录；每个 tick 恰好对应一帧。
- `stop_physics_trace`：停止追加但保留已经记录的帧。
- `clear_physics_trace`：清空记录和丢帧计数，不改变游戏 tick 或状态。
- `get_physics_trace {after_tick?,limit?}`：返回 tick 大于 `after_tick` 的帧；`limit` 为 1–1000，默认 1000。结果包含 `frames`、`dropped_frames` 和 `has_more`。

runtime 最多保留 10000 帧，超出时淘汰最旧帧并增加 `dropped_frames`。每帧包含球的位置、线速度、线加速度、三维角速度、控制输入、接触事件、线动量、平动动能和最大穿透深度。位置单位为 cm，线速度为 cm/s，线加速度为 cm/s²，角速度为 rad/s，冲量为 N·s，能量为 J。当前物理模型尚未计算真实旋转动力学，因此 `angular_velocity` 默认保持为零；该字段不能由视觉旋转角反推。

原始输入：

- `key_down/key_up {key}`：单个字符；支持游戏现有的帮助、瞄准、摄像机和力度按键。
- `special_key {key}`：`left`、`right`、`up`、`down`。
- `mouse_move {x,y}`。
- `mouse_button {button,state,x,y}`：button 为 `left/right/other`，state 为 `down/up`。
- `mouse_wheel {direction}`。
- `resize {width,height}`。

语义操作：

- `toggle_aim`、`toggle_help`、`shoot`。
- `set_aim_yaw {yaw}`，弧度。
- `set_shot_power {power}`，范围 0–200。
- `orbit_camera {yaw_delta,pitch_delta}`。
- `pan_camera {x_delta,z_delta}`。
- `zoom_camera {delta}`。

确定性时间：

- `step {ticks}`：推进 0–100000 个固定 tick。
- `run_until {condition,max_steps}`：逐 tick 推进；`max_steps` 默认 10000、最大 1000000。
- condition 支持 `balls_stopped`、`ball_collision`、`rail_collision`、`ball_pocketed`、`cue_ball_pocketed`、`eight_ball_pocketed`、`shot_ended`。

测试专用场景：

- `set_ball {index,position?,velocity?,angular_velocity?,rotation_axis?,rotation_angle?,pocketed?}`。
- `load_scenario {scenario}`：加载 canonical physics scenario v1 或 v2；能力列表分别通过 `physics_scenario_v1` 与 `physics_scenario_v2_cue_input` 声明支持。成功加载会原子替换状态、把仿真 tick 和事件序号重置到零并清空旧 trace。
- `load_scenario {balls}`：兼容旧格式；`balls` 必须有 16 项，每项包含 position 和 velocity，完整校验后原子替换。
- `set_player_state {current_player?,next_player?,illegal_shot?}`。

rendered 专用：`screenshot {path}`。它渲染当前 tick、保存二进制 PPM，再返回相同 tick；headless 返回 `unsupported_in_mode`。

## 状态和事件

`get_state` 返回 tick、16 个球的位置/速度/速度标量/旋转/落袋状态、balls_moving、aim、input、players、camera、hud、game_over 和事件历史。浮点断言应由客户端提供容差。

### 轨迹与离台诊断字段

当前序列化器还输出以下只读诊断字段；它们不构成新的控制命令，也不改变 `step` 的推进语义。读取旧记录时不得把缺失的诊断值当作已观测的零值。

- `get_state.balls[].trajectory_generation` 为十进制字符串，保留完整 uint64 精度；不得先转为 JavaScript Number。它标识轨迹代次，不是 tick 或事件序号。
- 状态与 trace 的球对象仅在离台时输出 `out_of_play: true`。它与 `pocketed` 分开，不能把离台球解释成正常进袋。
- trace 接触的 `kind` 可为 `ball_ball`、`rail`、`pocket` 或 `support`；`support` 是支撑接触，不能计作球球碰撞或碰库。消费者遇到未知类别应保留原值，而非默认映射为球球接触。
- 接触记录包含 `whole_tick_time_of_impact_seconds`、`first_position_at_contact_cm`、`second_position_at_contact_cm`，以及字符串类型的 `trajectory_generation_before/after`。局部 `time_of_impact_seconds` 与整 tick 时间不可混用。
- `administrative_reset_after_capture` 表示捕获后的管理性复位；仅为真时输出 `administrative_reset_position_cm`。绘制轨迹时应分段，不把复位位置变化画成物理飞行。
- `pocket_vertical_shaft` 描述袋口竖直通道接触。solver event 的 `position_projections` 则按记录顺序保留 `ordinal`、`iteration`、`contact_index`、`ball_index`、字符串 `trajectory_generation` 及 `before_cm/after_cm`；这些是求解器位置校正，不是额外运动 tick。

这些字段的序列化与完整整数精度由 `tests/automation_protocol_tests.cpp` 检查；命令兼容性须另运行 headless 客户端测试，不能仅凭字段增加或源码哈希变化判定通过。

schema v2 额外要求 `cue_impact`，包含 cue ball、物理 cue 速度/质量、单位方向、仰角、以 cm 和球半径比例表示且相互一致的二维 tip offset，以及 chalk 状态。runtime 和每个后续 trace frame 会原样返回这组请求输入；state 中的 `cue_impact_support` 逐项列出 `exactly_consumable_fields`、`unsupported_codes` 和 `shot_executed`。该 capability 只表示“可解析和追踪”，不表示引擎已经支持竖直偏杆、cue 质量或从 cue 速度到游戏力度的物理映射；当前加载 v2 不会自动击球。

`cue_contact` 始终包含 `microtrace_schema_version: 1` 和 `microsteps`。普通 v4 击球的 `microsteps` 是空数组；启用冻结接触模型的 v5 击球会按求解顺序返回完整微迹。冻结求解失败时，游戏状态与规则事件会回滚，但 `get_state.cue_contact.error_code` 和已经产生的 `microsteps` 仍会保留，便于定位失败。

每个微步包含带单位后缀的球杆位置、速度、加速度、压缩量与压缩率、法向/切向力和累计冲量、动能/弹性能/耗散能/能量残差、最大穿透、刚性求解残差与迭代数，以及该步的 16 球状态和接触约束数组。位置分别使用 `m`（球杆瞬态量）或 `cm`（游戏球状态），速度使用 `m_s` 或 `cm_s`，力使用 `n`，冲量使用 `n_s`，能量使用 `j`。JSON 不会输出 NaN 或 Infinity；非有限状态会先转化为稳定失败码。

事件包含 `event`、单调递增 `sequence` 和产生它的 `tick`。`get_events {after_sequence}` 只返回更大的序号。历史最多保留 10000 条。

## 错误码

- `parse_error`：JSON 无法解析；进程继续读取下一行。
- `invalid_request`：请求结构或字段类型错误。
- `unsupported_version`：协议版本不受支持。
- `unknown_command`、`unknown_key`。
- `invalid_argument`、`invalid_state`。
- `condition_not_met`：有限等待达到上限。
- `unsupported_in_mode`。
- `screenshot_failed`、`transport_error`。

## Python 客户端

```python
from automation_client import AutomationClient

with AutomationClient("build/check/Billiards") as game:
    game.toggle_aim()
    game.set_aim_yaw(0.0)
    game.set_shot_power(80)
    game.shoot()
    game.run_until("balls_stopped")
    print(game.state()["balls"])
```

客户端仅依赖 Python 标准库，提供请求超时、结构化 `AutomationError` 和异常清理。

## 扩展传输

新传输实现 `AutomationTransport::readMessage/writeMessage`，只负责连接、分帧和 I/O。TCP、Unix Socket、WebSocket 或其他 socket 适配器必须把完全相同的命令、状态和 trace JSON 对象交给 `runAutomation` 和 `AutomationController`，包括 `cue_contact` 的版本化微迹；不得删减字段、实现游戏规则或改变协议语义。
