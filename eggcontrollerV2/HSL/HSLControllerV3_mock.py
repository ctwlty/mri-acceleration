# -*- coding: utf-8 -*-
import time
import threading
import random


class HSLController:
    HSLConnect = False
    speed = 65.0
    watingTime = 0.1
    ipAddress = "COM3"
    port = 115200

    registers = [[0, 0, 0] for _ in range(24)]

    _prepare_status = False
    _hand_status = 9999
    _skew_num = 0
    _recognition_status = False

    operate_lock = threading.Lock()

    STEPS_PER_POSITION = 2000
    DIRECTION_FORWARD = 1

    MOCK_MODE = True

    def __init__(self, ip="COM3", port=115200):
        self.ipAddress = ip
        self.port = port
        self.serial_conn = None
        print(f"[MOCK] HSLController初始化: 串口={ip}, 波特率={port}")

    def connect_HSL(self, ip="COM3", port=115200):
        self.ipAddress = ip
        self.port = port
        print(f"[MOCK] 连接电机控制器: 串口={ip}, 波特率={port}")
        self.HSLConnect = True
        return True

    def disconnect_HSL(self):
        print("[MOCK] 断开电机控制器连接")
        self.HSLConnect = False

    def _send_command(self, command):
        print(f"[MOCK] 发送指令: {command}")
        return "OK"

    def getCheckStatus(self):
        if not self.HSLConnect:
            return False
        return self.getCheckEggStatus()

    def getCheckEggStatus(self):
        # 检查第1号寄存器（现在是新蛋的位置）
        egg_list = self.handleGetGHSLValueList(1, 1, 1)
        if egg_list and (egg_list[0][0] == -1 or egg_list[0][1] == -1 or egg_list[0][2] == -1):
            return True
        # 如果第1号是空的，也返回True，允许采样新蛋
        if egg_list and egg_list[0][0] == 0 and egg_list[0][1] == 0 and egg_list[0][2] == 0:
            return True
        return False

    def getCheckLastStatus(self):
        egg_list = self.handleGetGHSLValueList(23, 23, 1)
        if egg_list and (egg_list[0][0] == -1 or egg_list[0][1] == -1 or egg_list[0][2] == -1):
            return True
        return False

    def setHSLWatingTime(self, wait_time):
        self.watingTime = wait_time

    def getPrepareStatus(self):
        return self._prepare_status

    def setPrepareStatus(self, value: bool):
        self._prepare_status = value

    def handleGetSkewNum(self):
        return self._skew_num

    def handleSetSkewNum(self, send_value: int):
        self._skew_num = send_value

    def handleGetRecognition(self):
        return self._recognition_status

    def handleSetRecognition(self, send_value: bool):
        self._recognition_status = send_value

    def handleNextGroup(self):
        if not self.HSLConnect:
            return False

        try:
            if self.getCheckLastStatus():
                return False

            if not self.getHandStatus():
                return False

            steps = self.STEPS_PER_POSITION
            self._send_command(f"MOVE_STEPS{steps}")
            time.sleep(0.1)
            return True

        except Exception as e:
            return False

    def setHSLSpeed(self, value: float):
        try:
            if value < 50:
                value = 50
            elif value > 200:
                value = 200
            self.speed = value
            self._send_command(f"SET_SPEED{value}")
            return True
        except Exception as e:
            return False

    def getHandStatus(self):
        return self._hand_status == 9999 or self._hand_status == 0

    def setHandRun(self):
        try:
            if self.getHandStatus():
                self._hand_status = 5
        except Exception as e:
            pass

    def handleGetGHSLValueList(self, start=1, end=24, type=0):
        if start < 1 or end > 24 or start > end:
            return []

        result_list = []
        for i in range(start - 1, end):
            result_list.append(self.registers[i])

        if type == 0:
            value1 = [item[0] for item in result_list]
            value2 = [item[1] for item in result_list]
            value3 = [item[2] for item in result_list]
            return [value1, value2, value3]
        else:
            return result_list

    def handleSetHSLValue(self, index, send_value=[0, 0, 0]):
        try:
            if index < 1 or index > 24:
                return

            with self.operate_lock:
                if len(send_value) >= 3:
                    self.registers[index - 1] = [send_value[0], send_value[1], send_value[2]]
                else:
                    temp_value = [0, 0, 0]
                    for i, val in enumerate(send_value):
                        if i < 3:
                            temp_value[i] = val
                    self.registers[index - 1] = temp_value

        except Exception as e:
            pass

    def handleSetHSLValueList(self, send_value=[[0, 0, 0]], start=1, end=24, type=0):
        try:
            if len(send_value) < (end - start + 1):
                return

            with self.operate_lock:
                for i in range(start, end):
                    y = i - start
                    if y < len(send_value):
                        value0, value1, value2 = 0, 0, 0
                        if len(send_value[y]) >= 3:
                            value0 = send_value[y][0]
                            value1 = send_value[y][1]
                            value2 = send_value[y][2]
                        elif len(send_value[y]) == 2:
                            value0 = send_value[y][0]
                            value1 = send_value[y][1]
                        elif len(send_value[y]) == 1:
                            value0 = send_value[y][0]

                        self.registers[i - 1] = [value0, value1, value2]

        except Exception as e:
            pass

    @staticmethod
    def list_available_ports():
        return [
            {'device': 'COM3', 'description': 'Mock Serial Port 1', 'hwid': 'USB\\VID_1234&PID_5678'},
            {'device': 'COM4', 'description': 'Mock Serial Port 2', 'hwid': 'USB\\VID_1234&PID_5679'},
        ]

    def mock_add_test_eggs(self, positions=None):
        if positions is None:
            positions = [0, 1, 2]
        for pos in positions:
            if 0 <= pos < 24:
                self.registers[pos] = [-1, -1, -1]


if __name__ == '__main__':
    print("=" * 50)
    print("MOCK HSLController 测试")
    print("=" * 50)

    hsl = HSLController()
    print(f"\n连接状态: {hsl.HSLConnect}")

    hsl.connect_HSL("COM3", 115200)
    print(f"连接后状态: {hsl.HSLConnect}")

    hsl.mock_add_test_eggs([0, 1, 2, 12, 23])
    print(f"\n寄存器状态: {hsl.handleGetGHSLValueList(1, 5, 1)}")

    print(f"\n采蛋位状态: {hsl.getCheckEggStatus()}")
    print(f"倒数第二位状态: {hsl.getCheckLastStatus()}")

    print("\n测试完成!")
