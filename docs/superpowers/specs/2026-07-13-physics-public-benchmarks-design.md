# BilliardGL 第二阶段：公开实验基准设计

日期：2026-07-13

## 1. 目标与边界

第二阶段把论文和公开实验转化为可离线运行、可审计、可重复的数据基准。它建立真实世界 ground truth，不修改摩擦、碰撞、旋转、球杆、库边或袋口物理公式，也不以实验结果调参。

所有实际参与验证或未来调优的数值必须提交到仓库。CI 不依赖论文网站、付费页面或本地 PDF。当前引擎超出实验区间时如实报告 `MODEL_MISMATCH`；输入条件或证据无法充分还原时报告 `REFERENCE_LIMITATION`。

本阶段接入四个主要来源：

- Mathavan、Jackson、Parkin，*Application of high-speed imaging to determine the dynamics of billiards*，DOI `10.1119/1.3157159`；
- Doménech-Carbó，*Independent friction-restitution description of billiard ball collisions*，DOI `10.1016/j.mechrescom.2023.104149`；
- Mathavan、Jackson、Parkin，*A theoretical analysis of billiard ball dynamics under cushion impacts*；
- Cross，*Impact of a cue with a billiard ball*，DOI `10.1177/17543371231184011`。

允许引入同一现象的同行评审实验或作者公开技术数据作为补充，但不能静默替代主要来源，不能用理论曲线、公开视频或项目自身输出冒充实验数据。

## 2. 总体架构

第二阶段新增 `Reference Dataset` 层，不复制生产物理执行逻辑：

```text
论文或公开实验
  -> raw_extracted.csv
  -> 单位转换与误差传播
  -> normalized.csv
  -> 预注册 calibration/holdout
  -> reference adapter
  -> canonical scenario + expectations
  -> production GameRuntime
  -> trace + analyzer
  -> 实验报告与严格 mismatch 对账
```

建议目录：

```text
tests/physics_validation/reference_data/
  mathavan_2009_high_speed/
  domenech_2023_ball_collision/
  mathavan_2010_cushion/
  cross_2023_cue_impact/
  supplemental/
```

每个来源是独立的版本化 reference package。reference adapter 只把 normalized 行转换成现有 scenario 和 expectation；执行仍通过第一阶段的 `GameRuntime`、automation protocol、trace 和 acceptance runner。

## 3. 数据包格式

每个数据包至少包含：

### 3.1 `manifest.json`

- 数据 schema version 和稳定 dataset ID；
- 作者、标题、期刊、年份、DOI、公开 URL；
- 精确页码、表号、图号或补充材料编号；
- 实验器材、球直径、质量、表面、库边、球杆和环境条件；
- 原始坐标系、单位、采样频率和测量方法；
- 论文声明的测量精度、置信区间或误差来源；
- 获取方式：原始表格、作者附件、人工录入或图表数字化；
- 原文许可状态和仓库内数值衍生数据说明；
- 可获得原文件的 SHA-256；不能提交原文件时仍保存本地校验值和获取步骤；
- 提取工具版本、提取日期、提取人与独立复核记录；
- 证据等级、器材适用性和理由。

受版权保护的论文 PDF 和原始图表不是运行依赖，只有明确允许再分发或存在授权证明时才提交。所有计算所需数值及其复现元数据必须提交。

### 3.2 `raw_extracted.csv`

保存论文中的原始数值和原始单位。不得覆盖异常点，不得把单位换算值反写为原始值。图表数字化数据同时保存坐标轴标定点、像素坐标、数据坐标转换和拟合残差。

### 3.3 `normalized.csv`

每行至少包含：

- dataset、series、case 和 point ID；
- 原始自变量和观测值；
- 统一后的 SI 或项目单位；
- measurement、digitization、conversion uncertainty；
- 误差传播后的 acceptance interval；
- calibration/holdout 标签；
- 来源页码、图表号和点位说明；
- WPA Pool 适用性或器材转换说明。

### 3.4 `split.json` 与提取记录

