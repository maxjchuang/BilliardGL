# Phase 3 真实用户研究协议

日期：2026-07-27  
状态：**PENDING / NOT_RUN**  
研究目标：验证首次有效击球、击点控制、镜头往返与规则反馈是否达到 Phase 3 退出门。

本协议不包含研究结果。Codex、sub-agent、自动化脚本、项目成员或模拟用户均不得充当
参与者；flat proof、GPU capture 和工程自动化也不能替代真实用户研究。

## 1. 样本与招募

必须一次性形成八份完整、互不重复的匿名 session：

- 5 名 `project_new_user`：首次使用 BilliardGL，不是项目成员。
- 3 名 `billiards_experienced_user`：至少一年真实线下台球经验，不是项目成员。

两组都必须是真人，且 `agent_or_robot=false`。参与者只使用随机匿名 ID，格式为
`P3-XXXXXXXXXX`；观察员使用 `OBS-XXXXXXXX`。不记录姓名、邮箱、电话、地址、生日、
员工号、账号、照片或录音。开放回答中若意外出现个人信息，观察员必须停止提交、完成
脱敏并重新执行 PII review，不能把原文写入 evidence。

参与者与观察员不能是同一身份。每份 session 完成后双方分别确认
`phase3_user_study_attestation_v1`；缺任一确认、自填或复用 participant ID 都会失败。

## 2. 冻结环境

每位参与者开始前，观察员记录并复核同一组：

- executable/build SHA-256；
- 内容包 SHA-256；
- HUD token SHA-256；
- ProductShell token SHA-256；
- 594-case capture matrix SHA-256；
- 已完成的实际 GPU capture manifest SHA-256；
- 匿名设备编码、OS/GPU、键鼠等输入设备、logical/physical viewport 和 DPI。

八份 session 的绑定必须完全相同。任何 build、内容或 capture 变化都要求开启一轮新研究，
不能把两轮参与者拼接为八人样本。

## 3. 观察规则

- 使用单调时钟，从应用启动记为 `0 ms`；每一步记录相对毫秒。
- 观察员只读任务提示，不解释控件位置、快捷键、击点方向或正确答案。
- 参与者可自主打开帮助；观察员必须在对应 step 记录 `help_opened=true`。
- 每次误操作记录稳定 error code 或经过脱敏的简述。不得事后凭印象补写。
- 每项任务记录 start/end、每个 required step、错误、帮助次数和一条开放回答。
- 中断、崩溃或观察证据缺失时该 session 不得标记 COMPLETE；不得用估算时间补齐。

## 4. 四项任务

### A. 启动到首次有效击球

给参与者的提示只有：“请启动游戏并完成一次你认为有效的击球。”

依次记录：应用启动、HUD 可见、找到力度、找到击点、进入瞄准、释放击球、runtime 确认
有效击球。`valid_shot_at_ms` 必须等于最后一步时间。另由步骤推导“是否在找到力度与击点
之前打开帮助”，不能由观察员直接填写一个更有利的布尔值。

### B. 低杆与右塞

提示：“请分别设置一次低杆和一次右塞，并提交这两个击点设置。”

记录选择/提交两次动作及实际 `tip_offset_xy_R`。低杆要求 Y<0；右塞要求 X>0；所有值
必须在 [-1R, 1R]。本任务验证控件发现和提交，不要求实际击球结果符合某条路线。

### C. 俯视到低位往返

提示：“保持当前击球设置，切到俯视观察，再回到球员低位。”

切换前后序列化同一个 ShotControlModel 并记录 SHA-256。必须成功进入 overhead、恢复
player_low，且前后 intent SHA 完全相同；视觉镜头变化不能改变 aim、tip 或 power。

### D. 解释犯规与换手

展示冻结的犯规/换手产品状态，提示：“请用自己的话说明发生了什么，以及接下来由谁操作。”

记录开放回答和是否识别 foul reason / handoff。三名真实台球经验参与者随后还需分别解释：

1. 当前击点位于母球何处；
2. 当前击球方向如何表达；
3. 当前 assist level 提供什么、不会替玩家做什么。

观察员按冻结 rubric 将三项分别标记正确/错误，不能只写总分；原始开放回答必须保留且
通过 PII review。新用户的 experienced debrief 六个字段统一写 `N/A`。

## 5. 预注册门槛

validator 只在八份 COMPLETE session、双签和全部 SHA 绑定无误后计算 aggregate：

- 五名新用户首次有效击球中位数 ≤30 秒。
- 至少 4/5 新用户在找到力度与击点前没有打开帮助，即 ≥80%。
- 三名台球经验用户的击点、方向、assist 三项解释全部正确。
- 8/8 镜头往返不改变 ShotControlModel SHA。
- 8/8 完成四项任务。

aggregate 由 validator 根据 session 明细重算；研究执行者不得提前填写。若门槛未达到，
证据状态必须为 FAIL，不能删掉失败参与者、重新分类分组或只报告成功子集。

## 6. PENDING 与实际证据流程

1. 保留 `phase3-user-study-v1.pending.json` 原件不变；它只有匿名槽位，没有参与者结果。
2. 完成 GPU capture 和 build/content 冻结后，复制模板到外部研究产物目录。
3. 每位真人完成后填写一份 session，并由参与者/观察员双重确认和 PII review。
4. 八份齐全后运行 validator 计算 aggregate；不得手算后反向修改 session。
5. PENDING 模式成功只表示模板有效，固定输出 `participants=0/8 result=NOT_RUN`。

```bash
python3 scripts/generate_phase3_user_study_templates.py --output-dir docs/art
python3 scripts/validate_phase3_user_study.py \
  docs/art/phase3-user-study-v1.pending.json --pending-template
python3 -m unittest -v tests.visual.test_phase3_user_study
```

实际研究文件去掉 `--pending-template`。validator 对有效但未达门槛的完整研究返回 FAIL，
不会把“证据格式正确”混同为“用户研究通过”。

## 7. 冻结模板产物

- PENDING 模板：`docs/art/phase3-user-study-v1.pending.json`  
  SHA-256：`79d6510254ceaa7ac474d6c596b10a6134dbab77c2abafb3aa35342767c70218`
- 总研究 schema：`docs/art/phase3-user-study-v1.schema.json`  
  SHA-256：`123a34f7db9e223cdaed4db4838d47205e12cc64b01e27f03df75f024d4e8133`
- 单参与者 schema：`docs/art/phase3-user-study-participant-v1.schema.json`  
  SHA-256：`41452904511e5ed931c963f49dd04e426f35aa0a5c4f5e19e27afa365392b36d`

这些哈希只冻结研究模板和数据契约，不是参与者证据，也不表示 Phase 3 用户研究已通过。
