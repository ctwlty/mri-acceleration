# nuclear_system

场景化核磁共振科研/教学客户端项目根目录。

## 当前内容

- `docs/`: 产品方案与技术设计
- `prototype/`: 静态原型与预览图
- `client/`: Qt/C++ 客户端骨架
- `.harness/`: Harness 变更记录与规则

## 启动客户端

进入 `client/` 后按 `client/README.md` 的步骤构建和运行。

## 开发约定

- 以后所有新代码都以此目录为项目根。
- 优先保持模板化流程，不把底层 DLL 细节直接暴露给用户。
