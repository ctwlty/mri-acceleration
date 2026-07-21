from ctypes import (
    CDLL, c_int, c_float, c_char_p, POINTER,
    c_double, c_bool
)
import time
import numpy as np
import os
import glob

# 解析数据相关
OFFSET_NOSAMPLES = 0xFC00
OFFSET_NOVIEWS = 0xFC04
OFFSET_NOSLICES = 0xFC0C
OFFSET_DATATYPE = 0xFC12
OFFSET_NOECHOES = 0xFC98
OFFSET_NOEXPERIMENTS = 0xFC9C
DATA_START_OFFSET = 0x10108
OFFSET_NOVIEWS_SEC = 0xFC08

TIMEOUT = 1500
RAW_DATA_OFFSET = 65536 + 8 + 256


class console(object):
    DLL_PATH = r"Iface\mriRely\mridll.dll"
    INIT_INI_PATH = r"Iface\mriRely\hw_cfg\init.ini"
    OUTPUT_PATH = r"D:\mri_data\par0423-3"
    PAR_PTH = r'Iface\mriRely\par0423.par'
    is_connected = False
    current_par_file = None

    def __init__(self):
        try:
            self.lib = CDLL(self.DLL_PATH)
        except Exception as e:
            raise FileNotFoundError(f"loading dll failed：{self.DLL_PATH}, error：{str(e)}")

        print(r'dll load successfully')
        self._init_dll_prototypes()
        print(r'func are loaded')

        self._init_path_check()
        print(r"start connection")
        self.is_connected = self._connect()
        print(r'connection complete')

    def _init_dll_prototypes(self):
        self.lib.Init.argtypes = [c_char_p]
        self.lib.Init.restype = c_int

        self.lib.ConfigFile.argtypes = [c_char_p]
        self.lib.ConfigFile.restype = c_int
        self.lib.SetSystemSel.argtypes = [c_int]
        self.lib.SetSystemSel.restype = None

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
        self.lib.SetSaveMode.argtypes = [c_int]
        self.lib.SetSaveMode.restype = None

        self.lib.CloseSys.argtypes = []
        self.lib.CloseSys.restype = None

    def _init_path_check(self):
        if not os.path.exists(self.INIT_INI_PATH):
            raise FileNotFoundError(f"初始化文件不存在：{self.INIT_INI_PATH}")
        if not os.path.exists(self.OUTPUT_PATH):
            os.makedirs(self.OUTPUT_PATH, exist_ok=True)

    def _connect(self) -> bool:
        init_ret = self.lib.Init(self.INIT_INI_PATH.encode("utf-8"))
        if init_ret != 0:
            raise RuntimeError(f"谱仪初始化失败，错误码：{init_ret}")
        
        cfg_ret = self.lib.ConfigFile(self.INIT_INI_PATH.encode('utf-8'))
        if cfg_ret != 0:
            raise RuntimeError(f'config error, code is {cfg_ret}')

        save_path_ret = self.lib.SetOutputPath(self.OUTPUT_PATH.encode("utf-8"))
        if save_path_ret != 0:
            self.lib.CloseSys()
            raise RuntimeError(f"设置保存路径失败，错误码：{save_path_ret}")

        par_ret = self.lib.SetParameterFile(self.PAR_PTH.encode(), False)
        if par_ret != 0:
            self.lib.CloseSys()
            raise RuntimeError(f'par file setting failed, error {par_ret}')
        
        self.current_par_file = self.PAR_PTH
        self.lib.SetSystemSel(3)
        self.lib.SetSaveMode(1)

        return True

    def _parse_raw_core(self, raw_file_path, echo_idx=0, exp_idx=0):
        if not os.path.exists(raw_file_path):
            raise FileNotFoundError(f"文件不存在: {raw_file_path}")

        with open(raw_file_path, "rb") as f:
            raw_data = f.read()

        noSamples = np.frombuffer(raw_data, dtype='<u4', count=1, offset=OFFSET_NOSAMPLES)[0]
        noViews = np.frombuffer(raw_data, dtype='<u4', count=1, offset=OFFSET_NOVIEWS)[0]
        noSlices = np.frombuffer(raw_data, dtype='<u4', count=1, offset=OFFSET_NOSLICES)[0]
        dataType = np.frombuffer(raw_data, dtype='<u2', count=1, offset=OFFSET_DATATYPE)[0]
        noEchoes = np.frombuffer(raw_data, dtype='<u4', count=1, offset=OFFSET_NOECHOES)[0]
        noExperiments = np.frombuffer(raw_data, dtype='<u4', count=1, offset=OFFSET_NOEXPERIMENTS)[0]

        expected_total_points = noExperiments * noEchoes * noSlices * noViews * noSamples
        data_block = raw_data[DATA_START_OFFSET:]
        
        complex_data = None
        if dataType == 0x2:
            bytes_per_point = 8
            expected_bytes = expected_total_points * bytes_per_point
            valid_data = data_block[:expected_bytes]
            raw_int32 = np.frombuffer(valid_data, dtype='<i4')
            i_data = raw_int32[0::2]
            q_data = raw_int32[1::2]
            complex_data = i_data + 1j * q_data
        else:
            raise NotImplementedError("仅支持 DataType=0x2 数据")

        full_shape = (noExperiments, noEchoes, noSlices, noViews, noSamples)
        complex_data = complex_data.reshape(full_shape)
        kspace_3d = complex_data[exp_idx, echo_idx, :, :, :]

        print(f"[Info] 原始数据维度: (Slice={noSlices}, View={noViews}, Sample={noSamples})")

        if noSlices == 1 and noViews == 1:
            print("[Info] 检测到一维波谱数据 (FID)")
            return kspace_3d.flatten(), "SPEC"
        else:
            print("[Info] 检测到图像数据，正在重建...")
            image_3d = np.fft.fftshift(np.fft.fftn(np.fft.ifftshift(kspace_3d)))
            image_3d = np.abs(image_3d)
            
            p1, p99 = np.percentile(image_3d, (1, 99))
            image_3d = np.clip(image_3d, p1, p99)
            image_3d = (image_3d - np.min(image_3d)) / (np.max(image_3d) - np.min(image_3d))
            return image_3d, "IMAGE"

    def get_all_fid(self) -> np.ndarray:
        """获取FID数据"""
        print(f"[{time.ctime()}] 开始准备扫描...")
        
        run_ret = self.lib.Run()
        if run_ret != 0:
            raise RuntimeError(f"扫描启动失败，错误码：{run_ret}")
        print(r'try to wait for scan')
    
        print(f"[{time.ctime()}] 扫描已启动，开始等待完成...")
        start_time = time.time()
        while True:
            if time.time() - start_time > TIMEOUT:
                self.lib.Abort()
                raise TimeoutError(f"扫描超时（{TIMEOUT}秒），已终止扫描")
            
            status = self.lib.ScanStatus()
            print(f"[{time.ctime()}] 扫描状态码：{status}（3=完成/4=写数据中，1=扫描中/2=传数据中）")
            
            if status in (3, 0):
                break
            elif status in (-1, 5, 6):
                self.lib.Abort()
                raise RuntimeError(f"扫描异常，状态码：{status}")
            
            time.sleep(5)
    
        raw_files = glob.glob(os.path.join(self.OUTPUT_PATH, "*.raw"))
        if not raw_files:
            raise FileNotFoundError(f"在保存目录 {self.OUTPUT_PATH} 未找到.raw数据文件")
        
        latest_raw = max(raw_files, key=os.path.getmtime)
        file_size = os.path.getsize(latest_raw)
        print(f"[{time.ctime()}] 找到最新RAW文件：{latest_raw}，大小：{file_size}字节")
        
        if file_size < RAW_DATA_OFFSET:
            raise ValueError(f"RAW文件损坏，实际大小{file_size}字节，小于最小文件头{RAW_DATA_OFFSET}字节")
    
        return self._parse_raw_core(latest_raw)[0]

    def is_scanning(self) -> bool:
        """判断是否扫描中"""
        status = self.lib.ScanStatus()
        return status in (1, 2)

    def __del__(self):
        """析构函数：安全关闭系统"""
        if hasattr(self, 'lib') and self.is_connected:
            try:
                self.lib.Abort()
                self.lib.CloseSys()
                print("菲特谱仪连接已安全关闭")
            except Exception as e:
                print(f"关闭谱仪时出错：{str(e)}")