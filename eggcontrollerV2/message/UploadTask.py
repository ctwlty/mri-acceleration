'''
Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
Date: 2024-06-28 11:58:03
LastEditors: 梁航 496213401@qq.com
LastEditTime: 2024-09-24 17:40:48
FilePath: \FMRI-EBFD-main\src\ImageReconstructTask.py
Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
'''
from multiprocessing import Process
import json
import requests
import numpy as np
import time
from FMRI.src.utils_TRPA_engine import *
from logger.robot_logger import logger
import io
# 图像重建线程
class DetectTask(Process):
    def __init__(self, data):
        super().__init__()
        self.cfg = data['cfg']
        self.upload_queue = data['upload_queue']

    def upload_image(self, prefix, id, img):
        img_bytes = io.BytesIO()
        # Numpy 转 字节流
        np.save(img_bytes, img)
        img_bytes.seek(0)

        # PIL.Image 转 字节流
        # img.save(img_bytes, format='png')
        # img_bytes.seek(0)
        
        files = {
            # 'files': img_bytes
            'files': (prefix+str(id), img_bytes, 'image/png')  # 添加文件名和MIME类型
        }
        data = {
            'source': 0
        }
        response = requests.post("https://mainlab.tiyan.pj.hdzn.cn/sci/tiyan/file/uploadFile", files=files, data=data)
        if response.status_code == 200:
            logger.info(f"upload success! url: {response.json()['data'][0]['storeName']}")
            # print(response.json())
            # print(response.status_code)
            return response.json()['data'][0]['storeName']
        else:
            logger.info(f"upload error! code: {response.text}")
            return "upload error!"
    
    def run(self):
        # log

        while True:
            # 从队列中获取图片数据
            if self.upload_queue.qsize() > 0:
                (rgb, kspace) = self.upload_queue.get()
    
                t1 = time.time()

                # 将rec_im 上传到OSS得到图片路径
                rgb_url = self.upload_image("rgb-", id, rgb)
                kspace_url = self.upload_image("kspace-", id, kspace)

                logger.info('upload TIME: %.3fs' % (time.time()-t1))
                logger.info('---------------------------------')
                