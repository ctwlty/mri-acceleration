# 电机控制系统使用说明

## 一、系统概述

本系统使用DM556步进电机驱动器+Arduino单片机替代原有的PLC控制器，用于控制电机移动，实现流水线位置切换。新的HSLController.py保持了原有所有接口，确保MainWindow_V3.py无需修改即可运行。

### 主要特点

- ✅ 保持原有接口，无需修改MainWindow_V3.py
- ✅ 通过串口与Arduino通信，控制DM556驱动器
- ✅ 支持速度调节（50-200 RPM）
- ✅ 不检查到达位置，直接控制移动距离
- ✅ 支持未来扩展到第二个电机

---

## 二、硬件连接

### 2.1 所需硬件

1. **Arduino Uno** 或兼容开发板
2. **DM556步进电机驱动器**
3. **步进电机**（根据实际需求选择）
4. **电源**（24V或根据电机要求）
5. **USB数据线**

### 2.2 连接示意图

```
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│   PC/电脑   │         │   Arduino   │         │   DM556     │
│             │         │             │         │  驱动器     │
│  USB接口    ├────────>│  USB接口    │         │             │
│ (串口通信)   │         │             │         │             │
└─────────────┘         │  Pin 2 (PUL)├────────>│  PUL+       │
                        │             │         │             │
                        │  Pin 3 (DIR)├────────>│  DIR+       │
                        │             │         │             │
                        │   GND       ├────────>│  PUL-       │
                        │             │         │  DIR-       │
                        └─────────────┘         │             │
                                                 │   A+ A-     │
                                                 │   B+ B-     │
                                                 │   (接电机)  │
                                                 └─────────────┘
                                                        │
                                                 ┌───────┴───────┐
                                                 │   步进电机    │
                                                 └───────────────┘
```

### 2.3 详细引脚连接


| Arduino引脚 | DM556驱动器 | 说明                        |
| ----------- | ----------- | --------------------------- |
| Pin 2       | PUL+        | 脉冲信号（正）              |
| Pin 3       | DIR+        | 方向信号（正）              |
| GND         | PUL-        | 脉冲信号（负）              |
| GND         | DIR-        | 方向信号（负）              |
| -           | 24V电源     | DM556供电（根据驱动器要求） |
| -           | A+ A- B+ B- | 连接步进电机线圈            |

### 2.4 DM556驱动器设置

根据实际电机设置以下参数：

- **细分设置（SW1-SW3）**：根据需要设置（如800细分）
- **电流设置（SW4-SW6）**：根据电机额定电流设置
- **脉冲模式（SW7-SW8）**：设置为脉冲+方向模式

---

## 三、软件安装

### 3.1 Arduino端

1. 安装Arduino IDE（从官网下载）
2. 打开 `stepper_motor_controller.ino` 文件
3. 选择正确的开发板型号（Tools > Board > Arduino Uno）
4. 选择正确的串口（Tools > Port）
5. 点击上传按钮，将代码烧录到Arduino

### 3.2 Python端

1. 安装Python依赖库：

```bash
pip install pyserial
```

2. 替换原有的HSLController.py：

   - 将新的 `HSLController.py` 复制到项目目录
   - 确保路径正确：`HSL/HSLController.py`
3. 测试连接：

```python
from HSL.HSLController import HSLController

# 创建控制器
hsl = HSLController()

# 列出可用串口
ports = HSLController.list_available_ports()
for port in ports:
    print(f"{port['device']}: {port['description']}")

# 连接（根据实际串口修改）
hsl.connect_HSL("COM3", 115200)

if hsl.HSLConnect:
    print("连接成功")
    hsl.disconnect_HSL()
```

---

## 四、HSLController函数使用说明

### 4.1 连接与断开

#### connect_HSL(ip, port)

连接电机控制器

**参数：**

- `ip` (str): 串口名称，如 "COM3"（Windows）或 "/dev/ttyUSB0"（Linux）
- `port` (int): 波特率，默认115200

**示例：**

