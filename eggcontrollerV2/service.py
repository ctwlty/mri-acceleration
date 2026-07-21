import threading
import time
from flask import Flask
import matplotlib.pyplot as plt

from Iface.HCController import HCController as HCController


plt.figure()
plt.close()
hc = HCController()

app = Flask(__name__)

WAITING_URL = "/waiting"
START_URL = "/start"
RESULt_URL = "/result"

startspimling =  False
watting = False
result = {}


@app.route(RESULt_URL, methods=["GET"])
def get_Result():
    global result
    # print(result)
    if result == False:
        resp = {
            "code": "500",
            "message": "核磁请求错误",
            "data": {}
        }
    else:
        data = {'result': result}
        resp = {
            "code": "200",
            "message": "success",
            "data": data
        }
    return resp

@app.route(WAITING_URL, methods=["GET"])
def get_waiting():
    global watting
    # print("wait", watting)
    resp = {
        "code": "200",
        "message": "success",
        "data": watting
    }
    return resp


@app.route(START_URL, methods=["GET"])
def get_start():
    # hc.handleStart()
    global startspimling
    global watting
    global result
    startspimling = True 
    watting = True
    result = None
    resp = {
        "code": "200",
        "message": "HC start",
        "data": {}
    }
    return resp


def startAPP():
    ip = '0.0.0.0' # 本机ip地址
    port = 8085

    app.run(host=ip, port=port)


# 主程序
if __name__ == '__main__':
    # ip = '192.168.1.18' # 本机ip地址
    # port = 8085

    # app.run(host=ip, port=port)
    
    algorithmThread = threading.Thread(target=startAPP, daemon=True)  
    algorithmThread.start()

    while True:
        try:
            if startspimling:
                # hc.handleStart()
                startspimling = False
                start = time.time()
                while hc.getWaitStatus():
                    s = hc.getWaitStatus()
                    print("wattingsssss", s)
                    time.sleep(0.5)
                print("scanning done! 采样耗时:" + str(time.time() - start) + "s")
                watting = False
                result = hc.getResult()
                # print(result)
        except Exception as e:
            print("循环出现异常", str(e))