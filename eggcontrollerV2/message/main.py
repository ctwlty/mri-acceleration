'''
Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
Date: 2024-06-28 11:15:17
LastEditors: 梁航 496213401@qq.com
LastEditTime: 2024-09-24 17:24:36
FilePath: \FMRI-EBFD-main\main.py
Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
'''
from multiprocessing import Process, Queue, Manager
import threading
import time

import numpy as np
from src.DetectTask import DetectTask
import yaml
from logs.Logger import _get_logger
from flask import Flask, request, json
from PIL import Image
import io
import matplotlib.pyplot as plt
from FMRI.src.data import numpy2tensor, tensor2image, tensor2complex, tensor_split, image2kspace, kspace2image

app = Flask(__name__)

UPLOAD_URL = "/upload"
LENGTH_URL = "/length"
RESULT_URL = "/result"


# 传输fid数据包
@app.route(UPLOAD_URL, methods=["POST"])
def upload_kspace():
    if request.values.get("fidjson"):

        # 本地数据模拟
        # with open(cfg['data'], 'rb') as file:
        #     kspace_sampled = np.fromfile(file, dtype=np.complex64)

        # 接入核磁设备数据
        kspaceStr = request.values['fidjson']
        kspace_sampled = np.asarray(kspaceStr.replace('[', '').replace(']', '').replace(' ', ',').split(','), dtype=np.complex64)

        kspace_sampled = kspace_sampled.reshape((256,256)).astype(np.complex128)

        image_zeroFilled = kspace2image(kspace_sampled)
        image_zeroFilled = image_zeroFilled/np.max(np.abs(image_zeroFilled))
        kspace_sampled = image2kspace(image_zeroFilled)

        image_data = (request.values['id'], image_zeroFilled, kspace_sampled)
        image_queue.put(image_data)

        resp = {
            "code":"1000",
            "message":"success",
            "data":{}
        }
    else:
        resp = {
            "code":"501",
            "message":"error",
            "data":{}
        }
    return resp

@app.route(LENGTH_URL, methods=["GET"])
def get_result_queue_length():
    resp = {
        "code": "1000",
        "message": "success",
        "data": {
            "length": result_queue.qsize()
        }
    }
    return resp

@app.route(RESULT_URL, methods=["GET"])
def get_result():
    if result_queue.qsize() > 0:
        # 从队列中取出一个值 
        (id, kspace, rgb, status) = result_queue.get()

        kspaceArray = kspace.flatten()
        kspaceStrArray = kspaceArray.astype(str)
        kspaceStr = ','.join(kspaceStrArray)

        rgbArray = rgb.flatten()
        rgbStrArray = rgbArray.astype(str)
        rgbStr = ','.join(rgbStrArray)
        
        resp = {
            "code": "1000",
            "message": "success",
            "data": {
                "id": id,
                "kspace_image": kspaceStr,
                "rgb_image": rgbStr,
                "status": status
            }
        }
    else:
        resp = {
            "code": "501",
            "message": "queue is empty",
            "data": {}
        }
    return resp

# 主程序
if __name__ == '__main__':
    with open('./config/config.yaml', 'r') as f:
        cfg = yaml.safe_load(f)

    image_queue = Manager().Queue()
    result_queue = Manager().Queue()
    # upload_queue = Manager().Queue()

    # log
    logger = _get_logger(cfg['log_path'], 'info')
    logger.info('---------------work start------------------')

    manager = Manager()
    data = manager.dict()
    data['cfg'] = cfg
    data['image_queue'] = image_queue
    data['result_queue'] = result_queue
    # data['upload_queue'] = upload_queue
    
    image_detect_process = DetectTask(data)
    # upload_process = UploadTask(data)
    
    image_detect_process.start()
    # upload_process.start()

    print(cfg['ip'])
    app.run(host=cfg['ip'], port=cfg['port'])

    # 等待所有进程结束
    image_detect_process.join()
    # upload_process.join()