```python
hsl.connect_HSL("COM3", 115200)
if hsl.HSLConnect:
    print("连接成功")
```

#### disconnect_HSL()

断开电机控制器连接

**示例：**

```python
hsl.disconnect_HSL()
```

---

### 4.2 电机控制

#### handleNextGroup()

移动到下一组（移动一个寄存器位置）

**返回值：**

- `bool`: True=移动指令已发送，False=失败

**特点：**

- 不检查是否到达指定位置
- 直接发送移动指令后返回
- 移动距离由 `STEPS_PER_POSITION` 参数控制

**示例：**

```python
result = hsl.handleNextGroup()
if result:
    print("移动指令已发送")
```

#### setHSLSpeed(value)

设置电机速度

**参数：**

- `value` (float): 速度（RPM），范围50-200

**示例：**

```python
hsl.setHSLSpeed(65.0)  # 设置为65 RPM
```

---

### 4.3 寄存器操作

#### handleGetGHSLValueList(start, end, type)

获取寄存器状态列表

**参数：**

- `start` (int): 起始寄存器编号（1-24）
- `end` (int): 结束寄存器编号（1-24）
- `type` (int): 返回格式
  - `0`: 返回三个分开的列表 `[value1_list, value2_list, value3_list]`
  - `1`: 返回嵌套列表 `[[v1,v2,v3], [v1,v2,v3], ...]`

**返回值：**

- `list`: 寄存器状态列表

**示例：**

```python
# 获取第13-15号寄存器状态
status = hsl.handleGetGHSLValueList(13, 15, 1)
print(status)  # 输出: [[0,0,0], [-1,0,0], [1,0,0]]
```

#### handleSetHSLValue(index, send_value)

设置单个寄存器状态

**参数：**

- `index` (int): 寄存器编号（1-24）
- `send_value` (list): 状态列表 `[状态1, 状态2, 状态3]`

**状态值说明：**

- `-1`: 待检测
- `0`: 无样品
- `1-4`: 检测等级

**示例：**

```python
hsl.handleSetHSLValue(13, [-1, 0, 0])  # 设置第13号寄存器为待检测
```

#### handleSetHSLValueList(send_value, start, end, type)

批量设置寄存器状态

**参数：**

- `send_value` (list): 2维数组，每个元素是 `[状态1, 状态2, 状态3]`
- `start` (int): 起始寄存器编号
- `end` (int): 结束寄存器编号
- `type` (int): 未使用（保持接口兼容）

**示例：**

```python
# 批量设置前3个寄存器
values = [[0,0,0], [0,0,0], [-1,0,0]]
hsl.handleSetHSLValueList(values, 1, 3)
```

---

### 4.4 状态查询

#### getCheckStatus()

检查是否可以采蛋（第13号寄存器）

**返回值：**

- `bool`: True=可以采蛋，False=不可以

**示例：**

```python
if hsl.getCheckStatus():
    print("可以采蛋")
```

#### getCheckEggStatus()

检查第13号寄存器是否有待检测项

**返回值：**

- `bool`: True=有待检测项，False=无

**示例：**

```python
if hsl.getCheckEggStatus():
    print("第13号寄存器有待检测项")
```

#### getCheckLastStatus()

检查第23号寄存器是否有待检测项

**返回值：**

- `bool`: True=有待检测项，False=无

**示例：**

```python
if hsl.getCheckLastStatus():
    print("第23号寄存器有待检测项")
```

#### getHandStatus()

获取机械臂运动状态

**返回值：**

- `bool`: True=待机，False=运行中

**示例：**

```python
if hsl.getHandStatus():
    print("机械臂待机")
```

---

### 4.5 其他功能

#### setHSLWatingTime(time)

设置流水线等待时间

**参数：**

- `time` (float): 等待时间（秒）

**示例：**

```python
hsl.setHSLWatingTime(0.5)
```

#### setPrepareStatus(value)

设置采样准备状态

**参数：**

- `value` (bool): True=准备，False=未准备

**示例：**

