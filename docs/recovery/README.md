# 维护备份与继续开发

当前进度和未闭合验收以 `../project-checkpoint-20261021.md` 为入口。本目录的源码恢复包是额外备份，主工程源码/测试/资源仍以 Git 中普通文件为准。

## 研究源码恢复包

`20261021-maintenance/SOURCE_ARCHIVE.json` 记录分卷顺序、长度和 SHA256；四个 `.zip.partNN` 拼接后是标准 ZIP。包内按 SHA 去重，索引保留各原路径、字节数、模式与 SHA。包括证据/美术目录的源代码、文档、小 JSON，以及小型表格/配置/许可文件，覆盖 100756 个路径。

它**不包含全部原始证据**：大型 JSON、轨迹日志、截图、可执行文件、对象文件和外部资产保留在本机原路径。新机器不能仅凭此包声称重现完整验收；需要完整本地证据和资产。没有删除原证据来制造备份完整的假象。

先校验分卷，再验证所需前缀：

```sh
python3 scripts/restore_evidence_sources.py --destination "$PWD" --prefix docs/evidence/20261021-physics-P0-curved-stick-successor-v132/ --verify-only
```

缺失源码可去掉 `--verify-only` 恢复。脚本只写缺失文件，已有文件若 SHA 不同会拒绝覆盖。可用 `--destination /absolute/empty/directory` 恢复到独立位置查看；原历史 seal 里的绝对路径需要原环境，不能将移址恢复自动当作已通过的封存。

## 本地空间

原始证据和构建目录通过 `.gitignore` 排除，不代表允许删除。需要额外保留 `/Users/bytedance/Projects/github/BilliardGL-evidence/m4/`。目前跨任务构建依赖复杂，没有批准整目录清理。

`scripts/compact_local_evidence.py` 使用 macOS `ditto --hfsCompression` 进行透明压缩。每个文件先复制、验证 SHA，再原子替换；路径、内容、权限和时间由复制工具保留，内容相同才替换。硬链接/符号链接、已压缩文件跳过，无节省时保留原文件。操作逐文件记录原 SHA 和前后分配空间；失败会停止，不把未处理文件计入节省。

压缩后的文件可直接被现有构建、分析和图片工具读取，无需恢复步骤。复制到不支持压缩的文件系统时可能重新占用完整空间。不要把逻辑文件大小误当作实际已分配磁盘空间。

本次维护不代表新增游戏验收通过；没有替换默认资产、物理或音频。

本机 Git 维护时设置了 `gc.auto=0`、`gc.pruneExpire=never`，避免后续验证期间自动打包抢占资源，也避免自动过期本地恢复对象。需要时由主 Agent 单独安排 Git 整理；不要用强制清理不可达对象代替空间盘点。

本次实际结果见 `20261021-maintenance/RESULT.md`。
