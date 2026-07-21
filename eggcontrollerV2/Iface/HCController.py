import numpy as np

# from Iface.consolev3 import console
from Iface.console_mock import console


class HCController():
    status = 0
    HCStatus = 0
    HCConnect = False
    ipAddress = "192.168.0.1"
    port = 102

    def __init__(self):
        print('start1')
        self.c = console()
        print('start')

    def getWaitStatus(self):
        return self.c.is_scanning()

    def getResult(self):
        fid = self.c.get_all_fid()

        # 获取数据实际形状
        shape = fid.shape
        
        ksp = fid
        kspArray = ksp.flatten()
        kspStrArray = kspArray.astype(str)
        kspStr = ','.join(kspStrArray)

        # 返回数据和形状信息
        return kspStr, shape


