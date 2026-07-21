# -*- coding: utf-8 -*-
import os

import numpy as np
from PyQt5 import QtWidgets, QtCore, QtGui
from PyQt5.QtWidgets import QWidget, QDesktopWidget, QApplication, QMessageBox, QPushButton, QVBoxLayout, QLineEdit, QDialog, QLabel
from PyQt5.QtGui import QPainter, QPixmap, QTransform, QImage
from PyQt5.QtCore import Qt, pyqtSignal, QPropertyAnimation, QPoint, QTimer, QEasingCurve

import threading
import time
from functools import partial
import matplotlib.pyplot as plt



import sys
# from HSL.HSLControllerV4 import HSLController
from HSL.HSLControllerV3_mock import HSLController
from Iface.HCController import HCController
from message.message import Message
from logger.robot_logger import logger

_translate = QtCore.QCoreApplication.translate

class Ui_MainWindow(QtWidgets.QMainWindow):
    registerNum = 24
    list_lock = threading.Lock()
    isAutoRunning = True
    autoRunComplete = True
    nextGroupActive = True
    save_path = 'webdata/data/'
    displayIndex = 0
    samplingComplete = True
    samplingSuccessCount = 0  # 采样成功次数

    updateEggStatusList_single = pyqtSignal(list)
    updateEggStatus_single = pyqtSignal(int, list)
    updateEggImg_single = pyqtSignal(int, list, bool)
    animation_single = pyqtSignal()
    afterNextEvent_single = pyqtSignal()
    
    def __init__(self):
        super().__init__()
        self.hsl = HSLController()
        self.hc = HCController()
        plt.figure()
        plt.close()
        self.updateEggStatusList_single.connect(self.updateEggStatusList)
        self.updateEggStatus_single.connect(self.updateEggStatus)
        self.updateEggImg_single.connect(self.updateEggImg)
        self.animation_single.connect(self.animationRunning)
        self.afterNextEvent_single.connect(self.afterNextEvent)

        self.message = Message(self.save_path)
        self.algorithmThread = threading.Thread(target=self.algorithm, daemon=True)
        self.algorithmThread.start()
        self.messageThread = threading.Thread(target=self.message.start_running, daemon=True)
        self.messageThread.start()
        self.setupUi()
        self.initList()

    def initList(self):
        self.totalList = []
        self.registerList = []
        # 开始时不创建空框，有数据才添加
        self.eggGroupList = []
        # 采样计数器，用于轮着显示mock0-4.png
        self.samplingCount = 0

    def btn_Event(self):
        self.HslConnectBtn.clicked.connect(self.HSLConnectBtn_click)
        self.HslDisconnectBtn.clicked.connect(self.HSLDisconnectBtn_click)
        self.reset.clicked.connect(self.reset_click)
        self.autoHSL.clicked.connect(self.autoHSL_click)
        self.manualHSL.clicked.connect(self.manualHSL_click)
        self.nextGroup.clicked.connect(self.nextGroup_thread)
        self.samplingBtn.clicked.connect(self.samplingBtn_click_sync)

        self.HslConnectBtn.setEnabled(True)
        self.HslDisconnectBtn.setEnabled(False)
        self.reset.setEnabled(False)
        self.autoHSL.setEnabled(False)
        self.manualHSL.setEnabled(False)
        self.nextGroup.setEnabled(False)
        self.samplingBtn.setEnabled(True)

    def createMsgBox(self, title, content):
        self.msg = QMessageBox()
        self.msg.setWindowModality(Qt.WindowModality.NonModal)
        self.msg.setWindowTitle(title)
        self.msg.setText(content)
        self.msg.setStandardButtons(QMessageBox.Ok)
        self.msg.show()
    def HSLConnectBtn_click(self):
        self.HslConnectBtn.setText(_translate("MainWindow", "HSL连接中"))
        self.HslDisconnectBtn.setEnabled(False)
        self.HslConnectBtn.setEnabled(False)
        self.hsl.connect_HSL(str(self.HslIpbox.text()), int(self.Hslportbox.text()))
        if self.hsl.HSLConnect:
            self.HslConnectBtn.setText(_translate("MainWindow", "连接"))
            self.reset_click()
            self.HslDisconnectBtn.setEnabled(True)
            self.HslConnectBtn.setEnabled(False)
            self.reset.setEnabled(True)
            self.autoHSL.setEnabled(True)
            self.manualHSL.setEnabled(False)
            self.nextGroup.setEnabled(True)
        else:
            self.HslConnectBtn.setText(_translate("MainWindow", "连接"))
            self.createMsgBox("错误", "HSL连接错误")
            self.HslDisconnectBtn.setEnabled(False)
            self.HslConnectBtn.setEnabled(True)
            self.reset.setEnabled(False)
            self.autoHSL.setEnabled(False)
            self.manualHSL.setEnabled(False)
            self.nextGroup.setEnabled(False)
            logger.warning("HSL连接错误")

    def HSLDisconnectBtn_click(self):
        self.hsl.disconnect_HSL()
        if self.hsl.HSLConnect == False:
            self.HslDisconnectBtn.setEnabled(False)
            self.HslConnectBtn.setEnabled(True)
            self.reset.setEnabled(False)
            self.autoHSL.setEnabled(False)
            self.manualHSL.setEnabled(False)
            self.nextGroup.setEnabled(False)

    def reset_click(self):
        if self.hsl.HSLConnect:
            try:
                self.list_lock.acquire()
                statusList = [[0, 0, 0] for i in range(self.registerNum)]
                timestamp = int(time.time())

                self.hsl.handleSetHSLValueList(statusList)
                self.registerList = []
                self.totalList = []

                for i in range(self.registerNum):
                    item = {
                        "id": timestamp - i,
                        'status': [0, 0, 0],
                        'rgb_image': [],
                        'kspace_image': [],
                    }
                    self.totalList.append(item)
                    self.registerList.append(item)

                for i in reversed(range(self.scrollAreaWidgetContents_2.layout().count())):
                    widget = self.scrollAreaWidgetContents_2.layout().itemAt(i).widget()
                    if widget:
                        widget.deleteLater()
                self.eggGroupList = []

                self.updateEggStatusList_single.emit(statusList)

                # 重置采样成功计数
                self.samplingSuccessCount = 0
                self.total_label.setText(_translate("MainWindow", "采样成功：0次"))
                self.list_lock.release()
            except Exception as e:
                logger.exception(f"重置寄存器出现异常,{str(e)}")

    def autoHSL_click(self):
        if self.hsl.HSLConnect:
            self.HslConnectBtn.setEnabled(False)
            self.HslDisconnectBtn.setEnabled(False)
            self.reset.setEnabled(False)
            self.autoHSL.setEnabled(False)
            self.manualHSL.setEnabled(True)
            self.nextGroup.setEnabled(False)
            self.samplingBtn.setEnabled(False)

            self.isAutoRunning = True
            self.autoHSLThread = threading.Thread(target=self.autoHSLMethod, daemon=True)
            self.autoHSLThread.start()

    def manualHSL_click(self):
        if self.hsl.HSLConnect:
            self.isAutoRunning = False
            timer = time.time()
            while True:
                if self.autoRunComplete == True:
                    break
                if timer - time.time() > 1000:
                    logger.error("自动模式关闭失败")
                    return
                time.sleep(1)
                
            self.HslConnectBtn.setEnabled(False)
            self.HslDisconnectBtn.setEnabled(True)
            self.reset.setEnabled(True)
            self.autoHSL.setEnabled(True)
            self.manualHSL.setEnabled(False)
            self.nextGroup.setEnabled(True)
            self.samplingBtn.setEnabled(True)

    def autoHSLMethod(self):
        self.autoRunComplete = True
        while self.isAutoRunning:
            self.autoRunComplete = False
            try:
                if self.hsl.getCheckStatus() and self.lastRegisterListStatuts():
                    # 直接调用同步版本，不启动新线程
                    self.samplingBtn_click_sync()
                else:
                    time.sleep(self.hsl.watingTime)
                # 只在不是自动模式时才下一组？或者等待更长时间
                time.sleep(1)
                self.nextGroup_click()
            except Exception as e:
                time.sleep(self.hsl.watingTime)
                logger.exception(f"自动模式出现异常,{str(e)}")
            self.autoRunComplete = True
            time.sleep(2)

    def algorithm(self):
        while True:
            try:
                time.sleep(0.5)

                if self.message.reciveQueue.empty() == False:
                    item = self.message.getReciveQueue()
                    id = item['id']
                    status = item['status']
                    kspace_image = item['kspace']
                    rgb1_image = item['rgb1']
                    rgb2_image = item['rgb2']
                    rgb3_image = item['rgb3']
                    rgb1_image_predict = item['rgb1_predict']
                    rgb2_image_predict = item['rgb2_predict']
                    rgb3_image_predict = item['rgb3_predict']
                    rgbList = [rgb1_image, rgb2_image, rgb3_image]
                    rgbList2 = [rgb1_image_predict, rgb2_image_predict, rgb3_image_predict]
                    
                    self.list_lock.acquire()
                    try:
                        for i in range(len(self.registerList) - 1):
                            if int(self.registerList[i]['id']) == int(id):
                                if self.hasAfterNext:
                                    updateIndex = i
                                else:
                                    updateIndex = i - 1
                                self.updateEggStatus_single.emit(i, status)
                                self.hsl.handleSetHSLValue(i + 1, status)
                                self.registerList[i]['status'] = status
                                self.totalList[i]['status'] = status
                                self.registerList[i]['kspace_image'] = [kspace_image, kspace_image, kspace_image]
                                self.registerList[i]['rgb_image'] = rgbList2
                                self.registerList[i]['rgb_image_original'] = rgbList
                                self.totalList[i]['rgb_image'] = rgbList2
                                self.totalList[i]['rgb_image_original'] = rgbList
                                self.totalList[i]['kspace_image'] = [kspace_image, kspace_image, kspace_image]
                                break
                    finally:
                        self.list_lock.release()

            except Exception as e2:
                logger.exception(f"请求算法接口异常,{str(e2)}")

    def lastRegisterListStatuts(self):
        # 如果registerList为空或只有一个元素，直接返回True
        if len(self.registerList) < 2:
            return True
        # 检查最后一个有数据的寄存器是否正在分析
        last_index = len(self.registerList) - 1
        if -1 in self.registerList[last_index]['status']:
            logger.warning(f"{last_index+1}号寄存器存在正在分析的状态位，请等待算法模块分析，或人工调整")
            return False
        return True

    hasAfterNext = False

    def nextGroup_click(self):
        self.nextGroupActive = True
        
        # 如果没有连接，模拟连接成功（mock模式）
        if not self.hsl.HSLConnect:
            self.hsl.HSLConnect = True
        
        if self.lastRegisterListStatuts():
            self.list_lock.acquire()
            self.hasAfterNext = False
            ngRes = False
            try:
                self.animation_single.emit()
                ngRes = self.hsl.handleNextGroup()
                if ngRes:
                    statusList = self.hsl.handleGetGHSLValueList(1, self.registerNum, type=1)
                    timestamp = int(time.time())
                    statueItem = {
                        "id": timestamp,
                        'status': [0, 0, 0],
                        'rgb_image': [],
                        'kspace_image': [],
                    }
                    # 如果registerList满了，才移除最后一个，否则不删除
                    if len(self.registerList) >= self.registerNum:
                        self.registerList.pop(len(self.registerList) - 1)
                    self.registerList.insert(0, statueItem)
                    
                    if len(self.totalList) >= self.registerNum:
                        self.totalList.pop(len(self.totalList) - 1)
                    self.totalList.insert(0, statueItem)
                    
                    # 处理状态逻辑
                    popitem = self.registerList[len(self.registerList) - 1] if len(self.registerList) > 1 else statueItem
                    # 移除机械臂相关代码
                    
                    # 更新状态
                    for i in range(min(self.registerNum, len(self.registerList))):
                        if i < len(statusList):
                            self.registerList[i]['status'] = statusList[i]
                            self.totalList[i]['status'] = statusList[i]
                else:
                    self.nextGroupActive = False
                    logger.error("流水线移动中或23号寄存器存在未分析状态位")
            except Exception as e:
                logger.exception(f"下一组出现异常,{str(e)}")
            finally:
                # 总是释放锁
                self.list_lock.release()
            
            # 在锁释放后再触发afterNextEvent
            if ngRes:
                self.afterNextEvent_single.emit()
        else:
            self.nextGroupActive = False

    def nextGroup_thread(self):
        thread = threading.Thread(target=self.nextGroup_click, daemon=True)
        thread.start()

    def afterNextEvent(self):
        self.list_lock.acquire()

        self.hasAfterNext = True
        try:
            # 保存旧框的图片信息（用于恢复）
            old_images = []
            for item in self.eggGroupList:
                old_images.append({
                    'rgb_image': item.get('rgb_image', []),
                    'kspace_image': item.get('kspace_image', []),
                    'status': item.get('status', [0,0,0])
                })
            
            # 移除所有旧框
            for item in self.eggGroupList:
                self.horizontalLayout_4.removeWidget(item['eggWidget'])
            
            # 创建新UI：新框在最前面
            self.eggGroupList = []
            
            # 新框（位置1）- 空框，等待下次采样
            newEggItem = self.createEgg(0, [0, 0, 0])
            newEggItem['rgb_image'] = []
            newEggItem['kspace_image'] = []
            self.eggGroupList.append(newEggItem)
            self.horizontalLayout_4.addWidget(newEggItem['eggWidget'])
            
            # 把registerList中有数据的框依次加入（位置2...）
            # eggGroupList[0]已经是空框，现在添加有数据的框
            # 从registerList[0]开始读取（因为采样时数据保存在registerList[0]）
            for i in range(len(self.registerList)):
                # 检查是否有图片数据
                rgb_image = self.registerList[i].get('rgb_image', [])
                if len(rgb_image) == 0 or (len(rgb_image) > 0 and rgb_image[0] == ''):
                    continue  # 跳过空框
                
                status = self.registerList[i].get('status', [0, 0, 0])
                kspace_image = self.registerList[i].get('kspace_image', [])
                
                # 创建框
                eggItem = self.createEgg(i, status)
                eggItem['rgb_image'] = rgb_image
                eggItem['kspace_image'] = kspace_image
                self.eggGroupList.append(eggItem)
                self.horizontalLayout_4.addWidget(eggItem['eggWidget'])
            
            print(f"[DEBUG afterNextEvent] eggGroupList长度: {len(self.eggGroupList)}")
            print(f"[DEBUG afterNextEvent] registerList长度: {len(self.registerList)}")
            
            # 更新标号
            for i, item in enumerate(self.eggGroupList):
                item['grouptitle'].setText(_translate("MainWindow", "流水线位置编号：" + str(i + 1)))
            
            statusList2 = [item['status'] for item in self.registerList]
            self.updateEggStatusList_single.emit(statusList2)
            
            # 在锁外更新图片显示，避免死锁
            self.list_lock.release()
            
            # 更新图片显示：使用eggGroupList中的数据
            # eggGroupList[0]是空框，从索引1开始更新
            for i in range(1, len(self.eggGroupList)):
                rgb_image = self.eggGroupList[i].get('rgb_image', [])
                if len(rgb_image) > 0 and rgb_image[0]:
                    self.updateEggImg_single.emit(i, rgb_image, True)
                    print(f"[DEBUG afterNextEvent] 更新图片 index={i}, path={rgb_image[0]}")
                    
        except Exception as e:
            logger.exception(f"下一组事件后处理异常,{str(e)}")
            self.list_lock.release()

    def samplingBtn_click_thread(self):
        print("[DEBUG] samplingBtn_click_thread 被调用")
        thread = threading.Thread(target=self.samplingBtn_click, daemon=True)
        thread.start()

    def samplingBtn_click_sync(self):
        # 同步版本的采样，不启动线程
        self.samplingBtn.setEnabled(False)
        self.samplingComplete = False

        self.list_lock.acquire()
        try:
            displayIndex = 0
            
            # 检查位置1是否存在，不存在就创建
            if len(self.eggGroupList) == 0:
                # 创建第一个框
                timestamp = int(time.time())
                statueItem = {
                    "id": timestamp,
                    'status': [0, 0, 0],
                    'rgb_image': [],
                    'kspace_image': [],
                }
                self.registerList.insert(0, statueItem)
                self.totalList.insert(0, statueItem)
                
                newEggItem = self.createEgg(0, [0, 0, 0])
                self.eggGroupList.append(newEggItem)
                self.horizontalLayout_4.addWidget(newEggItem['eggWidget'])
                newEggItem['grouptitle'].setText(_translate("MainWindow", "流水线位置编号：1"))
            
            # 如果registerList为空，初始化
            if len(self.registerList) == 0:
                timestamp = int(time.time())
                statueItem = {
                    "id": timestamp,
                    'status': [0, 0, 0],
                    'rgb_image': [],
                    'kspace_image': [],
                }
                self.registerList.insert(0, statueItem)
                self.totalList.insert(0, statueItem)

            self.updateEggStatus_single.emit(displayIndex, [-1, -1, -1])
            self.registerList[displayIndex]['kspace_image'] = ['', '', '']
            self.registerList[displayIndex]['rgb_image'] = ['', '', '']
            self.registerList[displayIndex]['rgb_image_original'] = ['', '', '']
            
            if len(self.totalList) > displayIndex:
                self.totalList[displayIndex]['rgb_image'] = ['', '', '']
                self.totalList[displayIndex]['rgb_image_original'] = ['', '', '']
                self.totalList[displayIndex]['kspace_image'] = ['', '', '']

            id = str(self.registerList[displayIndex]['id'])

            # 从Iface模块获取k空间数据
            print("[DEBUG] 调用 HCController.getResult() 获取数据")
            kspaceStr, shape = self.hc.getResult()
            print(f"[DEBUG] 获取数据成功，形状: {shape}")
            
            # 解析k空间数据
            kspaceArray = np.array(kspaceStr.split(','), dtype=np.complex128)
            print(f"[DEBUG] kspaceArray长度: {len(kspaceArray)}")
            
            # 根据返回的形状重建数据
            kspace = kspaceArray.reshape(shape)
            print(f"[DEBUG] kspace形状: {kspace.shape}")
            
            # 确保是二维数据（处理三维数据）
            if kspace.ndim == 3:
                # 如果是三维，取中间切片
                kspace = kspace[:, :, kspace.shape[2] // 2]
            elif kspace.ndim > 3:
                # 如果维度更高，取前两个维度
                kspace = kspace[:, :, 0, 0]
            print(f"[DEBUG] 处理后形状: {kspace.shape}")
            
            # 保存路径
            kspacePath = self.save_path + id + '/kspace_' + id + '.png'
            rgbPath = self.save_path + id + '/rgb._' + id + '.png'
            os.makedirs(self.save_path + id + '/', exist_ok=True)
            
            # 保存k空间图片
            self.message.saveImg(kspacePath, np.abs(kspace), cmap='hot')
            
            # 重建图像（IFFT）
            # k空间数据已经经过fftshift（原点在中心），所以先做ifftshift将原点移回角落
            kspace_ifftshift = np.fft.ifftshift(kspace)
            # 然后做IFFT
            im = np.fft.ifft2(kspace_ifftshift, (256, 256))
            im = np.squeeze(im)
            im_abs = np.abs(im)
            
            # 阈值降噪
            threshold = np.percentile(im_abs, 10)
            im_denoised = np.where(im_abs > threshold, im_abs, 0)
            
            # 保存重建图像（使用热力图）
            self.message.saveImg(rgbPath, im_denoised, cmap='hot')
            
            rgbList = [rgbPath, rgbPath, rgbPath]
            kspacList = [kspacePath, kspacePath, kspacePath]
            
            if displayIndex < len(self.eggGroupList):
                self.eggGroupList[displayIndex]['rgb_image'] = rgbList
                self.eggGroupList[displayIndex]['kspace_image'] = kspacList
            
            self.registerList[displayIndex]['kspace_image'] = kspacList
            self.registerList[displayIndex]['rgb_image'] = rgbList
            
            print(f"[DEBUG] 显示位置 displayIndex={displayIndex}")
            print(f"[DEBUG] eggGroupList长度={len(self.eggGroupList)}")
            print(f"[DEBUG] rgbList[0]存在={os.path.exists(rgbList[0])}")
            
            status2 = [1, 0, 0]
            self.updateEggStatus_single.emit(displayIndex, status2)
            self.registerList[displayIndex]['status'] = status2
            
            if len(self.totalList) > displayIndex:
                self.totalList[displayIndex]['status'] = status2

            self.displayIndex = 0

            item = {
                'id': id,
                'status': status2,
                'kspace': rgbPath,
                'rgb1': rgbPath,
                'rgb2': '',
                'rgb3': '',
                'rgb1_predict': '',
                'rgb2_predict': '',
                'rgb3_predict': '',
            }

            # 确保图片文件存在后再显示
            timeout = 0
            while not os.path.exists(rgbPath) and timeout < 100:
                time.sleep(0.01)
                timeout += 1
            
            # 在设置状态之后再设置图片，确保黑色背景生效
            self.updateEggImg_single.emit(displayIndex, rgbList, True)
            
            # 更新采样成功计数
            self.samplingSuccessCount += 1
            self.total_label.setText(_translate("MainWindow", "采样成功：" + str(self.samplingSuccessCount) + "次"))
            
            self.samplingComplete = True
            
            # 采样完成后启用下一组按钮
            self.nextGroup.setEnabled(True)

        except Exception as e:
            logger.exception(f"采样出现异常,{str(e)}")
            self.samplingComplete = True
        self.list_lock.release()
        self.samplingBtn.setEnabled(True)

    def samplingBtn_click(self):
        print("[DEBUG] samplingBtn_click 被调用")
        self.samplingBtn.setEnabled(False)
        self.samplingComplete = False

        self.list_lock.acquire()
        try:
            displayIndex = 0
            
            # 检查位置1是否存在，不存在就创建
            if len(self.eggGroupList) == 0:
                # 创建第一个框
                timestamp = int(time.time())
                statueItem = {
                    "id": timestamp,
                    'status': [0, 0, 0],
                    'rgb_image': [],
                    'kspace_image': [],
                }
                self.registerList.insert(0, statueItem)
                self.totalList.insert(0, statueItem)
                
                newEggItem = self.createEgg(0, [0, 0, 0])
                self.eggGroupList.append(newEggItem)
                self.horizontalLayout_4.addWidget(newEggItem['eggWidget'])
                newEggItem['grouptitle'].setText(_translate("MainWindow", "流水线位置编号：1"))
            
            # 如果registerList为空，初始化
            if len(self.registerList) == 0:
                timestamp = int(time.time())
                statueItem = {
                    "id": timestamp,
                    'status': [0, 0, 0],
                    'rgb_image': [],
                    'kspace_image': [],
                }
                self.registerList.insert(0, statueItem)
                self.totalList.insert(0, statueItem)

            self.updateEggStatus_single.emit(displayIndex, [-1, -1, -1])
            self.registerList[displayIndex]['kspace_image'] = ['', '', '']
            self.registerList[displayIndex]['rgb_image'] = ['', '', '']
            self.registerList[displayIndex]['rgb_image_original'] = ['', '', '']
            
            if len(self.totalList) > displayIndex:
                self.totalList[displayIndex]['rgb_image'] = ['', '', '']
                self.totalList[displayIndex]['rgb_image_original'] = ['', '', '']
                self.totalList[displayIndex]['kspace_image'] = ['', '', '']

            id = str(self.registerList[displayIndex]['id'])

            # 从Iface模块获取k空间数据
            print("[DEBUG] 调用 HCController.getResult() 获取数据")
            kspaceStr, shape = self.hc.getResult()
            print(f"[DEBUG] 获取数据成功，形状: {shape}")
            
            # 解析k空间数据
            kspaceArray = np.array(kspaceStr.split(','), dtype=np.complex128)
            print(f"[DEBUG] kspaceArray长度: {len(kspaceArray)}")
            
            # 根据返回的形状重建数据
            kspace = kspaceArray.reshape(shape)
            print(f"[DEBUG] kspace形状: {kspace.shape}")
            
            # 确保是二维数据（处理三维数据）
            if kspace.ndim == 3:
                # 如果是三维，取中间切片
                kspace = kspace[:, :, kspace.shape[2] // 2]
            elif kspace.ndim > 3:
                # 如果维度更高，取前两个维度
                kspace = kspace[:, :, 0, 0]
            print(f"[DEBUG] 处理后形状: {kspace.shape}")
            
            # 保存路径
            kspacePath = self.save_path + id + '/kspace_' + id + '.png'
            rgbPath = self.save_path + id + '/rgb._' + id + '.png'
            os.makedirs(self.save_path + id + '/', exist_ok=True)
            
            # 保存k空间图片
            self.message.saveImg(kspacePath, np.abs(kspace), cmap='hot')
            
            # 重建图像（IFFT）
            # k空间数据已经经过fftshift（原点在中心），所以先做ifftshift将原点移回角落
            kspace_ifftshift = np.fft.ifftshift(kspace)
            # 然后做IFFT
            im = np.fft.ifft2(kspace_ifftshift, (256, 256))
            im = np.squeeze(im)
            im_abs = np.abs(im)
            
            # 阈值降噪
            threshold = np.percentile(im_abs, 10)
            im_denoised = np.where(im_abs > threshold, im_abs, 0)
            
            # 保存重建图像（使用gray颜色映射，避免红色网格效果）
            self.message.saveImg(rgbPath, im_denoised, cmap='gray')
            
            rgbList = [rgbPath, rgbPath, rgbPath]
            kspacList = [kspacePath, kspacePath, kspacePath]
            
            if displayIndex < len(self.eggGroupList):
                self.eggGroupList[displayIndex]['rgb_image'] = rgbList
                self.eggGroupList[displayIndex]['kspace_image'] = kspacList
            
            self.registerList[displayIndex]['kspace_image'] = kspacList
            self.registerList[displayIndex]['rgb_image'] = rgbList
            
            print(f"[DEBUG] 显示位置 displayIndex={displayIndex}")
            print(f"[DEBUG] eggGroupList长度={len(self.eggGroupList)}")
            print(f"[DEBUG] rgbList[0]存在={os.path.exists(rgbList[0])}")
            
            status2 = [1, 0, 0]
            self.updateEggStatus_single.emit(displayIndex, status2)
            self.registerList[displayIndex]['status'] = status2
            
            if len(self.totalList) > displayIndex:
                self.totalList[displayIndex]['status'] = status2

            self.displayIndex = 0

            item = {
                'id': id,
                'status': status2,
                'kspace': rgbPath,
                'rgb1': rgbPath,
                'rgb2': '',
                'rgb3': '',
                'rgb1_predict': '',
                'rgb2_predict': '',
                'rgb3_predict': '',
            }

            # 在设置状态之后再设置图片，确保黑色背景生效
            self.updateEggImg_single.emit(displayIndex, rgbList, True)

            # 更新采样成功计数
            self.samplingSuccessCount += 1
            self.total_label.setText(_translate("MainWindow", "采样成功：" + str(self.samplingSuccessCount) + "次"))

            self.samplingComplete = True

            # 采样完成后启用下一组按钮
            self.nextGroup.setEnabled(True)

        except Exception as e:
            logger.exception(f"采样出现异常,{str(e)}")
            self.samplingComplete = True
        self.list_lock.release()
        self.samplingBtn.setEnabled(True)

    '''更新蛋显示拦--列表'''
    def updateEggStatusList(self, eggStatusList):
        for x in range(len(eggStatusList)):
            # 添加边界检查
            if x >= len(self.eggGroupList):
                continue
            if x >= len(self.registerList):
                continue
            
            imgList = self.registerList[x]['rgb_image']
            self.updateEggStatus(x, eggStatusList[x])
            self.updateEggImg(x, imgList)

    '''更新蛋显示拦--单个'''
    def updateEggStatus(self, x, eggStatus):
        # 添加边界检查
        if x < 0 or x >= len(self.eggGroupList):
            print(f"[WARNING updateEggStatus] index={x} 超出范围, eggGroupList长度={len(self.eggGroupList)}")
            return
        egg = self.eggGroupList[x]['eggItems']['egg']


    '''更新列表图片'''
    def updateEggImg(self, index, imgList, cut=False, retry_count=0):
        try:
            print(f"[DEBUG updateEggImg] index={index}, imgList={imgList}")
            # 添加边界检查
            if index < 0 or index >= len(self.eggGroupList):
                print(f"[WARNING updateEggImg] index={index} 超出范围, eggGroupList长度={len(self.eggGroupList)}")
                return
            egg = self.eggGroupList[index]['eggItems']['egg']
            # 设置黑色背景
            egg.setStyleSheet("background-color: #000000;")
            
            if imgList is None or len(imgList) == 0 or imgList[0] is None or imgList[0] == '':
                print(f"[DEBUG updateEggImg] imgList为空，设置空白图片")
                eggPhoto = QPixmap('')
                egg.setPixmap(eggPhoto)
            else:
                print(f"[DEBUG updateEggImg] 尝试加载图片: {imgList[0]}")
                eggPhoto = QPixmap(imgList[0])
                print(f"[DEBUG updateEggImg] eggPhoto.isNull()={eggPhoto.isNull()}, size={eggPhoto.width()}x{eggPhoto.height()}")
                
                # 旋转图片
                eggPhoto2 = eggPhoto.transformed(QTransform().rotate(-90))
                width = eggPhoto2.width()
                height = eggPhoto2.height()
                labelWidth = egg.width()
                labelHeight = egg.height()
                print(f"[DEBUG updateEggImg] 标签尺寸: {labelWidth}x{labelHeight}")
                
                if width == 0 or height == 0:
                    eggPhoto = QPixmap('')
                    egg.setPixmap(eggPhoto)
                    return

                if cut:
                    img = eggPhoto2.toImage()
                    width = img.width()
                    height = img.height()
                    sub_width = int(width * 0.6)
                    sub_height = height
                    sub_img = img.copy(int(width * 0.35), 0, sub_width, sub_height)
                    eggPhoto2 = QPixmap.fromImage(sub_img)
                    width = sub_width
                    height = sub_height

                # 设置标签自动扩展填满布局空间
                egg.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
                
                # 启用图片缩放填充
                egg.setScaledContents(True)
                
                # 强制刷新布局
                egg.parentWidget().updateGeometry()
                QtWidgets.QApplication.processEvents()
                egg.adjustSize()
                
                # 获取刷新后的尺寸
                labelWidth = egg.width()
                labelHeight = egg.height()
                print(f"[DEBUG updateEggImg] 刷新后标签尺寸: {labelWidth}x{labelHeight}")
                
                # 如果尺寸还是太小，延迟重试
                if labelWidth < 50 or labelHeight < 50:
                    if retry_count < 3:
                        print(f"[DEBUG updateEggImg] 标签尺寸太小，延迟重试 ({retry_count})")
                        QTimer.singleShot(200, lambda: self.updateEggImg(index, imgList, cut, retry_count + 1))
                        return
                    else:
                        print(f"[DEBUG updateEggImg] 重试多次后尺寸仍然太小，使用默认尺寸")
                
                # 确保最小尺寸
                targetWidth = max(labelWidth, 200)
                targetHeight = max(labelHeight, 150)
                print(f"[DEBUG updateEggImg] 目标尺寸: {targetWidth}x{targetHeight}")
                
                # 保持比例填满整个标签框（只在对侧有黑边）
                eggPhoto3 = eggPhoto2.scaled(targetWidth, targetHeight, Qt.KeepAspectRatioByExpanding, Qt.SmoothTransformation)
                egg.setPixmap(eggPhoto3)

        except Exception as e:
            logger.exception(f"更新检测批次出现异常,{str(e)}")


    '''动画执行'''
    def animationRunning(self):
        # 添加边界检查：至少需要2个框才能移动
        if len(self.eggGroupList) < 2:
            print(f"[WARNING animationRunning] eggGroupList长度={len(self.eggGroupList)}, 跳过动画")
            return
        
        print(f"[DEBUG animationRunning] 开始动画, eggGroupList长度={len(self.eggGroupList)}")
        
        # 强制刷新布局
        self.scrollAreaWidgetContents_2.updateGeometry()
        QtWidgets.QApplication.processEvents()
        
        pos1 = self.eggGroupList[0]['eggWidget'].pos()
        pos2 = self.eggGroupList[1]['eggWidget'].pos()
        deltaX = pos2.x() - pos1.x()
        print(f"[DEBUG animationRunning] pos1={pos1}, pos2={pos2}, deltaX={deltaX}")
        
        # 给最后一个动画添加完成信号
        last_item = self.eggGroupList[-1]
        last_item['animation'].finished.connect(self.onAnimationFinished)
        
        for x, item in enumerate(self.eggGroupList):
            pos = item['eggWidget'].pos()
            item['animation'].setStartValue(QPoint(pos.x(), pos.y()))
            item['animation'].setEndValue(QPoint(pos.x() + deltaX, pos.y()))  # 移动到右侧
            QTimer.singleShot(0, item['animation'].start) 
        pass
        
    '''动画完成后处理'''
    def onAnimationFinished(self):
        # 断开信号连接防止重复触发
        for item in self.eggGroupList:
            try:
                item['animation'].finished.disconnect()
            except:
                pass
        self.afterNextEvent_single.emit()

    '''根据状态列表创建蛋组件'''
    def createEggList(self, eggStatusList):
        self.eggGroupList = []
        for x in range(0, len(eggStatusList)):
            item = self.createEgg(x, eggStatusList[x]['status'])
            # 同步 registerList 中的图片信息
            if x < len(self.registerList):
                item['rgb_image'] = self.registerList[x].get('rgb_image', [])
                item['kspace_image'] = self.registerList[x].get('kspace_image', [])
            self.horizontalLayout_4.addWidget(item['eggWidget'])
            self.eggGroupList.append(item)

    def createEgg(self, x, eggStatus):
        eggWidget = QtWidgets.QWidget(self.scrollAreaWidgetContents_2)
        eggWidget.setObjectName("eggWidget_" + str(x))
        verticalLayout = QtWidgets.QVBoxLayout(eggWidget)
        verticalLayout.setObjectName("verticalLayout_" + str(x))
        grouptitle = QtWidgets.QLabel(self.scrollAreaWidgetContents_2)
        grouptitle.setMinimumSize(QtCore.QSize(500, 32))
        grouptitle.setMaximumSize(QtCore.QSize(16777215, 16777215))
        grouptitle.setFrameShape(QtWidgets.QFrame.Box)
        grouptitle.setAlignment(QtCore.Qt.AlignCenter)
        grouptitle.setObjectName("grouptitle_" + str(x))
        grouptitle.setText(_translate("MainWindow", "流水线位置编号：" + str(x + 1)))
        verticalLayout.addWidget(grouptitle)

        eggItems = self.createEggStatue(eggStatus, x)
        verticalLayout.addWidget(eggItems['eggitem'])

        anim = QPropertyAnimation(eggWidget, b"pos")
        anim.setDuration(3000)  # 动画时长 3 秒
        anim.setEasingCurve(QEasingCurve.Linear)  # 匀速运动
        anim.setLoopCount(1)  # 循环
        # self.animations.append(anim)
        item = {
            'eggWidget': eggWidget,
            'lineNum': x + 1,
            'eggItems': eggItems,
            'verticalLayout': verticalLayout,
            'grouptitle': grouptitle,
            'animation': anim,
            'rgb_image': [],
            'kspace_image': []
        }
        verticalLayout.setStretch(0, 0)
        verticalLayout.setStretch(1, 4)
        verticalLayout.setStretch(2, 4)
        verticalLayout.setStretch(3, 4)
        return item


    '''创建鸡蛋组件一整块'''
    def createEggStatue(self, eggStatus, x):
        eggItem = QtWidgets.QWidget(self.scrollAreaWidgetContents_2)
        eggItem.setObjectName("eggitem_" + str(x))
        eggItemQHBoxLayout = QtWidgets.QHBoxLayout(eggItem)
        eggItemQHBoxLayout.setObjectName("eggitem_QHBoxLayout_" + str(x))
        egg = QtWidgets.QLabel(self.scrollAreaWidgetContents_2)
        egg.setMinimumSize(QtCore.QSize(0, 84))
        egg.setFrameShape(QtWidgets.QFrame.Box)
        egg.setFrameShadow(QtWidgets.QFrame.Plain)
        egg.setLineWidth(2)
        egg.setMidLineWidth(2)
        egg.setText("")
        egg.setObjectName("egg_" + str(x))
        egg.setAlignment(QtCore.Qt.AlignCenter)
        eggItemQHBoxLayout.addWidget(egg)

        if -1 in eggStatus:
            egg.setStyleSheet("background-color: #707272")
        else:
            egg.setStyleSheet("background-color: none;")

        eggItemQHBoxLayout.setStretch(0, 2)
        eggItems = {
            'eggitem': eggItem,
            'egg': egg,
        }
        return eggItems

    '''创建鸡蛋组件——分个'''
    def createEggStatueList(self, eggStatus, x):
        eggItems = []
        for y in range(0, len(eggStatus)):
            eggItem = QtWidgets.QWidget(self.scrollAreaWidgetContents_2)
            eggItem.setObjectName("eggitem_" + str(x) + str(y))
            eggItemQHBoxLayout = QtWidgets.QHBoxLayout(eggItem)
            eggItemQHBoxLayout.setObjectName("eggitem_QHBoxLayout_" + str(x) + str(y))

            egg = QtWidgets.QLabel(eggItem)
            egg.resize(QtCore.QSize(120, 120))
            egg.setMaximumSize(QtCore.QSize(120, 120))
            egg.setFrameShape(QtWidgets.QFrame.Box)
            egg.setFrameShadow(QtWidgets.QFrame.Plain)
            egg.setLineWidth(2)
            egg.setMidLineWidth(2)
            egg.setText("")
            egg.setObjectName("egg_" + str(x) + str(y))
            egg.setAlignment(QtCore.Qt.AlignCenter)
            eggItemQHBoxLayout.addWidget(egg)

            eggItemQHBoxLayout.setStretch(0, 1)
            eggItems.append({
                'eggitem': eggItem,
                'egg': egg,
            })
        return eggItems
        pass

    def setupUi(self):
        self.setObjectName("MainWindow")
        self.resize(1075, 929)
        self.centralwidget = QtWidgets.QWidget(self)
        self.centralwidget.setEnabled(True)
        self.centralwidget.setObjectName("centralwidget")

        self.verticalLayout = QtWidgets.QVBoxLayout(self.centralwidget)
        self.verticalLayout.setObjectName("verticalLayout")
        self.horizontalLayout = QtWidgets.QHBoxLayout()
        self.horizontalLayout.setObjectName("horizontalLayout")
        self.HSLVerticalLayout = QtWidgets.QVBoxLayout()
        self.HSLVerticalLayout.setObjectName("HSLVerticalLayout")
        self.horizontalLayout_2 = QtWidgets.QHBoxLayout()
        self.horizontalLayout_2.setObjectName("horizontalLayout_2")
        self.HSLIpAddress = QtWidgets.QLabel(self.centralwidget)
        self.HSLIpAddress.setMaximumSize(QtCore.QSize(159, 16777215))
        self.HSLIpAddress.setObjectName("HSLIpAddress")
        self.horizontalLayout_2.addWidget(self.HSLIpAddress)
        self.HslIpbox = QtWidgets.QLineEdit(self.centralwidget)
        self.HslIpbox.setEnabled(True)
        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Expanding)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.HslIpbox.sizePolicy().hasHeightForWidth())
        self.HslIpbox.setSizePolicy(sizePolicy)
        self.HslIpbox.setMaximumSize(QtCore.QSize(120, 32))
        self.HslIpbox.setObjectName("HslIpbox")
        self.horizontalLayout_2.addWidget(self.HslIpbox)
        self.Hslportbox = QtWidgets.QSpinBox(self.centralwidget)
        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.MinimumExpanding, QtWidgets.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.Hslportbox.sizePolicy().hasHeightForWidth())
        self.Hslportbox.setSizePolicy(sizePolicy)
        self.Hslportbox.setMaximumSize(QtCore.QSize(150, 30))
        self.Hslportbox.setMaximum(115200)
        self.Hslportbox.setObjectName("Hslportbox")
        self.horizontalLayout_2.addWidget(self.Hslportbox)
        spacerLeft = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.horizontalLayout_2.addItem(spacerLeft)
        self.HslConnectBtn = QtWidgets.QPushButton(self.centralwidget)
        self.HslConnectBtn.setMaximumSize(QtCore.QSize(100, 24))
        self.HslConnectBtn.setObjectName("HslConnectBtn")
        self.horizontalLayout_2.addWidget(self.HslConnectBtn)
        spacerMid = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.horizontalLayout_2.addItem(spacerMid)
        self.HslDisconnectBtn = QtWidgets.QPushButton(self.centralwidget)
        self.HslDisconnectBtn.setMaximumSize(QtCore.QSize(100, 16777215))
        self.HslDisconnectBtn.setObjectName("HslDisconnectBtn")
        self.horizontalLayout_2.addWidget(self.HslDisconnectBtn)
        spacerRight = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.horizontalLayout_2.addItem(spacerRight)
        self.HSLVerticalLayout.addLayout(self.horizontalLayout_2)
        self.HSLLayout = QtWidgets.QHBoxLayout()
        self.HSLLayout.setSizeConstraint(QtWidgets.QLayout.SetFixedSize)
        self.HSLLayout.setObjectName("HSLLayout")
        spacerAutoLeft = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.HSLLayout.addItem(spacerAutoLeft)
        self.autoHSL = QtWidgets.QPushButton(self.centralwidget)
        self.autoHSL.setMaximumSize(QtCore.QSize(100, 16777215))
        self.autoHSL.setObjectName("autoHSL")
        self.HSLLayout.addWidget(self.autoHSL)
        spacer1 = QtWidgets.QSpacerItem(20, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.HSLLayout.addItem(spacer1)
        self.nextGroup = QtWidgets.QPushButton(self.centralwidget)
        self.nextGroup.setMaximumSize(QtCore.QSize(100, 16777215))
        self.nextGroup.setObjectName("nextGroup")
        self.HSLLayout.addWidget(self.nextGroup)
        spacer2 = QtWidgets.QSpacerItem(20, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.HSLLayout.addItem(spacer2)
        self.manualHSL = QtWidgets.QPushButton(self.centralwidget)
        self.manualHSL.setMaximumSize(QtCore.QSize(100, 16777215))
        self.manualHSL.setObjectName("manualHSL")
        self.HSLLayout.addWidget(self.manualHSL)
        spacerManualRight = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.HSLLayout.addItem(spacerManualRight)
        self.HSLVerticalLayout.addLayout(self.HSLLayout)

        self.samplingLayout = QtWidgets.QHBoxLayout()
        self.samplingLayout.setObjectName("samplingLayout")
        self.reset = QtWidgets.QPushButton(self.centralwidget)
        self.reset.setMaximumSize(QtCore.QSize(120, 40))
        self.reset.setObjectName("reset")
        self.samplingLayout.addWidget(self.reset)
        self.samplingBtn = QtWidgets.QPushButton(self.centralwidget)
        self.samplingBtn.setMaximumSize(QtCore.QSize(120, 40))
        self.samplingBtn.setObjectName("samplingBtn")
        self.samplingLayout.addWidget(self.samplingBtn)
        self.HSLVerticalLayout.addLayout(self.samplingLayout)

        self.horizontalLayout.addLayout(self.HSLVerticalLayout)

        self.line = QtWidgets.QFrame(self.centralwidget)
        self.line.setFrameShape(QtWidgets.QFrame.VLine)
        self.line.setFrameShadow(QtWidgets.QFrame.Sunken)
        self.line.setObjectName("line")
        self.horizontalLayout.addWidget(self.line)

        self.HCVerticalLayout = QtWidgets.QVBoxLayout()
        self.HCVerticalLayout.setObjectName("HCVerticalLayout")

        self.total_label = QtWidgets.QLabel(self.centralwidget)
        self.total_label.setFrameShape(QtWidgets.QFrame.Box)
        self.total_label.setAlignment(QtCore.Qt.AlignCenter)
        self.total_label.setMinimumSize(QtCore.QSize(200, 40))
        self.total_label.setObjectName("total_label")
        self.HCVerticalLayout.addWidget(self.total_label)

        self.horizontalLayout.addLayout(self.HCVerticalLayout)
        self.horizontalLayout.setStretch(0, 3)
        self.horizontalLayout.setStretch(2, 1)
        self.verticalLayout.addLayout(self.horizontalLayout)

        self.scrollArea = QtWidgets.QScrollArea(self.centralwidget)
        self.scrollArea.setMaximumSize(QtCore.QSize(16777215, 16777215))
        self.scrollArea.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOn)
        self.scrollArea.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOn)
        self.scrollArea.setWidgetResizable(True)
        self.scrollArea.setObjectName("scrollArea")
        self.scrollAreaWidgetContents_2 = QtWidgets.QWidget()
        self.scrollAreaWidgetContents_2.setGeometry(QtCore.QRect(0, 0, 838, 566))
        self.scrollAreaWidgetContents_2.setObjectName("scrollAreaWidgetContents_2")
        self.horizontalLayout_4 = QtWidgets.QHBoxLayout(self.scrollAreaWidgetContents_2)
        self.horizontalLayout_4.setSizeConstraint(QtWidgets.QLayout.SetMinAndMaxSize)
        self.horizontalLayout_4.setObjectName("horizontalLayout_4")

        self.scrollArea.setWidget(self.scrollAreaWidgetContents_2)
        self.verticalLayout.addWidget(self.scrollArea)
        self.verticalLayout.setStretch(0, 3)
        self.verticalLayout.setStretch(1, 6)
        self.setCentralWidget(self.centralwidget)
        self.statusbar = QtWidgets.QStatusBar(self)
        self.statusbar.setObjectName("statusbar")
        self.setStatusBar(self.statusbar)
        self.menubar = QtWidgets.QMenuBar(self)
        self.menubar.setGeometry(QtCore.QRect(0, 0, 875, 23))
        self.menubar.setObjectName("menubar")
        self.setMenuBar(self.menubar)

        self.retranslateUi(self)
        self.btn_Event()
        QtCore.QMetaObject.connectSlotsByName(self)

    def retranslateUi(self, MainWindow):

        self.setWindowTitle(_translate("MainWindow", "MainWindow"))

        self.HslIpbox.setText("COM3")
        self.Hslportbox.setValue(9600)
        self.HSLIpAddress.setText(_translate("MainWindow", "HSL_COM:"))
        self.HslConnectBtn.setText(_translate("MainWindow", "连接"))
        self.HslDisconnectBtn.setText(_translate("MainWindow", "断开"))
        self.reset.setText(_translate("MainWindow", "重置流水线"))
        self.autoHSL.setText(_translate("MainWindow", "自动模式"))
        self.manualHSL.setText(_translate("MainWindow", "手动模式"))
        self.nextGroup.setText(_translate("MainWindow", "下一组"))
        self.samplingBtn.setText(_translate("MainWindow", "采样"))
        self.total_label.setText(_translate("MainWindow", "采样成功：0次"))

    def show(self):
        super().show()


if __name__ == '__main__':
    app = QtWidgets.QApplication(sys.argv)
    windowsLoad = Ui_MainWindow()
    windowsLoad.show()
    sys.exit(app.exec_())
