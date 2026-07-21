/*
  Arduino UNO 控制 DM556 步进电机
  功能：通过串口发送步数，实现指定角度（步数）的正转或反转
  指令格式：输入整数步数，正数正转，负数反转，以换行符结束
  示例：
    输入 1600  → 正转 1600 步
    输入 -3200 → 反转 3200 步
    输入 0     → 无效指令

  引脚定义：
    PUL_PIN = 9   (脉冲)
    DIR_PIN = 8   (方向)
    ENA_PIN = 7   (使能)

  细分设置与 STEPS_PER_REV 的对应关系：
    8 细分   → 1600
    16 细分  → 3200
    32 细分  → 6400   (当前默认)
  请根据 DM556 的实际拨码开关设置修改该值。

  转速控制：SPEED_DELAY 数值越小，转速越快；数值越大，转速越慢。
  建议范围：400 ~ 2000，当前默认 200 为中高速。
*/

#include <Arduino.h>

// 引脚定义
#define PUL_PIN 8
#define DIR_PIN 9
#define ENA_PIN 10

// 电机参数（可修改）
const long STEPS_PER_REV = 6400;   // 每转步数（32细分）
const int SPEED_DELAY = 200;       // 脉冲间隔（μs），控制转速

// 串口接收缓存
String inputString = "";
bool stringComplete = false;

// 函数声明
void stepMotor(long steps);
void parseAndExecute();

void setup() {
  // 初始化引脚
  pinMode(PUL_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  digitalWrite(ENA_PIN, LOW);      // 使能电机（LOW=使能，HIGH=断电）

  // 初始化串口
  Serial.begin(9600);
  while (!Serial);                  // 等待串口连接（Leonardo等需要）
  Serial.println("Motor Controller Ready");
  Serial.println("Send steps (positive=forward, negative=backward) followed by Enter");
  Serial.print("Example: 1600   or   -3200\n");
}

void loop() {
  // 读取串口数据
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (inputString.length() > 0) {
        stringComplete = true;
      }
    } else {
      inputString += ch;
    }
  }

  // 有完整指令时执行
  if (stringComplete) {
    parseAndExecute();
    inputString = "";
    stringComplete = false;
    Serial.println("Ready for next command.");
  }
}

// 解析并执行运动指令
void parseAndExecute() {
  inputString.trim();                 // 去除首尾空格
  if (inputString.length() == 0) return;

  long steps = inputString.toInt();   // 将字符串转换为长整型

  if (steps == 0) {
    Serial.println("Error: steps cannot be zero.");
    return;
  }

  // 设置方向
  if (steps > 0) {
    digitalWrite(DIR_PIN, HIGH);      // HIGH = 正转
    Serial.print("Forward ");
  } else {
    digitalWrite(DIR_PIN, LOW);       // LOW = 反转
    steps = -steps;                   // 转为正值用于计数
    Serial.print("Backward ");
  }
  Serial.print(steps);
  Serial.println(" steps...");

  // 执行转动（阻塞，转动期间无法接收新指令）
  stepMotor(steps);

  Serial.println("Done.");
}

// 核心步进函数：发送指定数量的脉冲
void stepMotor(long steps) {
  for (long i = 0; i < steps; i++) {
    digitalWrite(PUL_PIN, HIGH);
    delayMicroseconds(SPEED_DELAY);
    digitalWrite(PUL_PIN, LOW);
    delayMicroseconds(SPEED_DELAY);
  }
}