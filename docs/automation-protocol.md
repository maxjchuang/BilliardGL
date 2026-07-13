# BilliardGL 自动化协议 v1

## 启动

```bash
./build/check/Billiards --automation --transport stdio --headless
./build/check/Billiards --automation --transport stdio --rendered
```

`headless` 不初始化窗口、OpenGL 或音频，适合快速物理测试。`rendered` 创建真实 OpenGL 上下文并支持截图。两种模式都只在收到时间控制命令时推进物理。

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

进程首先输出 `ready`，其中包含 `protocol_version`、`mode`、`transport` 和已排序的 `capabilities`。客户端应以能力列表为准，不应根据模式猜测命令。

## 命令

生命周期与查询：`ping`、`get_capabilities`、`get_state`、`get_events`、`reset_game`、`clear_events`、`quit`。

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

- `set_ball {index,position?,velocity?,rotation_axis?,rotation_angle?,pocketed?}`。
- `load_scenario {balls}`：`balls` 必须有 16 项，每项包含 position 和 velocity；完整校验后原子替换。
- `set_player_state {current_player?,next_player?,illegal_shot?}`。

rendered 专用：`screenshot {path}`。它渲染当前 tick、保存二进制 PPM，再返回相同 tick；headless 返回 `unsupported_in_mode`。

## 状态和事件

`get_state` 返回 tick、16 个球的位置/速度/速度标量/旋转/落袋状态、balls_moving、aim、input、players、camera、hud、game_over 和事件历史。浮点断言应由客户端提供容差。

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

新传输实现 `AutomationTransport::readMessage/writeMessage`，只负责连接、分帧和 I/O。TCP、Unix Socket 或 WebSocket 适配器必须把相同 JSON 文本交给 `runAutomation` 和 `AutomationController`，不得实现游戏规则或改变协议语义。