```python
hsl.setPrepareStatus(True)
```

#### getPrepareStatus()

获取采样准备状态

**返回值：**

- `bool`: 采样准备状态

**示例：**

```python
status = hsl.getPrepareStatus()
```

---

## 五、MainWindow_V3调用逻辑说明

### 5.1 初始化流程

```python
def __init__(self):
    super().__init__()
    self.hsl = HSLController()  # 创建控制器实例
    # ... 其他初始化代码
```

### 5.2 连接流程

```python
def HSLConnectBtn_click(self):
    # 1. 从UI获取串口和波特率
    ip = str(self.HslIpbox.text())  # 如 "COM3"
    port = int(self.Hslportbox.text())  # 如 115200

    # 2. 连接控制器
    self.hsl.connect_HSL(ip, port)

    # 3. 检查连接状态
    if self.hsl.HSLConnect:
        # 连接成功，启用相关按钮
        self.reset.setEnabled(True)
        self.autoHSL.setEnabled(True)
        # ...
    else:
        # 连接失败，显示错误
        self.createMsgBox("错误", "HSL连接错误")
```

### 5.3 移动到下一组

```python
def nextGroup_click(self):
    # 1. 检查连接状态和最后寄存器状态
    if self.hsl.HSLConnect and self.lastRegisterListStatuts():
        # 2. 发送移动指令
        ngRes = self.hsl.handleNextGroup()
        if ngRes:
            # 3. 更新寄存器状态
            statusList = self.hsl.handleGetGHSLValueList(1, 24, type=1)
            # 4. 更新UI显示
            self.updateEggStatusList_single.emit(statusList)
        else:
            print("移动失败或寄存器状态不允许")
```

### 5.4 设置寄存器

```python
def registerChangeBtn_click(self):
    # 1. 从UI获取寄存器编号和状态
    registerNum = int(self.LSXIndex.value())
    statusInput = self.statusList.text()

    # 2. 转换状态格式
    status = [int(statusInput), 0, 0]

    # 3. 设置寄存器
    self.hsl.handleSetHSLValue(registerNum, status)

    # 4. 更新UI
    self.updateEggStatus_single.emit(registerNum - 1, status)
```

### 5.5 设置速度

```python
def speedEditBtn_click(self):
    # 1. 从UI获取速度值
    speed = float(self.speedBox.text())

    # 2. 设置速度
    self.hsl.setHSLSpeed(speed)
```

### 5.6 自动运行模式

```python
def autoHSLMethod(self):
    while self.isAutoRunning:
        try:
            # 1. 检查是否可以采蛋
            if self.hsl.getCheckStatus() and self.lastRegisterListStatuts():
                # 2. 执行采样
                self.samplingBtn_click()
            else:
                # 3. 等待
                time.sleep(self.hsl.watingTime)

            # 4. 移动到下一组
            self.nextGroup_click()
        except Exception as e:
            time.sleep(self.hsl.watingTime)
```

---

## 六、参数配置

### 6.1 关键参数说明

在 `HSLController.py` 中，以下参数需要根据实际机械结构调整：

```python
# 每个寄存器位置对应的步数
# 需要根据实际机械结构调整
# 示例：如果每个位置间隔需要电机转2圈，每圈200步，则为400
STEPS_PER_POSITION = 2000  # 默认值，请根据实际情况修改
```

### 6.2 如何确定STEPS_PER_POSITION

**步骤：**

1. 确认电机每转步数（STEPS_PER_REV）：通常为200步/圈
2. 确认DM556驱动器细分设置：如设置为8细分，则有效步数为200×8=1600步/圈
3. 确认机械传动比：如丝杆导程为5mm/圈，每移动一个位置需要移动10mm
4. 计算所需步数：步数 = (移动距离 / 导程) × 步数/圈

**示例计算：**

- 电机步数：200步/圈
- 驱动器细分：8细分
- 有效步数：200 × 8 = 1600步/圈
- 丝杆导程：5mm/圈
- 每个位置移动距离：10mm
- 所需步数：(10 / 5) × 1600 = 3200步

