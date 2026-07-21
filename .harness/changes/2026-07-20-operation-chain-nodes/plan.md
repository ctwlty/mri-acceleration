# Plan

## UX Structure

将原先一排 `QLabel` 胶囊替换为横向操作链：

1. 推荐模板
2. 准备与预检
3. 定位与采集
4. 获取图像
5. 处理与重建
6. 质控与交接

每个节点使用卡片式结构，显示编号、节点标题和当前模板对应说明。节点之间使用方向箭头连接。

## Implementation

- 新增 `makeOperationNode()` 创建操作链节点。
- 使用 `m_operationDetails[6]` 保存每个节点的动态详情。
- 将 `setWorkflowPills()` 改为 `setOperationChain()`。
- 为操作链滚动区、节点、编号、标题、详情、箭头增加 QSS。

## Acceptance

- 构建通过。
- 离屏启动通过。
- 不触碰设备控制与参数下发逻辑。
