/*
 * 步进电机控制器 - 用于DM556驱动器
 * 通过串口接收指令，控制步进电机移动
 *
 * 硬件连接：
 * - Arduino <-> DM556驱动器
 *   - Pin 2 -> PUL+ (脉冲信号)
 *   - Pin 3 -> DIR+ (方向信号)
 *   - GND   -> PUL-, DIR- (共地)
 * - Arduino <-> PC
 *   - USB   -> 串口通信 (波特率115200)
 *
 * 指令协议：
 * - CONNECT      - 测试连接，返回OK
 * - MOVE_STEPS<n>- 移动n步，返回OK
 * - SET_SPEED<n> - 设置速度为n RPM，返回OK
 * - GET_STATUS   - 获取状态，返回OK
 * - RESET        - 重置，返回OK
 *
 * 支持扩展：
 * - 预留第二个电机的引脚（Pin 4, 5）
 * - 可通过修改宏定义启用
 */

// ==================== 配置参数 ====================

// 串口配置
#define BAUD_RATE 115200

// 电机1引脚配置
#define PUL_PIN_1  2    // 脉冲信号
#define DIR_PIN_1  3    // 方向信号

// 电机2引脚配置（预留，暂未启用）
// #define ENABLE_MOTOR2  // 取消注释以启用
#ifdef ENABLE_MOTOR2
#define PUL_PIN_2  4
#define DIR_PIN_2  5
#endif

// 电机参数
#define DEFAULT_SPEED 65      // 默认速度 (RPM)
#define MIN_SPEED 50          // 最小速度 (RPM)
#define MAX_SPEED 200         // 最大速度 (RPM)
#define STEPS_PER_REV 200     // 电机每转步数（根据实际电机调整）
#define MICROS_PER_REV (60000000L / (DEFAULT_SPEED * STEPS_PER_REV))  // 每步时间(微秒)

// 命令缓冲区
#define CMD_BUFFER_SIZE 64

// ==================== 全局变量 ====================

// 电机状态
float current_speed = DEFAULT_SPEED;  // 当前速度 (RPM)
bool motor_running = false;           // 电机运行标志

// 命令处理
char cmd_buffer[CMD_BUFFER_SIZE];
int cmd_index = 0;
bool cmd_complete = false;

// ==================== 函数声明 ====================

void setup();
void loop();
void parseCommand();
void moveSteps(long steps, int motor_id = 1);
void setSpeed(float rpm);
bool checkConnection();
void resetSystem();

// ==================== 主函数 ====================

void setup() {
  // 初始化串口
  Serial.begin(BAUD_RATE);
  Serial.setTimeout(100);  // 100ms超时

  // 初始化电机引脚
  pinMode(PUL_PIN_1, OUTPUT);
  pinMode(DIR_PIN_1, OUTPUT);
  digitalWrite(PUL_PIN_1, LOW);
  digitalWrite(DIR_PIN_1, LOW);

#ifdef ENABLE_MOTOR2
  pinMode(PUL_PIN_2, OUTPUT);
  pinMode(DIR_PIN_2, OUTPUT);
  digitalWrite(PUL_PIN_2, LOW);
  digitalWrite(DIR_PIN_2, LOW);
#endif

  // 初始化命令缓冲区
  memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
  cmd_index = 0;
  cmd_complete = false;

  // 等待串口稳定
  delay(1000);

  Serial.println("Motor Controller Ready");
}

void loop() {
  // 读取串口数据
  while (Serial.available() > 0) {
    char c = Serial.read();

    // 收到换行符，命令完成
    if (c == '\n' || c == '\r') {
      cmd_buffer[cmd_index] = '\0';  // 字符串结束符
      if (cmd_index > 0) {
        cmd_complete = true;
      }
      cmd_index = 0;
    }
    // 缓冲区未满，存储字符
    else if (cmd_index < CMD_BUFFER_SIZE - 1) {
      cmd_buffer[cmd_index++] = c;
    }
    // 缓冲区溢出，重置
    else {
      cmd_index = 0;
      memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
    }
  }

  // 处理完整命令
  if (cmd_complete) {
    parseCommand();
    cmd_complete = false;
    memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
  }

  // 防止串口缓冲区溢出
  if (Serial.available() > 128) {
    while (Serial.available() > 0) {
      Serial.read();
    }
  }
}