```python
STEPS_PER_POSITION = 3200  # 修改为此值
```

---

## 七、故障排查

### 7.1 常见问题

#### 问题1：连接失败

**现象：**

```
连接失败: could not open port 'COM3'
```

**解决方法：**

1. 检查串口名称是否正确
2. 检查USB线是否连接
3. 检查Arduino是否已上电
4. 在设备管理器中查看串口号（Windows）
5. 尝试列出所有可用串口：

```python
ports = HSLController.list_available_ports()
for port in ports:
    print(f"{port['device']}: {port['description']}")
```

---

#### 问题2：电机不转动

**现象：**

- 发送移动指令后电机无反应

**检查步骤：**

1. 检查Arduino与DM556的连接（PUL、DIR引脚）
2. 检查DM556供电是否正常（24V）
3. 检查电机线圈连接（A+/A-、B+/B-）
4. 检查DM556细分设置（SW1-SW3）
5. 检查DM556脉冲模式（SW7-SW8应为脉冲+方向模式）
6. 检查电机电流设置（SW4-SW6）

---

#### 问题3：移动距离不准确

**现象：**

- 每次移动的距离不一致或不符合预期

**解决方法：**

1. 检查 `STEPS_PER_POSITION` 参数设置
2. 检查DM556细分设置是否与代码一致
3. 检查机械传动是否有松动
4. 检查电机是否丢步（可能是速度过快或负载过重）
5. 降低速度测试：

```python
hsl.setHSLSpeed(50)  # 降到最低速度测试
```

---

#### 问题4：速度调节无效

**现象：**

- 调整速度后电机速度不变

**解决方法：**

1. 检查是否正确调用 `setHSLSpeed()` 函数
2. 检查速度值是否在有效范围内（50-200 RPM）
3. 重新上传Arduino代码
4. 检查串口通信是否正常

---

#### 问题5：串口通信超时

**现象：**

```
发送指令失败: TIMEOUT
```

**解决方法：**

1. 检查串口线长度（建议使用短于1.5米的线）
2. 降低波特率测试（如改为9600）：

```python
hsl.connect_HSL("COM3", 9600)
```

3. 同时修改Arduino代码中的BAUD_RATE为相同值
4. 检查是否有其他程序占用串口

---

### 7.2 调试技巧

#### 技巧1：使用串口监视器测试Arduino

1. 打开Arduino IDE
2. 选择 Tools > Serial Monitor
3. 波特率设置为115200
4. 发送测试指令：

```
CONNECT
SET_SPEED65
MOVE_STEPS1000
GET_STATUS
```

#### 技巧2：添加调试日志

在 `HSLController.py` 中添加更多日志：

```python
logger.info(f"发送指令: {command}")
logger.debug(f"接收响应: {response}")
```

#### 技巧3：使用示波器检查脉冲信号

- 将示波器探头连接到PUL+引脚
- 观察脉冲波形是否正常
- 测量脉冲频率是否符合预期

---

## 八、扩展功能

### 8.1 添加第二个电机

**步骤：**

1. 在Arduino代码中启用电机2：

```cpp
#define ENABLE_MOTOR2  // 取消注释
```

2. 修改 `HSLController.py`，添加电机2控制方法：

```python
def moveStepsMotor2(self, steps):
    self._send_command(f"MOVE_STEPS2{steps}")
```

3. 修改Arduino代码的 `moveSteps()` 函数，支持 `motor_id` 参数：

```cpp
void moveSteps(long steps, int motor_id) {
    // 根据motor_id选择引脚
}
```

---

### 8.2 添加位置传感器（可选）

如果需要检测是否到达指定位置，可以：

1. 在机械结构上添加限位开关或光电传感器
2. 将传感器连接到Arduino数字引脚
3. 修改代码，在移动过程中检测传感器状态
4. 到达位置后发送确认信号给Python

---

## 九、注意事项

