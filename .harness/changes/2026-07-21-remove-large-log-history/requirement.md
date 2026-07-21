# Requirement

## User Request

仓库推送被单个 224MB 日志文件拦住了，需要把这个大文件从 Git 历史里清掉，但保留本机文件和正常源码。

## Success Criteria

- 远端不再拒绝 200MB 上限。
- `error_2026.4.10.log` 不再出现在 Git 历史对象中。
- 本机 SDK/构建/日志目录仍然可用，不被误删。

## Out Of Scope

- 不改设备控制逻辑。
- 不改产品页面。
- 不删除本机磁盘上的日志文件。
