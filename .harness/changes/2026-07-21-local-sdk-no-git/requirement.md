# Requirement

## User Request

把大体积 SDK 和运行产物从 Git 仓库里去掉，但保留本机可直接使用的方式。

## Success Criteria

- SDK、DLL、构建目录、日志和扫描数据不再作为仓库内容提交。
- 本机已有 SDK 的电脑可以按约定直接使用，不影响开发和测试。
- 仓库里有明确说明，避免后续再次误提交。

## Out Of Scope

- 不改真实 SDK 接口。
- 不改设备控制逻辑。
- 不删除本机磁盘上的 SDK 文件。