`split.json` 记录不可由 runner 参数覆盖的数据分组。`extraction.json` 或提取脚本记录轴标定、转换公式、舍入规则和输入哈希。两名复核者或两种独立提取结果的差异超过预注册上限时，数据点不得进入数值验收，只能标为 `REFERENCE_LIMITATION`。

## 4. 许可与离线复现策略

完整 normalized 数据是 CI 和未来调优的唯一运行输入。即使原论文页面下线或需要订阅，现有基准仍可离线执行。

每个数据包必须在加入数值点前完成许可审计。许可不明确不会阻止保存独立提取的事实数值与转换元数据，但禁止复制整篇论文、原始截图或大段图表。若衍生数值的再分发也存在明确限制，则该来源只保留审计元数据并报告 limitation，直到获得授权或合法的替代实验数据。

## 5. Calibration 与 Holdout

划分在查看项目仿真误差前预注册：

- 每个子系统约 1/3 calibration、2/3 holdout；
- 以完整实验序列为分组单位，不把同一曲线的相邻点拆到两组；
- calibration 覆盖低、中、高典型工况；
- 极端速度、入射角、旋转和滑杆边界优先进入 holdout；
- runner 不提供临时重分组参数；
- 修改 split 必须改变数据集版本，并在报告中显示。

第二阶段不使用 calibration 调参。第三阶段可以读取 calibration，但不得读取 holdout 结果来选择参数或模型分支。只有 holdout 满足验收区间，才能声明对应物理现象符合公开实验。

## 6. 来源与场景映射

### 6.1 Mathavan 2009 高速摄影

提取滚动/滑动摩擦、垂直撞库以及斜向球球碰撞数据，生成停止时间、停止距离、碰撞后速度、分离角和垂直反弹指标。论文使用 snooker 器材且报告约 1 mm 位置精度，因此可验证测量方法、趋势和部分无量纲关系；未经明确换算的材料参数不能直接作为 WPA Pool 通过标准。

### 6.2 Doménech 2023 球球碰撞

开放获取页面明确给出实验器材：regulation billiard ball 直径 6.1 cm、质量 205.0 g，PVC 支撑面，入射速度 `0.80 ± 0.05 m/s`，并测量碰撞和散射角。数据包保存 regulation billiard、钢、黄铜和橡胶实验，但只有器材条件可转换的系列进入 Pool 数值验收。其余系列用于趋势或 `REFERENCE_LIMITATION`。

### 6.3 Mathavan 2010 库边

提取入射速度、入射角、反弹速度、反弹角和旋转变化。论文中的 snooker 实验与引用的 pool 恢复系数必须分开保存；引用值不能冒充该论文的实验点。数据包在导入前必须锁定精确图号、轴范围、实验条件和数字化误差。

### 6.4 Cross 2023 球杆击球

提取不同垂直击点对应的初始线速度、角速度、法向/切向响应和滑杆边界。期刊摘要确认这些量来自实验，但全文可访问性受限；只有取得足够实验条件和图表后，数值点才进入 adapter。未取得的字段以逐项 limitation 登记，不能用简单冲量模型生成的理论值填充。

### 6.5 补充来源

补充数据位于 `supplemental/`，必须达到相同或更高证据等级，记录器材差异和转换依据。主要来源的未覆盖项仍保留 limitation，不因补充来源存在而伪装为已接入。

## 7. Reference Adapter

adapter 接受一个 normalized 数据点或实验序列，输出：

- 稳定 scenario/case ID；
- canonical ball、cue、table 和 simulation 条件；
- 指标名、观测值、acceptance interval 和证据引用；
- calibration/holdout 标签；
- 无法表达条件时的结构化 limitation。

adapter 不执行物理、不估算缺失实验值、不访问网络。相同 dataset version 和 case ID 必须生成逐字节一致的场景与 expectation。当前 scenario schema 无法表达 cue-tip offset 等条件时，先输出 limitation；是否扩展 schema 必须在实施计划中作为独立 TDD 任务处理。

## 8. 指标与验收

第二阶段新增或完善以下实验指标：

