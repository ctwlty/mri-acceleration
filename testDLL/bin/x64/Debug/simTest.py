from ctypes import (
    CDLL, c_int, c_float, c_char_p, c_wchar_p, POINTER,
    c_double, c_bool, sizeof, byref, c_void_p, c_long, windll
)
from time import sleep
from os import chdir

class console(object):
    DLL_PATH = r"D:\123pan\Downloads\testDLL\bin\x64\Debug\mridll.dll"  
    INIT_INI_PATH = r"D:\123pan\Downloads\testDLL\bin\x64\Debug\hw_cfg\init.ini".encode('utf-8')
    PAR_PTH = r'D:\123pan\Downloads\testDLL\bin\x64\Debug\PTScan.par'.encode('utf-8')
    SAVE_PTH = r'D:\\mri_data'.encode('utf-8')
    PREFIX = r'tmp1'.encode()
    SAVE_PAR_PTH = r'D:\\mri_data\\new.par'.encode()

    def __init__(self) -> None:
        try:
            self.lib = CDLL(self.DLL_PATH)
        except Exception as e:
            raise FileNotFoundError(f"加载菲特DLL失败：{self.DLL_PATH}，错误：{str(e)}")
        
        self._init_dll()

    def _init_dll(self):
        self.lib.Init.argtypes = [c_char_p]
        self.lib.Init.restype = c_int
        self.lib.ConfigFile.argtypes = [c_char_p]
        self.lib.ConfigFile.restype = c_int
        self.lib.SetParameterFile.argtypes = [c_char_p, c_bool]
        self.lib.SetParameterFile.restype = c_int
        self.lib.Run.argtypes = []
        self.lib.Run.restype = c_int
        self.lib.ScanStatus.argtypes = []
        self.lib.ScanStatus.restype = c_int
        self.lib.Abort.argtypes = []
        self.lib.Abort.restype = None
        self.lib.SetOutputPath.argtypes = [c_char_p]
        self.lib.SetOutputPath.restype = c_int
        self.lib.CloseSys.argtypes = []
        self.lib.CloseSys.restype = None
        self.lib.ScanCompleted.argtypes = []
        self.lib.ScanCompleted.restype = c_int
        self.lib.GetTotalScanNo.argtypes = []
        self.lib.GetTotalScanNo.restype = c_int
        self.lib.GetCurrentScanNo.argtypes = []
        self.lib.GetCurrentScanNo.restype = c_int

        self.lib.SetAverageMode.argtypes = [c_int]
        self.lib.SetAverageMode.restype = None
        self.lib.SetSaveMode.argtypes = [c_int]
        self.lib.SetSaveMode.restype = None
        self.lib.SetChannelValid.argtypes = [c_char_p]
        self.lib.SetChannelValid.restype = c_int
        self.lib.SetParameter.argtypes = [c_char_p, c_double]
        self.lib.SetParameter.restype = c_int
        self.lib.SetTxCenterFre.argtypes = [c_int, c_int, c_int, c_double]
        self.lib.SetTxCenterFre.restype = c_int
        self.lib.SaveParameterFile.argtypes = [c_char_p]
        self.lib.SaveParameterFile.restype = c_int
        self.lib.SetChannelValue.argtypes = [c_int, c_float]
        self.lib.SetChannelValue.restype = None
        self.lib.SetOutputPrefix.argtypes = [c_char_p]
        self.lib.SetOutputPrefix.restype = c_int
        self.lib.SaveParameterFile.argtypes = [c_char_p]
        self.lib.SaveParameterFile.restype = c_int
        self.lib.C_PrepareRun.argtypes = [c_bool]
        self.lib.C_PrepareRun.restype = c_int

        self.lib.SetSystemSel.argtypes = [c_int]
        self.lib.SetSystemSel.restype = None

    def simpleRun(self):
        ret = self.lib.Init(self.INIT_INI_PATH)
        print(f'init {ret}')
        ret = self.lib.ConfigFile(self.INIT_INI_PATH)
        print(f'config {ret}')
        self.lib.SetAverageMode(0)
        self.lib.SetSaveMode(1)
        ret = self.lib.SetParameterFile(self.PAR_PTH, False)
        print(f'par {ret}')
        ret = self.lib.SetOutputPath(self.SAVE_PTH)
        print(f'output pth {ret}')
        ret = self.lib.SetOutputPrefix(self.PREFIX)
        print(f'prefix {ret}')
        ret = self.lib.SetChannelValid('255'.encode('utf-8'))
        print(f'channel valid {ret}')
        ret = self.lib.SetParameter('viewBlock'.encode('utf-8'), 1.0)
        ret |= self.lib.SetParameter('TR'.encode('utf-8'), 500.0)
        print(f'set para {ret}')
        for i in range(8):
            ret |= self.lib.SetTxCenterFre(0, 4, i, 50.0)
        print(f'txcenterfre {ret}')
        ret = self.lib.SaveParameterFile(self.SAVE_PAR_PTH)
        print(f'save para file {ret}')
        self.lib.SetChannelValue(0, 1000.0)
        self.lib.SetChannelValue(1, 1000.0)
        self.lib.SetChannelValue(2, 1000.0)
        self.lib.SetSystemSel(3)
        
        # ret = self.lib.C_PrepareRun(False)
        # print(f'prepare run {ret}')
        ret = self.lib.Run()
        print(f"run {ret}")
        while True:
            com = self.lib.ScanCompleted()
            if com == 0 or com == 3:
                break
            sleep(60)
            print(f'complete is {com}')
            print(f'total is {self.lib.GetTotalScanNo()}')
            print(f'current is {self.lib.GetCurrentScanNo()}')
        
    def __del__(self):
        """析构函数：安全关闭系统"""
        if hasattr(self, 'lib'):
            try:
                self.lib.Abort()
                self.lib.CloseSys()
                print("菲特谱仪连接已安全关闭")
            except Exception as e:
                print(f"关闭谱仪时出错：{str(e)}")

if __name__ == "__main__":
    chdir(r"D:\123pan\Downloads\testDLL\bin\x64\Debug")
    work_dir = r"D:\123pan\Downloads\testDLL\bin\x64\Debug"
    
    print("="*60)
    print("设置工作目录")
    print("="*60)
    
    # 使用Windows API SetCurrentDirectoryA（与C++代码一致）
    kernel32 = windll.kernel32
    result = kernel32.SetCurrentDirectoryA(work_dir.encode('utf-8'))
    
    if result == 0:
        print(f"❌ 使用Windows API设置工作目录失败，错误代码: {kernel32.GetLastError()}")
    else:
        print(f"✅ 使用Windows API设置工作目录成功")
    
    # # 可选：添加DLL目录到PATH环境变量
    # import os
    # current_path = os.environ.get('PATH', '')
    # if work_dir not in current_path:
    #     os.environ['PATH'] = work_dir + os.pathsep + current_path
    #     print(f"✅ 已将DLL目录添加到PATH")
    c = console()
    c.simpleRun()