1. **电源安全**：

   - DM556驱动器需要24V电源，注意正负极
   - 电机电流较大，确保电源线足够粗
   - 避免短路，使用带保险的电源
2. **信号隔离**：

   - 建议在Arduino和DM556之间添加光耦隔离
   - 防止电机干扰影响Arduino
3. **机械保护**：

   - 在行程两端设置限位开关
   - 避免电机撞击机械限位
4. **温度控制**：

   - DM556驱动器和电机工作时会产生热量
   - 确保散热良好
5. **软件更新**：

   - 更新HSLController.py后，重新测试所有功能
   - 更新Arduino代码后，重新烧录

---

## 十、技术支持

如遇到问题，请提供以下信息：

1. 使用的Arduino型号
2. DM556驱动器的设置（SW1-SW8的状态）
3. 电机型号和参数
4. 错误日志或异常现象描述
5. 相关代码片段

---

## 附录A：完整测试代码

```python
# test_motor_controller.py
from HSL.HSLController import HSLController
import time

def test_motor_controller():
    print("=== 电机控制器测试 ===")

    # 创建控制器
    hsl = HSLController()

    # 列出可用串口
    print("\n可用串口:")
    ports = HSLController.list_available_ports()
    for port in ports:
        print(f"  {port['device']}: {port['description']}")

    # 连接控制器（根据实际串口修改）
    print("\n尝试连接...")
    hsl.connect_HSL("COM3", 115200)

    if not hsl.HSLConnect:
        print("连接失败！请检查串口和Arduino")
        return

    print("连接成功！")

    # 测试1：设置速度
    print("\n测试1: 设置速度")
    hsl.setHSLSpeed(65)
    time.sleep(1)

    # 测试2：移动到下一组
    print("\n测试2: 移动到下一组")
    result = hsl.handleNextGroup()
    if result:
        print("移动指令已发送")
    else:
        print("移动失败")

    time.sleep(3)

    # 测试3：获取寄存器状态
    print("\n测试3: 获取寄存器状态")
    status = hsl.handleGetGHSLValueList(1, 3, 1)
    print(f"寄存器1-3状态: {status}")

    # 测试4：设置寄存器
    print("\n测试4: 设置寄存器")
    hsl.handleSetHSLValue(13, [-1, 0, 0])
    time.sleep(1)
    status = hsl.handleGetGHSLValueList(13, 13, 1)
    print(f"寄存器13状态: {status}")

    # 断开连接
    print("\n断开连接...")
    hsl.disconnect_HSL()
    print("测试完成！")

if __name__ == '__main__':
    test_motor_controller()
```

运行测试：

```bash
python test_motor_controller.py
```

---

## 附录B：串口通信协议文档

### 指令格式

所有指令以换行符（\n）结尾

### 指令列表


| 指令       | 格式            | 参数   | 说明     | 响应            |
| ---------- | --------------- | ------ | -------- | --------------- |
| CONNECT    | `CONNECT`       | 无     | 测试连接 | `OK` 或 `ERROR` |
| MOVE_STEPS | `MOVE_STEPS<n>` | n=步数 | 移动n步  | `OK` 或 `ERROR` |
| SET_SPEED  | `SET_SPEED<n>`  | n=RPM  | 设置速度 | `OK` 或 `ERROR` |
| GET_STATUS | `GET_STATUS`    | 无     | 获取状态 | `Status: ...`   |
| RESET      | `RESET`         | 无     | 重置系统 | `OK`            |

### 示例

```
PC -> Arduino: CONNECT
Arduino -> PC: OK

PC -> Arduino: SET_SPEED65
Arduino -> PC: OK

PC -> Arduino: MOVE_STEPS2000
Arduino -> PC: OK

PC -> Arduino: GET_STATUS
Arduino -> PC: Status: Speed=65 RPM, Running=Yes
```

---

## 更新日志

### v1.0 (2025-01-XX)

- 初始版本发布
- 实现基础电机控制功能
- 保持原有HSLController接口
- 支持速度调节
- 不检查到达位置，直接控制移动距离
