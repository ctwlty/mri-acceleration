
import os
import time
import requests
import numpy as np
import queue
import matplotlib
matplotlib.use('Agg')
from logger.robot_logger import logger
from request.request import BaseRequestAPI
import matplotlib.pyplot as plt
import cv2


class Message():
    sendImageQueue = queue.Queue(maxsize=13)
    reciveQueue = queue.Queue(maxsize=13)
    saveQueue = queue.Queue(maxsize=13)
    batchId = "202409273231"
    save_path = './dataset/imgs/'
    baseUrl = "https://mainlab.tiyan.pj.hdzn.cn/sci/tiyan"
    upload_url = baseUrl + "/file/uploadFile"
    max_files = 30  # 最大保存文件数

    def __init__(self, save_path='./dataset/imgs/', baseUrl="https://mainlab.tiyan.pj.hdzn.cn/sci/tiyan"):
        self.req2 = BaseRequestAPI(self.baseUrl)
        self.save_path = save_path
        self.baseUrl = baseUrl
        self.upload_url = self.baseUrl + "/file/uploadFile"
        os.makedirs(self.save_path, exist_ok=True)

    def setBatchId(self, batchId):
        self.batchId = batchId

    def getBatchId(self):
        return self.batchId

    def setSendImageQueue(self, item):
        self.sendImageQueue.put(item)

    def getSendImageQueue(self):
        return self.sendImageQueue.get()

    def setReciveQueue(self, item):
        self.reciveQueue.put(item)

    def getReciveQueue(self):
        return self.reciveQueue.get()

    def setSaveQueue(self, item):
        self.saveQueue.put(item)

    def getSaveQueue(self):
        return self.saveQueue.get()

    def start_running(self):
        try:
            while True:
                if not self.sendImageQueue.empty():
                    item = self.sendImageQueue.queue[0]
                    print(item)
                    id = item['id']
                    status = item['status']
                    self.reciveQueue.put(item)
                    print("self.reciveQueue", self.reciveQueue.empty())
                    print("跳过服务器上传")
                    item = self.sendImageQueue.get()

                time.sleep(0.5)
        except Exception as e:
            print("消息队列线程出现异常" + str(e))
            logger.exception(f"消息队列线程出现异常,{str(e)}")

    def saveImg(self, path, img, cmap='hot'):
        self._cleanup_old_files()
        
        img_normalized = (img - np.min(img)) / (np.max(img) - np.min(img) + 1e-10)
        fig = plt.figure(figsize=(img.shape[1] / 100, img.shape[0] / 100), dpi=130)
        plt.imshow(img_normalized, cmap=cmap)
        plt.axis('off')
        plt.savefig(path, bbox_inches='tight', pad_inches=0, transparent=True)
        plt.close(fig)

    def _cleanup_old_files(self):
        try:
            if not os.path.exists(self.save_path):
                return
            
            files = []
            for f in os.listdir(self.save_path):
                f_path = os.path.join(self.save_path, f)
                if os.path.isfile(f_path):
                    files.append((os.path.getctime(f_path), f_path))
            
            if len(files) >= self.max_files:
                files.sort(key=lambda x: x[0])
                num_to_delete = len(files) - self.max_files + 1
                for _, f_path in files[:num_to_delete]:
                    os.remove(f_path)
                    logger.info(f"FIFO清理: 删除旧文件 {f_path}")
        except Exception as e:
            logger.exception(f"FIFO清理失败: {str(e)}")

    @staticmethod
    def kspace2image(kspace, norm=None):
        return np.fft.fftshift(np.fft.ifft2(np.fft.fftshift(kspace), norm=norm))

    def segment_and_draw_best_ellipse(self, img, min_contour_length=100, max_contour_length=300):
        img_crops = [img[85:170, 40:110], img[95:180, 110:175], img[85:170, 165:230]]
        transparent_images = []
        status = [1, 1, 1]

        for i, cropped_img in enumerate(img_crops):
            processed_img = np.abs(cropped_img)
            processed_img = cv2.normalize(processed_img, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
            denoised_img = cv2.bilateralFilter(processed_img, d=15, sigmaColor=150, sigmaSpace=150)

            thresh = cv2.adaptiveThreshold(denoised_img, 255, cv2.ADAPTIVE_THRESH_MEAN_C, cv2.THRESH_BINARY, blockSize=11, C=2)

            kernel = np.ones((2, 2), np.uint8)
            eroded = cv2.dilate(thresh, kernel, iterations=1)

            contours, _ = cv2.findContours(eroded, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

            h, w = processed_img.shape
            transparent_img = np.zeros((h, w, 4), dtype=np.uint8)

            if contours:
                max_contour = max(contours, key=cv2.contourArea)

                if cv2.arcLength(max_contour, True) >= min_contour_length and len(max_contour) >= 5:
                    ellipse = cv2.fitEllipse(max_contour)
                    ellipse_perimeter = 2 * np.pi * np.sqrt((ellipse[1][0] / 2) * (ellipse[1][1] / 2))

                    if ellipse_perimeter > max_contour_length:
                        mask = np.zeros((h, w), dtype=np.uint8)
                        cv2.ellipse(mask, ellipse, 255, -1)

                        for j in range(3):
                            transparent_img[:, :, j] = processed_img * (mask // 255)
                        transparent_img[:, :, 3] = mask

            if transparent_img[:, :, 3].sum() == 0:
                transparent_img[:, :, 3] = 0
                status[i] = 0

            transparent_images.append(transparent_img)

        return transparent_images, status

    def fenge(self, img):
        return [img[95:180, 45:115], img[95:180, 110:175], img[95:180, 175:240]]

    def upload_image(self, prefix, id):
        if not os.path.exists(self.save_path + prefix):
            return ''

        f = open(self.save_path + prefix, 'rb')

        files = {
            'files': (prefix, f.read(), 'image/png')
        }
        data = {'source': 0}
        response = requests.post(self.upload_url, files=files, data=data)
        f.close()

        if response.status_code == 200:
            logger.info(f"upload success! url: {response.json()['data'][0]['storeName']}")
            return response.json()['data'][0]['storeName']
        else:
            logger.info(f"upload error! code: {response.text}")
            return "upload error!"