- `trajectory_position_rmse_mm`；
- `stopping_time_seconds`、`stopping_distance_cm`；
- `transition_to_rolling_time_seconds`；
- `post_collision_linear_velocity_cm_s`；
- `post_collision_angular_velocity_rad_s`；
- `separation_angle_degrees`；
- `cushion_rebound_speed_cm_s`、`cushion_rebound_angle_degrees`；
- `cue_impact_linear_speed_cm_s`、`cue_impact_angular_speed_rad_s`；
- `stick_slip_classification`。

每个指标的允许区间取预注册工程上限、论文测量不确定度以及数字化/转换误差传播结果中更宽且有证据支持的区间。不得因为仿真失败扩大容差。

分类规则：

- 可表达输入且稳定输出超出实验区间：`MODEL_MISMATCH`；
- 输入可表达但对应物理量尚未建模，例如角速度保持零：`MODEL_MISMATCH`；
- 实验条件缺失、器材不能换算或 schema 无法表达必要输入：`REFERENCE_LIMITATION`；
- trace 缺失、跳 tick、adapter 与进程不一致：`INTEGRATION_MISMATCH`；
- NaN、漏碰、能量异常等：`NUMERICAL_FAILURE`。

`MODEL_MISMATCH` 不能吸收数值或集成失败。

## 9. 严格失败清单

实验 mismatch 使用独立的 `expected_model_mismatches.json`，不与第一阶段数值算法 known failures 混合。CI 只在实际 `(dataset_id, case_id, code, metric)` 集合与清单完全一致时通过；新增失败和意外消失都失败。

`REFERENCE_LIMITATION` 使用独立 manifest，逐项记录缺少的证据、受影响指标和解除条件。未登记的新 limitation 失败。报告始终把 mismatch 和 limitation 显示为物理未通过，不得显示为 passed 或 skipped。

## 10. 报告

JSON、CSV 和 Markdown 报告分别展示 calibration 和 holdout，并包含：

- 每点预测值、实验值、误差和允许区间；
- 每序列 RMSE、最大偏差和通过率；
- dataset version、来源和精确引用；
- scenario、trace、构建 SHA-256 和重放命令；
- 当前模型缺失能力；
- mismatch、limitation 和数值失败的独立对账。

误差图所需的规范化绘图数据必须提交或生成到 artifact；图像只作辅助，数值报告是权威结果。

## 11. 执行分层与 CI

### PR 快速检查

验证 schema、哈希、raw→normalized 转换、split 隔离和每个数据包的代表性 case，目标为数分钟。

### 完整实验回归

运行所有 calibration 和 holdout 点，生成完整报告，供合并前手动执行和定时任务使用。

### 数据重建审计

重新执行 raw→normalized 转换并逐字节比较提交结果。数字化工具可以在本地依赖 PDF/图像处理软件，但普通 CI 和实验 runner 不依赖外部网页、受限 PDF 或网络。

数据损坏、转换不可重复、split 泄漏、来源缺失、新增 integration failure 和 numerical failure 均直接阻止 CI。已登记 mismatch 与 limitation 仍在报告中保持失败状态。

## 12. 实施前数据准入门槛

实施计划在添加任何数值点前，必须为每个主要来源列出：

- 精确页码、表格和图号；
- 可提取系列、点数和实验变量；
- 获取位置与许可结论；
- 数字化方法和预计误差；
- calibration/holdout 的确定分组；
- WPA Pool 适用性；
- 无法取得字段的 limitation。

未完成准入门槛的数据包可以先建立结构和验证器，但不能提交未经审计的数值。

## 13. 第二阶段完成标准

1. 四个主要来源都有完整 reference package；不能提取的部分有明确 limitation 和解除条件。
2. 所有实际计算数值离线落地，CI 不依赖外部网页。
3. raw→normalized、误差传播、哈希和 split 可以重复验证。
4. 每个可表达实验点都能驱动生产 runtime 并生成实验对比。
5. calibration/holdout 物理隔离有自动测试。
6. model mismatch、reference limitation、integration failure 和 numerical failure 严格分账。
7. 每个报告指标可追溯到数据点、场景、构建版本和论文位置。
8. 单场景可以离线重放。
9. 本阶段没有物理公式、材料参数或求解器优化。
