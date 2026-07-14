# 第三阶段主题 0：调优治理与物理参数架构设计

日期：2026-07-14

## 1. 目标

建立所有后续物理优化共用的参数、调优、冻结和验证边界。该主题不改变摩擦或碰撞公式；它保证生产参数可追溯，并防止优化程序直接使用预注册验证结果。

## 2. PhysicsProfile

`PhysicsProfile` 由带显式单位的值对象组成：

- `BallProperties`：半径、质量、惯量模型和材料；
- `SurfaceProperties`：滑动摩擦、滚动阻力和旋转衰减参数；
- `CueProperties`：有效质量、杆头几何、摩擦和输入映射；
- `CushionProperties`：库鼻高度、恢复、摩擦和有效域；
- `SolverSettings`：权威 tick、接触容差、迭代和安全上限。

每个参数记录名称、数值、单位、合法范围、来源、器材适用性和公式版本。Chinese Pool 生产默认值与实验 profile 分离。场景覆盖只能生成新的不可变 profile，不能写回默认值。

## 3. 调优接口

Calibration runner 只加载 manifest 中固定为 `calibration` 的完整分组，输出：

- 目标函数与逐点误差；
- 参数向量和边界；
- 参数敏感度；
- calibration 报告及输入数据哈希；
- 候选模型标识和重放命令。

runner 不提供 split override，也不提供在同一优化过程加载验证结果的选项。不可执行、数值失败和 evidence limitation 不能作为缺失样本静默忽略。

## 4. 冻结记录

运行预注册验证集前必须生成并提交 freeze record，至少包含：

- 模型和公式版本；
- 完整参数向量、单位和来源；
- 代码与构建 SHA-256；
- calibration 数据版本、目标和报告哈希；
- 解析与数值测试结果；
- 预先声明的验收指标和阈值；
- 已知 limitation 与预期 mismatch。

冻结记录不可原地改写。任何公式、参数、代码或验收目标变化都生成新候选 ID。

## 5. 验证数据状态

完整数值继续提交到仓库，因此隔离是可审计流程而不是秘密保管：

- `calibration`：允许拟合和模型诊断；
- `validation`：只允许冻结后的验收；
- `spent`：已经参与模型选择，只能作为回归；
- `confirmation`：模型冻结后新增、未参与选择的最终确认数据。

验证失败后可以提出新物理假设，但不能把同一验证集继续称为盲测。强真实性声明最终依赖 confirmation 数据。

## 6. 失败与审计

验证 runner 不修改参数，只生成结果和状态迁移建议。失败必须保留候选、报告和重放信息。数据 split、容差或证据等级变化需要数据集版本升级及独立审查。

自动测试要求：

- calibration runner 拒绝 validation/spent/confirmation 行；
- validation runner 拒绝无冻结记录或哈希不匹配的构建；
- 参数越界、缺单位、缺来源和非有限值失败；
- profile 覆盖不改变 production default；
- 相同候选和数据生成确定性报告；
- 状态变更有不可删除审计记录。

## 7. 完成标准

主题完成时，后续模型只能通过 `PhysicsProfile` 取得物理参数，calibration 与验证具有独立入口，冻结记录可重复校验，完整数值仍可离线复现，同时文档明确不宣称密码学盲测。