// ==================== 命令解析 ====================

void parseCommand() {
  // 去除首尾空格
  String cmd = String(cmd_buffer);
  cmd.trim();

  if (cmd.length() == 0) {
    return;
  }

  Serial.print("Received: ");
  Serial.println(cmd);

  // CONNECT - 测试连接
  if (cmd == "CONNECT") {
    if (checkConnection()) {
      Serial.println("OK");
    } else {
      Serial.println("ERROR");
    }
  }

  // MOVE_STEPS<n> - 移动n步
  else if (cmd.startsWith("MOVE_STEPS")) {
    long steps = cmd.substring(10).toInt();
    if (steps != 0) {
      moveSteps(steps, 1);
      Serial.println("OK");
    } else {
      Serial.println("ERROR: Invalid steps");
    }
  }

  // SET_SPEED<n> - 设置速度
  else if (cmd.startsWith("SET_SPEED")) {
    float rpm = cmd.substring(9).toFloat();
    if (rpm >= MIN_SPEED && rpm <= MAX_SPEED) {
      setSpeed(rpm);
      Serial.println("OK");
    } else {
      Serial.println("ERROR: Invalid speed");
    }
  }

  // GET_STATUS - 获取状态
  else if (cmd == "GET_STATUS") {
    Serial.print("Status: Speed=");
    Serial.print(current_speed);
    Serial.print(" RPM, Running=");
    Serial.println(motor_running ? "Yes" : "No");
  }

  // RESET - 重置
  else if (cmd == "RESET") {
    resetSystem();
    Serial.println("OK");
  }

  // 未知命令
  else {
    Serial.println("ERROR: Unknown command");
  }
}

// ==================== 电机控制函数 ====================

void moveSteps(long steps, int motor_id) {
  if (steps == 0) {
    return;
  }

  motor_running = true;

  // 选择电机
  int pul_pin = (motor_id == 1) ? PUL_PIN_1 : PUL_PIN_2;
  int dir_pin = (motor_id == 1) ? DIR_PIN_1 : DIR_PIN_2;

  // 设置方向
  if (steps > 0) {
    digitalWrite(dir_pin, HIGH);  // 正方向
  } else {
    digitalWrite(dir_pin, LOW);   // 反方向
    steps = -steps;               // 转换为正数
  }

  // 计算脉冲间隔（微秒）
  // RPM = (steps_per_minute) / (steps_per_rev)
  // pulse_interval = 60,000,000 / (RPM * steps_per_rev)
  unsigned long pulse_interval = 60000000UL / (current_speed * STEPS_PER_REV);

  // 生成脉冲
  for (long i = 0; i < steps; i++) {
    // 上升沿
    digitalWrite(pul_pin, HIGH);
    delayMicroseconds(pulse_interval / 2);

    // 下降沿
    digitalWrite(pul_pin, LOW);
    delayMicroseconds(pulse_interval / 2);

    // 检查是否收到停止命令（可选）
    if (Serial.available() > 0) {
      char c = Serial.peek();
      if (c == 'S') {  // 紧急停止命令 'S'
        Serial.read();  // 清除字符
        break;
      }
    }
  }

  motor_running = false;
}

void setSpeed(float rpm) {
  // 限制速度范围
  if (rpm < MIN_SPEED) {
    rpm = MIN_SPEED;
  } else if (rpm > MAX_SPEED) {
    rpm = MAX_SPEED;
  }

  current_speed = rpm;
}

// ==================== 辅助函数 ====================

bool checkConnection() {
  // 检查引脚状态
  return digitalRead(PUL_PIN_1) == LOW && digitalRead(DIR_PIN_1) == LOW;
}

void resetSystem() {
  // 重置速度
  current_speed = DEFAULT_SPEED;

  // 停止所有电机
  digitalWrite(PUL_PIN_1, LOW);
  digitalWrite(DIR_PIN_1, LOW);

#ifdef ENABLE_MOTOR2
  digitalWrite(PUL_PIN_2, LOW);
  digitalWrite(DIR_PIN_2, LOW);
#endif

  motor_running = false;

  // 清空串口缓冲区
  while (Serial.available() > 0) {
    Serial.read();
  }
}