# -*- coding: utf-8 -*-
import time
import threading
import serial
import serial.tools.list_ports
from logger.robot_logger import logger


class HSLController:
    HSLConnect = False
    ipAddress = "COM3"
    port = 9600

    registers = [[0, 0, 0] for _ in range(24)]

    operate_lock = threading.Lock()

    def __init__(self, ip="COM3", port=9600):
        self.ipAddress = ip
        self.port = port
        self.serial_conn = None

    def connect_HSL(self, ip="COM3", port=9600):
        self.ipAddress = ip
        self.port = port

        try:
            self.serial_conn = serial.Serial(
                port=ip,
                baudrate=port,
                timeout=5,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE
            )

            self.serial_conn.reset_input_buffer()
            self.serial_conn.reset_output_buffer()

            time.sleep(1)
            
            response = self.serial_conn.readline().decode().strip()
            
            if "Ready" in response:
                self.HSLConnect = True
                logger.info(f"电机控制器连接成功: {ip}")
            else:
                self.HSLConnect = False
                logger.warning(f"电机控制器响应异常: {response}")

        except serial.SerialException as e:
            self.HSLConnect = False
            logger.exception(f"电机控制器连接失败: {str(e)}")

        except Exception as e:
            self.HSLConnect = False
            logger.exception(f"电机控制器连接异常: {str(e)}")

    def disconnect_HSL(self):
        try:
            if self.serial_conn and self.serial_conn.is_open:
                self.serial_conn.close()
                self.HSLConnect = False
                logger.info("电机控制器已断开连接")
        except Exception as e:
            logger.exception(f"断开连接失败: {str(e)}")

    def _send_command(self, command):
        if not self.HSLConnect or not self.serial_conn or not self.serial_conn.is_open:
            logger.error("电机控制器未连接")
            return None

        try:
            full_command = command + "\n"
            self.serial_conn.write(full_command.encode())
            self.serial_conn.flush()
            
            response = ""
            timeout = time.time() + 10
            while time.time() < timeout:
                if self.serial_conn.in_waiting > 0:
                    line = self.serial_conn.readline().decode().strip()
                    response += line + "\n"
                    if "Done." in line:
                        break
                    if "Error:" in line:
                        break
            
            return response.strip()

        except Exception as e:
            logger.exception(f"发送指令失败: {command}, 错误: {str(e)}")
            return None

    def getCheckStatus(self):
        if not self.HSLConnect:
            return False
        return self.getCheckEggStatus()

    def getCheckEggStatus(self):
        egg_list = self.handleGetGHSLValueList(13, 13, 1)
        if egg_list and (egg_list[0][0] == -1 or egg_list[0][1] == -1 or egg_list[0][2] == -1):
            return True
        return False

    def getCheckLastStatus(self):
        egg_list = self.handleGetGHSLValueList(23, 23, 1)
        if egg_list and (egg_list[0][0] == -1 or egg_list[0][1] == -1 or egg_list[0][2] == -1):
            return True
        return False

    def handleNextGroup(self):
        if not self.HSLConnect:
            logger.error("电机控制器未连接，无法移动")
            return False

        try:
            if self.getCheckLastStatus():
                logger.warning("第23号寄存器存在待检测项，暂不移动")
                return False

            PUSH_OUT_STEPS = -1600
            PUSH_IN_STEPS = 1600

            logger.info("开始推出鸡蛋...")
            response = self._send_command(str(PUSH_OUT_STEPS))
            if not response or "Done." not in response:
                logger.warning(f"推出失败: {response}")
                return False
            logger.info(f"推出成功: {PUSH_OUT_STEPS}步")

            logger.info("开始放入鸡蛋...")
            response = self._send_command(str(PUSH_IN_STEPS))
            if not response or "Done." not in response:
                logger.warning(f"放入失败: {response}")
                return False
            logger.info(f"放入成功: {PUSH_IN_STEPS}步")

            return True

        except Exception as e:
            logger.exception(f"移动到下一组失败: {str(e)}")
            return False

    def handleGetGHSLValueList(self, start=1, end=24, type=0):
        if start < 1 or end > 24 or start > end:
            logger.error(f"寄存器范围错误: start={start}, end={end}")
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
                logger.error(f"寄存器编号错误: {index}")
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

                logger.info(f"设置寄存器 {index} 为 {self.registers[index - 1]}")

        except Exception as e:
            logger.exception(f"设置寄存器失败: {str(e)}")

    def handleSetHSLValueList(self, send_value=[[0, 0, 0]], start=1, end=24):
        try:
            if len(send_value) < (end - start + 1):
                logger.warning(f'send_value长度比需要修改的流水线长度大')
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

            logger.info(f"批量设置寄存器 {start}-{end} 完成")

        except Exception as e:
            logger.exception(f"批量设置寄存器失败: {str(e)}")

    @staticmethod
    def list_available_ports():
        ports = serial.tools.list_ports.comports()
        port_list = []
        for port in ports:
            port_list.append({
                'device': port.device,
                'description': port.description,
                'hwid': port.hwid
            })
        return port_list