from ctypes import (
    CDLL, c_int, c_float, c_char_p, POINTER,
    c_double, c_bool
)
import time
import numpy as np
import matplotlib.pyplot as plt
import os
import glob
import sys

# -------------------------- 原有枚举类型定义（保留，确保接口兼容） --------------------------
class PulseID(int):
    P0 = 0
    P1 = 1
    P2 = 2

class DelayID(int):
    D0 = 0
    D1 = 1
    D2 = 2

class RFAID(int):
    RFA0 = 0
    RFA1 = 1

class FreqID(int):
    FQ0 = 0
    FQ1 = 1

class RFShapeID(int):
    RFSH0 = 0
    RFSH1 = 1

class ConstID(int):
    C0 = 0
    C1 = 1

class GradAmpID(int):
    GA0 = 0
    GA1 = 1

class GradShapeID(int):
    GSH0 = 0
    GSH1 = 1

class VDLID(int):
    VDL0 = 0
    VDL1 = 1

class RFALID(int):
    RFAL0 = 0
    RFAL1 = 1

class FQLID(int):
    FQL0 = 0
    FQL1 = 1

class PHLID(int):
    PHL0 = 0
    PHL1 = 1

class PCLID(int):
    PCL0 = 0
    PCL1 = 1

class GLID(int):
    GL0 = 0
    GL1 = 1

class DBLID(int):
    DBL_D2 = 0
    DBL_D3 = 1

# -------------------------- 原有HC ID定义（保留，内部映射到菲特参数） --------------------------
# 维度相关ID
IDPI_DIM1 = 321
IDPI_DIM2 = 322
IDPI_DIM3 = 323
IDPI_DIM4 = 324

# 射频相关ID
IDPF_SFO1 = 101
IDPF_SW = 102
IDPF_DT2 = 103
IDPI_TD = 331
IDPI_RG1 = 341
IDPI_RG2 = 342

# 梯度矩阵相关ID
IDPF_GSLICEX = 301
IDPF_GSLICEY = 302
IDPF_GSLICEZ = 303

# 梯度偏移相关ID
IDPI_GXOFFSET = 358
IDPI_GYOFFSET = 359
IDPI_GZOFFSET = 360

# 预加重相关ID
IDPF_PREXA1 = 401
IDPF_PREXK1 = 402
IDPF_PREYA1 = 411
IDPF_PREYK1 = 412
IDPF_PREZA1 = 421
IDPF_PREZK1 = 422

# 重复次数相关ID
IDPI_DS = 371
IDPI_RP1CNT = 381
IDPI_RP2CNT = 382
IDPI_RP3CNT = 383
IDPI_RP4CNT = 384

# 脉冲/延迟/射频幅度相关ID
IDPF_P0 = 501
IDPF_D0 = 511
IDPF_RFA0 = 521
IDPF_FQ0 = 531
IDPS_RFSH0 = 601
IDPI_C0 = 611
IDPF_GA0 = 621
IDPF_GSH0 = 631
IDPF_VDL0 = 641
IDPF_RFAL0 = 651
IDPF_FQL0 = 661
IDPF_PHL0 = 671
IDPF_PCL0 = 681
IDPF_GL0 = 691
IDPI_DBL_D2 = 701
IDPF_GML = 711

# -------------------------- 菲特手册枚举定义（严格对齐手册3.1节） --------------------------
# 预加重通道（手册3.1节 preempChannel）
PREEMP_CHANNEL = {
    "XX": 0, "YY": 1, "ZZ": 2, "XYZB0": 3,
    "YX": 4, "XY": 5, "XZ": 6, "ZX": 7,
    "ZY": 8, "YZ": 9, "LB0": 10
}
# 预加重键值（手册3.1节 PreempKeys）
PREEMP_KEYS = {
    "A1": 0, "A2": 1, "A3": 2, "A4": 3, "A5": 4, "A6": 5,
    "T1": 6, "T2": 7, "T3": 8, "T4": 9, "T5": 10, "T6": 11
}
# 机箱类型（手册3.1节 boxType）
BOX_TYPE = {"M": 0, "E1": 1, "E2": 2, "E3": 3}
# 板卡类型（手册3.1节 boardType）
BOARD_TYPE = {"TX1": 4, "TX2": 5, "RX1": 6, "RX2": 7, "RX3": 8, "RX4": 9}
# 梯度通道（手册3.1节 shimChannel）
SHIM_CHANNEL = {"X": 0, "Y": 1, "Z": 2, "B0": 3}
# RAW文件固定偏移（手册第四章）
RAW_DATA_OFFSET = 65536 + 8 + 256  # 0x10108 采集数据起始位置

TMP_PTH = r'C:\MRIScanner\Seqlibrary\LWSEQ\fid.par'
TMP_PTH = r'C:\Program Files\SpectrometerIDE\seq_calibration\slim_scope\1pulse.par'
TMP_PTH = r'C:\MRIScanner - 0.17T\Scan\1pulse.par'
TMP_PTH = r'C:\MRIScanner - 0.17T\Scan\PTScan.par'

TMP_PTH = r'C:\Program Files\SpectrometerIDE\seq_calibration\orchestra\pre_adj_demo_ft_X.par'
TMP_PTH = r'C:\Program Files\SpectrometerIDE\seq_calibration\slim_hr\pre_adj_demo_ft_X.par'
TMP_PTH = r'C:\Program Files\SpectrometerIDE\seq_calibration\slim_scope\pre_adj_demo_ft_X.par'
TMP_PTH = r'C:\Program Files\SpectrometerIDE\seq_calibration\orchestra_hr\1pulse.par'
TMP_PTH = r'C:\MRIScanner\FTSpect\seq_lib\demo_src\CSJ_SE_PRE.par'
TMP_PTH = r'C:\MRIScanner\Scan\PTScan.par'
TMP_PTH = r'D:\123pan\Downloads\testDLL\bin\x64\Debug\PTScan.par'


#TMP_PTH = r'C:\Program Files\SpectrometerIDE\dll\tmp.par'
# -------------------------- 核心console类（修正后，严格对齐手册+配置文件） --------------------------
class console(object):
    # -------------------------- 配置参数（已匹配实际配置文件） --------------------------
    # DLL_PATH = r"C:\Program Files\SpectrometerIDE\dll\mridll.dll"  # 菲特DLL路径
    # DLL_PATH = r'C:\MRIScanner\FTSpect\ComSrvHost\mridll.dll'
    # DLL_PTH = r'C:\Program Files\SpectrometerIDE\NMRDLL.dll'
    # INIT_INI_PATH = r"C:\Program Files\SpectrometerIDE\dll\hw_cfg\init.ini"  # 初始化文件（匹配提供的init.ini）
    # #INIT_INI_PATH = r'C:\MRIScanner\FTSpect\hw_cfg\init.ini'
    # HW_CFG_PATH = r"C:\Program Files\SpectrometerIDE\dll\hw_cfg\box_m_info.hw"  # 硬件配置文件（init.ini指定的box_m_info.hw）
    # SEQ_DIR = r"C:\Program Files\SpectrometerIDE\seq_calibration\slim_scope"  # 序列根目录（需包含seq_calibration文件夹）
    # OUTPUT_PATH = r"D:\mri_data"  # 数据保存目录
    # PAR_FILE_SUFFIX = ".par"  # 匹配实际提供的par文件后缀

    # -------------------------- 配置参数（已匹配实际配置文件） --------------------------
    DLL_PATH = r"refers\NMRDLL.dll"  # 菲特DLL路径
    DLL_PATH = r"D:\123pan\Downloads\testDLL\bin\x64\Debug\mridll.dll"  # 菲特DLL路径
    INIT_INI_PATH = r"D:\123pan\Downloads\testDLL\bin\x64\Debug\hw_cfg\init.ini"  # 初始化文件（匹配提供的init.ini）
    HW_CFG_PATH = r"D:\123pan\Downloads\testDLL\bin\x64\Debug\hw_cfg\box_m_info.hw"  # 硬件配置文件（init.ini指定的box_m_info.hw）
    SEQ_DIR = r"D:\sequences"  # 序列根目录（需包含seq_calibration文件夹）
    OUTPUT_PATH = r"D:\mri_data"  # 数据保存目录
    PAR_FILE_SUFFIX = ".par"  # 匹配实际提供的par文件后缀
    PAE_PTH = r'PTScan.par'
    is_connected = False

    def __init__(self):
        # 1. 加载菲特DLL
        try:
            self.lib = CDLL(self.DLL_PATH)
        except Exception as e:
            raise FileNotFoundError(f"loading dll failed：{self.DLL_PATH}, error：{str(e)}")
        
        print(r'dll load successfully')
        
        # 2. 初始化DLL函数原型（仅保留手册明确的函数，删除所有虚构函数）
        self._init_dll_prototypes()
        print(r'func are loaded')
        
        # 3. 初始化路径合法性校验
        self._init_path_check()

        # 4. 连接设备+硬件配置（严格遵循手册流程：Init→ConfigFile）
        self.is_connected = self._connect()

        # 5. 初始化参数映射（仅保留par文件/手册中真实存在的参数）
        self._init_id_mapping()

        # 6. 初始化序列参数文件标记
        self.current_par_file = None

    def _init_dll_prototypes(self):
        """初始化DLL函数原型（严格对照手册2.x节，删除所有虚构函数）"""
        # -------------------------- 2.1 谱仪连接（手册P9） --------------------------
        self.lib.Init.argtypes = [c_char_p]
        self.lib.Init.restype = c_int

        # -------------------------- 2.2 硬件配置（手册P10） --------------------------
        self.lib.ConfigFile.argtypes = [c_char_p]
        self.lib.ConfigFile.restype = c_int
        self.lib.GetRxAttInfo.argtypes = []
        self.lib.GetRxAttInfo.restype = c_char_p  # 手册明确的接收衰减查询接口

        # -------------------------- 2.3 梯度/匀场参数（手册P23） --------------------------
        self.lib.SetChannelValue.argtypes = [c_int, c_float]
        self.lib.SetChannelValue.restype = None  # 梯度偏移/匀场值设置
        self.lib.GetChannelValue.argtypes = [c_int]
        self.lib.GetChannelValue.restype = c_float
        self.lib.SetPreempValue.argtypes = [c_int, c_int, c_float]
        self.lib.SetPreempValue.restype = c_int
        self.lib.SetGradDelay.argtypes = [c_float]
        self.lib.SetGradDelay.restype = c_int
        self.lib.GetGradDelay.argtypes = []
        self.lib.GetGradDelay.restype = c_float

        # -------------------------- 2.4 序列相关（手册P29） --------------------------
        self.lib.SetParameterFile.argtypes = [c_char_p, c_bool]
        self.lib.SetParameterFile.restype = c_int
        self.lib.SetParameter.argtypes = [c_char_p, c_double]
        self.lib.SetParameter.restype = c_int
        self.lib.GetParameter.argtypes = [c_char_p]
        self.lib.GetParameter.restype = c_double
        self.lib.SetParameterArray.argtypes = [c_char_p, POINTER(c_float), c_int]
        self.lib.SetParameterArray.restype = c_int
        self.lib.SaveParameterFile.argtypes = [c_char_p]
        self.lib.SaveParameterFile.restype = c_int

        # -------------------------- 2.5 扫描控制（手册P33） --------------------------
        self.lib.Run.argtypes = []
        self.lib.Run.restype = c_int
        self.lib.ScanStatus.argtypes = []
        self.lib.ScanStatus.restype = c_int
        self.lib.Abort.argtypes = []
        self.lib.Abort.restype = None
        # only C_PrepareRun in the dll
        self.lib.C_PrepareRun.argtypes = [c_bool]
        self.lib.C_PrepareRun.restype = c_int
        self.lib.ScanCompleted.argtypes = []
        self.lib.ScanCompleted.restype = c_int
        self.lib.GetTotalScanNo.argtypes = []
        self.lib.GetTotalScanNo.restype = c_int
        self.lib.GetCurrentScanNo.argtypes = []
        self.lib.GetCurrentScanNo.restype = c_int

        # -------------------------- 2.9 数据保存（手册P44） --------------------------
        self.lib.SetOutputPath.argtypes = [c_char_p]
        self.lib.SetOutputPath.restype = c_int
        self.lib.SetSaveMode.argtypes = [c_int]
        self.lib.SetSaveMode.restype = None
        self.lib.SingleSample.argtypes = [c_int, c_int, c_int, c_int]
        self.lib.SingleSample.restype = c_int
        self.lib.GetOutputFile.argtypes = []
        self.lib.GetOutputFile.restype = c_char_p

        # -------------------------- 2.10 发射配置（手册P49） --------------------------
        self.lib.SetTxCenterFre.argtypes = [c_int, c_int, c_int, c_double]
        self.lib.SetTxCenterFre.restype = c_int
        self.lib.SetTxATT.argtypes = [c_int, c_int, c_int, c_float]
        self.lib.SetTxATT.restype = c_int
        self.lib.SetRFWaves.argtypes = [c_char_p]
        self.lib.SetRFWaves.restype = c_int

        # -------------------------- 2.11 梯度配置（手册P54） --------------------------
        self.lib.SetAllMaxtrixValue.argtypes = [POINTER(c_float), c_int]
        self.lib.SetAllMaxtrixValue.restype = c_int

        # -------------------------- 2.12 接收配置（手册P59） --------------------------
        self.lib.SetRxATT.argtypes = [c_int, c_int, c_int, c_float, c_float, c_float, c_float, c_int]
        self.lib.SetRxATT.restype = c_int
        self.lib.SetRxCenterFre.argtypes = [c_int, c_int, c_int, c_float, c_bool]
        self.lib.SetRxCenterFre.restype = c_int

        # -------------------------- 2.15 版本/路径信息（手册P63） --------------------------
        self.lib.GetDLLPath.argtypes = []
        self.lib.GetDLLPath.restype = c_char_p

        # -------------------------- 2.6 关闭系统（手册P40） --------------------------
        self.lib.CloseSys.argtypes = []
        self.lib.CloseSys.restype = None

    def _init_path_check(self):
        """路径合法性校验，避免初始化失败"""
        # 校验配置文件是否存在
        if not os.path.exists(self.INIT_INI_PATH):
            raise FileNotFoundError(f"初始化文件不存在：{self.INIT_INI_PATH}")
        if not os.path.exists(self.HW_CFG_PATH):
            raise FileNotFoundError(f"硬件配置文件不存在：{self.HW_CFG_PATH}")
        # 校验并创建序列目录
        if not os.path.exists(self.SEQ_DIR):
            os.makedirs(self.SEQ_DIR, exist_ok=True)
        # 校验并创建数据保存目录
        if not os.path.exists(self.OUTPUT_PATH):
            os.makedirs(self.OUTPUT_PATH, exist_ok=True)

    def _init_id_mapping(self):
        """初始化HC ID到菲特参数的映射（仅保留真实存在的参数）"""
        # 整型参数映射（HC ID → 手册/par文件真实参数名）
        self._int_id_map = {
            IDPI_DIM1: b"noSamples",    # 采样点数（par文件明确存在）
            IDPI_DIM2: b"noEchoes",     # 回波数（par文件明确存在）
            IDPI_DIM3: b"noSlices",     # 层数（par文件明确存在）
            IDPI_DIM4: b"noExperiments",# 实验次数（par文件明确存在）
            IDPI_TD: b"noSamples",      # 采样点数（同DIM1）
            IDPI_RP1CNT: b"noAverages", # 平均次数（par文件明确存在）
            IDPI_RP2CNT: b"noExperiments",
            IDPI_RP3CNT: b"noSlices",
            IDPI_RP4CNT: b"noViews",    # 相位编码步数（par文件明确存在）
        }
        # 接收衰减硬件通道映射（手册2.12.2节）
        self._rx_atten_map = {
            IDPI_RG1: (BOX_TYPE["M"], BOARD_TYPE["RX1"], 0),  # RG1=主机箱RX1板通道0
            IDPI_RG2: (BOX_TYPE["M"], BOARD_TYPE["RX2"], 0)   # RG2=主机箱RX2板通道0
        }
        # 浮点参数映射（HC ID → 真实参数名/硬件通道）
        self._float_id_map = {
            IDPF_SFO1: ("tx_center_fre", BOX_TYPE["M"], BOARD_TYPE["TX1"], 0),
            IDPF_DT2: b"samplePeriod",  # 采样周期（par文件明确存在，替换虚构的dt2）
            IDPF_RFA0: ("tx_att", BOX_TYPE["M"], BOARD_TYPE["TX1"], 0),
        }

        # 梯度矩阵映射
        self._grad_matrix_map = {
            IDPF_GSLICEX + i: f"gradMatrix{i}" for i in range(9)
        }

        # 预加重映射（严格对齐手册preempChannel/PreempKeys）
        self._preemp_map = {
            # X梯度预加重（A1-A5/T1-T5）
            IDPF_PREXA1 + 0: (PREEMP_CHANNEL["XX"], PREEMP_KEYS["A1"]),
            IDPF_PREXA1 + 2: (PREEMP_CHANNEL["XX"], PREEMP_KEYS["A2"]),
            IDPF_PREXA1 + 4: (PREEMP_CHANNEL["XX"], PREEMP_KEYS["A3"]),
            IDPF_PREXA1 + 6: (PREEMP_CHANNEL["XX"], PREEMP_KEYS["A4"]),
            IDPF_PREXA1 + 8: (PREEMP_CHANNEL["XX"], PREEMP_KEYS["A5"]),
            IDPF_PREXK1 + 1: (PREEMP_CHANNEL["XX"], PREEMP_KEYS["T1"]),
            IDPF_PREXK1 + 3: (PREEMP_CHANNEL["XX"], PREEMP_KEYS["T2"]),
            IDPF_PREXK1 + 5: (PREEMP_CHANNEL["XX"], PREEMP_KEYS["T3"]),
            IDPF_PREXK1 + 7: (PREEMP_CHANNEL["XX"], PREEMP_KEYS["T4"]),
            IDPF_PREXK1 + 9: (PREEMP_CHANNEL["XX"], PREEMP_KEYS["T5"]),
            # Y梯度预加重（A1-A5/T1-T5）
            IDPF_PREYA1 + 0: (PREEMP_CHANNEL["YY"], PREEMP_KEYS["A1"]),
            IDPF_PREYA1 + 2: (PREEMP_CHANNEL["YY"], PREEMP_KEYS["A2"]),
            IDPF_PREYA1 + 4: (PREEMP_CHANNEL["YY"], PREEMP_KEYS["A3"]),
            IDPF_PREYA1 + 6: (PREEMP_CHANNEL["YY"], PREEMP_KEYS["A4"]),
            IDPF_PREYA1 + 8: (PREEMP_CHANNEL["YY"], PREEMP_KEYS["A5"]),
            IDPF_PREYK1 + 1: (PREEMP_CHANNEL["YY"], PREEMP_KEYS["T1"]),
            IDPF_PREYK1 + 3: (PREEMP_CHANNEL["YY"], PREEMP_KEYS["T2"]),
            IDPF_PREYK1 + 5: (PREEMP_CHANNEL["YY"], PREEMP_KEYS["T3"]),
            IDPF_PREYK1 + 7: (PREEMP_CHANNEL["YY"], PREEMP_KEYS["T4"]),
            IDPF_PREYK1 + 9: (PREEMP_CHANNEL["YY"], PREEMP_KEYS["T5"]),
            # Z梯度预加重（A1-A5/T1-T5）
            IDPF_PREZA1 + 0: (PREEMP_CHANNEL["ZZ"], PREEMP_KEYS["A1"]),
            IDPF_PREZA1 + 2: (PREEMP_CHANNEL["ZZ"], PREEMP_KEYS["A2"]),
            IDPF_PREZA1 + 4: (PREEMP_CHANNEL["ZZ"], PREEMP_KEYS["A3"]),
            IDPF_PREZA1 + 6: (PREEMP_CHANNEL["ZZ"], PREEMP_KEYS["A4"]),
            IDPF_PREZA1 + 8: (PREEMP_CHANNEL["ZZ"], PREEMP_KEYS["A5"]),
            IDPF_PREZK1 + 1: (PREEMP_CHANNEL["ZZ"], PREEMP_KEYS["T1"]),
            IDPF_PREZK1 + 3: (PREEMP_CHANNEL["ZZ"], PREEMP_KEYS["T2"]),
            IDPF_PREZK1 + 5: (PREEMP_CHANNEL["ZZ"], PREEMP_KEYS["T3"]),
            IDPF_PREZK1 + 7: (PREEMP_CHANNEL["ZZ"], PREEMP_KEYS["T4"]),
            IDPF_PREZK1 + 9: (PREEMP_CHANNEL["ZZ"], PREEMP_KEYS["T5"]),
        }

        # 列表参数映射
        self._float_list_map = {
            IDPF_GSH0: b"gradShape0",
            IDPF_VDL0: b"vdl0",
            IDPF_RFAL0: b"rfAmplitudeList0",
            IDPF_FQL0: b"freqList0",
            IDPF_PHL0: b"phaseList0",
            IDPF_PCL0: b"phaseCodeList0",
            IDPF_GL0: b"gradList0",
            IDPF_GML: b"gradMatrixList"
        }
        self._int_list_map = {
            IDPI_DBL_D2: b"delayList2"
        }

    def _connect(self) -> bool:
        """设备连接（严格遵循手册2.1+2.2节流程）"""
        # 1. 初始化谱仪（手册P9）
        init_ret = self.lib.Init(self.INIT_INI_PATH.encode("utf-8"))
        if init_ret != 0:
            raise RuntimeError(f"谱仪初始化失败，错误码：{init_ret}（手册P9错误码说明）")
        
        # 2. 硬件配置（手册P10，使用init.ini指定的box_m_info.hw）
        # config_ret = self.lib.ConfigFile(self.INIT_INI_PATH.encode("utf-8"))
        # if config_ret != 0:
        #    self.lib.CloseSys()
        #    raise RuntimeError(f"硬件配置失败，错误码：{config_ret}（手册P10错误码说明）")
        
        cfg_ret = self.lib.ConfigFile(self.INIT_INI_PATH.encode('utf-8'))
        if cfg_ret != 0:
            raise RuntimeError(f'config error, code is {cfg_ret}')
        # 3. 设置数据保存路径和模式（手册2.9节）
        save_path_ret = self.lib.SetOutputPath(self.OUTPUT_PATH.encode("utf-8"))
        if save_path_ret != 0:
            self.lib.CloseSys()
            raise RuntimeError(f"设置保存路径失败，错误码：{save_path_ret}（手册P46）")
        self.lib.SetSaveMode(1)  # 边接收边保存（手册P46）
        
        return True

    # -------------------------- 原有接口兼容实现（修正后） --------------------------
    def updateui(self):
        """更新UI（兼容原有接口）"""
        print(f"UI更新：当前序列文件：{self.current_par_file}，设备连接状态：{self.is_connected}")

    def set_sequence(self, seq: str) -> bool:
        """设置序列（手册2.4节 SetParameterFile，匹配实际par文件后缀）"""
        seq_path = os.path.join(self.SEQ_DIR, f"{seq}{self.PAR_FILE_SUFFIX}")
        if not os.path.exists(seq_path):
            print(f"序列文件不存在：{seq_path}")
            return False
        seq_path_bytes = seq_path.encode("utf-8")
        self.current_par_file = seq_path_bytes
        # isedit=False=扫描模式，参数同步到谱仪
        ret = self.lib.SetParameterFile(seq_path_bytes, False)
        if ret != 0:
            print(f"序列加载失败，错误码：{ret}（手册P29错误码说明）")
            return False
        print(f"序列{seq}加载成功")
        return True

    def get_all_fid(self, ch: int, dim1: int, dim2: int = 1, dim3: int = 1, dim4: int = 1, squeeze: bool = True, timeout: int = 300) -> np.ndarray:
        """
        获取FID数据（严格对齐手册第四章RAW格式，包含完整扫描启动+状态轮询+超时控制）
        :param ch: 通道号
        :param dim1: 维度1（采样点数）
        :param dim2: 维度2（回波数）
        :param dim3: 维度3（层数）
        :param dim4: 维度4（实验次数）
        :param squeeze: 是否压缩维度
        :param timeout: 扫描超时时间（秒），默认5分钟
        :return: 复数格式的FID数据
        """
        # 前置校验：确保已加载序列文件
        if not self.current_par_file:
            raise RuntimeError("获取FID前必须先调用set_sequence加载序列文件（手册2.4节）")
        
        # 1. 启动扫描（手册2.5节：PrepareRun + Run 完整流程）
        print(f"[{time.ctime()}] 开始准备扫描...")
        prep_ret = self.lib.C_PrepareRun(False)  # False=非调试模式
        if prep_ret != 0:
            raise RuntimeError(f"扫描准备失败，错误码：{prep_ret}（手册P34）")
        
        run_ret = self.lib.Run()  # 启动扫描
        if run_ret != 0:
            raise RuntimeError(f"扫描启动失败，错误码：{run_ret}（手册P33）")
        print(r'try to wait for scan')
        while((self.lib.ScanCompleted() != 0) and (self.lib.ScanCompleted() != 3)):
            time.sleep(3)
            print(f'scancompleted is {self.lib.ScanCompleted()}')
            print(f'getTotalScanNo is {self.lib.GetTotalScanNo()}')
            print(f"getCurrentScanNo is {self.lib.GetCurrentScanNo()}")
    
        print(f"[{time.ctime()}] 扫描已启动，开始等待完成...")
        # 2. 轮询扫描状态（增加超时控制，修正拼写错误Scanstatus，优化轮询间隔）
        start_time = time.time()
        while True:
            # 检查超时
            if time.time() - start_time > timeout:
                self.lib.Abort()  # 超时终止扫描
                raise TimeoutError(f"扫描超时（{timeout}秒），已终止扫描")
            
            # 获取扫描状态（修正拼写：Scanstatus，手册P36）
            status = self.lib.ScanStatus()
            print(f"[{time.ctime()}] 扫描状态码：{status}（3=完成/4=写数据中，1=扫描中/2=传数据中）")
            
            # 状态判断（手册P36状态码定义）
            if status in (3, 4):  # 3=扫描+传输完成，4=正在写数据（可读取）
                break
            elif status in (-1, 5, 6):  # -1=未初始化/5=扫描错误/6=硬件错误
                self.lib.Abort()
                raise RuntimeError(f"扫描异常，状态码：{status}（手册P36）")
            
            # 合理轮询间隔（1秒，避免600秒过长导致无响应）
            time.sleep(1)
    
        # 3. 定位最新的.raw数据文件（手册P88命名规则）
        raw_files = glob.glob(os.path.join(self.OUTPUT_PATH, "*.raw"))
        if not raw_files:
            raise FileNotFoundError(f"在保存目录 {self.OUTPUT_PATH} 未找到.raw数据文件")
        
        # 取最新修改的.raw文件（确保读取本次扫描的数据）
        latest_raw = max(raw_files, key=os.path.getmtime)
        file_size = os.path.getsize(latest_raw)
        print(f"[{time.ctime()}] 找到最新RAW文件：{latest_raw}，大小：{file_size}字节")
        
        # 4. 校验文件最小长度（必须大于64K文件头）
        if file_size < RAW_DATA_OFFSET:
            raise ValueError(f"RAW文件损坏，实际大小{file_size}字节，小于最小文件头{RAW_DATA_OFFSET}字节")
    
        # 5. 读取二进制数据（跳过64K文件头，仅读取采集数据部分，手册第四章）
        with open(latest_raw, "rb") as f:
            f.seek(RAW_DATA_OFFSET)  # 跳过序列信息头
            raw_data = np.fromfile(f, dtype=np.float32)
        
        # 6. 校验数据长度
        expected_data_len = dim1 * 2 * dim2 * dim3 * dim4  # I/Q双通道，各占dim1个点
        if len(raw_data) < expected_data_len:
            raise ValueError(
                f"采集数据长度不匹配：实际{len(raw_data)}个点，预期{expected_data_len}个点\n"
                f"维度参数：dim1={dim1}, dim2={dim2}, dim3={dim3}, dim4={dim4}"
            )
        # 截取有效数据（避免多余字节干扰）
        raw_data = raw_data[:expected_data_len]
    
        # 7. 重塑数组（Fortran序，手册P92存储顺序）
        fid = np.reshape(raw_data, (dim1*2, dim2, dim3, dim4), order='F')
    
        # 8. 分离I/Q通道，组合为复数数组（I=偶数行，Q=奇数行，手册第四章）
        fid_complex = fid[::2, ...] + 1j * fid[1::2, ...]
    
        # 9. 维度压缩（保持原有接口逻辑）
        result = fid_complex if not squeeze else np.squeeze(fid_complex)
        print(f"[{time.ctime()}] FID数据获取完成，数据形状：{result.shape}")
        return result

    # -------------------------- 维度参数获取/设置（修正后） --------------------------
    def get_dim1_count(self) -> int:
        return float(self.lib.GetParameter(self._int_id_map[IDPI_DIM1]))

    def get_dim2_count(self) -> int:
        return float(self.lib.GetParameter(self._int_id_map[IDPI_DIM2]))

    def get_dim3_count(self) -> int:
        return float(self.lib.GetParameter(self._int_id_map[IDPI_DIM3]))

    def get_dim4_count(self) -> int:
        return float(self.lib.GetParameter(self._int_id_map[IDPI_DIM4]))

    def set_td(self, td: int) -> bool:
        """设置采样点数（手册2.4 SetParameter）"""
        ret = self.lib.SetParameter(self._int_id_map[IDPI_TD], float(td))
        return ret == 0

    def set_Dim(self, dim2, dim3: int = 1, dim4: int = 1) -> bool:
        ret1 = self.lib.SetParameter(self._int_id_map[IDPI_DIM2], float(dim2))
        ret2 = self.lib.SetParameter(self._int_id_map[IDPI_DIM3], float(dim3))
        ret3 = self.lib.SetParameter(self._int_id_map[IDPI_DIM4], float(dim4))
        return all([ret1 == 0, ret2 == 0, ret3 == 0])

    # -------------------------- 射频/采样参数设置（修正后） --------------------------
    def set_sfo1(self, sfo1: float) -> bool:
        """设置发射中心频率（手册2.10.4 SetTxCenterFre）"""
        _, box, board, ch = self._float_id_map[IDPF_SFO1]
        ret = self.lib.SetTxCenterFre(box, board, ch, sfo1)
        return ret == 0

    def get_dt2(self) -> float:
        """获取采样周期（手册2.4 GetParameter，修正虚构的dt2参数）"""
        return float(self.lib.GetParameter(self._float_id_map[IDPF_DT2]))

    def set_sw(self, sample_period: float) -> bool:
        """设置采样周期（间接控制采样带宽，替换虚构的sw参数）"""
        ret = self.lib.SetParameter(self._float_id_map[IDPF_DT2], sample_period)
        return ret == 0

    # -------------------------- 接收衰减设置（修正后，删除虚构的GetRxATT） --------------------------
    def set_RG1(self, rg1: int) -> bool:
        """设置接收衰减1（RG1）- 手册2.12.2节 SetRxATT"""
        if not (0 <= rg1 <= 60):
            raise ValueError(f"接收衰减RG1需在0-60dB之间，当前输入：{rg1}（手册P59）")
        box, board, ch = self._rx_atten_map[IDPI_RG1]
        # 手册参数：box, board, ch, att, amp1, amp2, amp3, switchValue
        ret = self.lib.SetRxATT(box, board, ch, float(rg1), 20.0, 20.0, 20.0, 1)
        return ret == 0

    def set_RG2(self, rg2: int) -> bool:
        """设置接收衰减2（RG2）- 手册2.12.2节 SetRxATT"""
        if not (0 <= rg2 <= 60):
            raise ValueError(f"接收衰减RG2需在0-60dB之间，当前输入：{rg2}（手册P59）")
        box, board, ch = self._rx_atten_map[IDPI_RG2]
        ret = self.lib.SetRxATT(box, board, ch, float(rg2), 20.0, 20.0, 20.0, 1)
        return ret == 0

    def get_RxAttInfo(self) -> str:
        """获取接收衰减信息（手册2.2.3节 唯一合法接口）"""
        info = self.lib.GetRxAttInfo()
        return info.decode("utf-8") if info else "获取失败"

    # -------------------------- 下采样率设置（修正后，删除虚构函数，通过采样周期实现） --------------------------
    def set_DS(self, ds: int) -> bool:
        """设置下采样率（通过调整采样周期间接实现，无虚构函数）"""
        valid_ds = [1, 2, 4, 8, 16]
        if ds not in valid_ds:
            raise ValueError(f"下采样率仅支持{valid_ds}，当前输入：{ds}")
        # 基准采样周期20us，下采样率=ds → 采样周期=基准*ds
        base_sample_period = 20.0
        target_period = base_sample_period * ds
        return self.set_sw(target_period)

    # -------------------------- 梯度矩阵/偏移设置（核心修正，区分延时和偏移） --------------------------
    def set_GM(self, SPR_XYZ: np.ndarray) -> bool:
        """设置梯度旋转矩阵（手册2.11.2 SetAllMaxtrixValue）"""
        if len(SPR_XYZ) != 9:
            raise ValueError("梯度矩阵必须是3x3展平的9个元素")
        # 转换为C浮点数组
        gm_array = (c_float * 9)(*SPR_XYZ.astype(float))
        ret = self.lib.SetAllMaxtrixValue(gm_array, 1)
        return ret == 0

    def set_grad_offset(self, gxo: float, gyo: float, gzo: float) -> bool:
        """设置梯度偏移（匀场值）- 手册2.3.1节 SetChannelValue（核心修正）"""
        try:
            self.lib.SetChannelValue(SHIM_CHANNEL["X"], gxo)
            self.lib.SetChannelValue(SHIM_CHANNEL["Y"], gyo)
            self.lib.SetChannelValue(SHIM_CHANNEL["Z"], gzo)
            return True
        except Exception as e:
            print(f"梯度偏移设置失败：{str(e)}")
            return False

    def set_grad_delay(self, delay_us: float) -> bool:
        """设置梯度功放延时（手册2.3.2节 SetGradDelay，独立功能）"""
        if not (142.0 <= delay_us <= 512.0):
            raise ValueError(f"梯度功放延时需在142-512us之间，当前输入：{delay_us}（手册P26）")
        ret = self.lib.SetGradDelay(delay_us)
        return ret == 0

    # -------------------------- 梯度预加重设置（修正后） --------------------------
    def set_Gx_preemphasis(self, A5, K5) -> bool:
        if len(A5) != 5 or len(K5) != 5:
            raise ValueError("A5/K5必须为5个元素")
        ret = True
        for i in range(5):
            hc_id = IDPF_PREXA1 + 2 * i
            channel, key = self._preemp_map[hc_id]
            ret &= (self.lib.SetPreempValue(channel, key, A5[i]) == 0)
        for i in range(5):
            hc_id = IDPF_PREXK1 + 2 * i + 1
            channel, key = self._preemp_map[hc_id]
            ret &= (self.lib.SetPreempValue(channel, key, K5[i]) == 0)
        return ret

    def set_Gy_preemphasis(self, A5, K5) -> bool:
        if len(A5) != 5 or len(K5) != 5:
            raise ValueError("A5/K5必须为5个元素")
        ret = True
        for i in range(5):
            hc_id = IDPF_PREYA1 + 2 * i
            channel, key = self._preemp_map[hc_id]
            ret &= (self.lib.SetPreempValue(channel, key, A5[i]) == 0)
        for i in range(5):
            hc_id = IDPF_PREYK1 + 2 * i + 1
            channel, key = self._preemp_map[hc_id]
            ret &= (self.lib.SetPreempValue(channel, key, K5[i]) == 0)
        return ret

    def set_Gz_preemphasis(self, A5, K5) -> bool:
        if len(A5) != 5 or len(K5) != 5:
            raise ValueError("A5/K5必须为5个元素")
        ret = True
        for i in range(5):
            hc_id = IDPF_PREZA1 + 2 * i
            channel, key = self._preemp_map[hc_id]
            ret &= (self.lib.SetPreempValue(channel, key, A5[i]) == 0)
        for i in range(5):
            hc_id = IDPF_PREZK1 + 2 * i + 1
            channel, key = self._preemp_map[hc_id]
            ret &= (self.lib.SetPreempValue(channel, key, K5[i]) == 0)
        return ret

    # -------------------------- 重复次数设置（修正后） --------------------------
    def set_repeat(self, rp1, rp2: int = 1, rp3: int = 1, rp4: int = 1) -> bool:
        ret1 = self.lib.SetParameter(self._int_id_map[IDPI_RP1CNT], float(rp1))
        ret2 = self.lib.SetParameter(self._int_id_map[IDPI_RP2CNT], float(rp2))
        ret3 = self.lib.SetParameter(self._int_id_map[IDPI_RP3CNT], float(rp3))
        ret4 = self.lib.SetParameter(self._int_id_map[IDPI_RP4CNT], float(rp4))
        return all([ret1 == 0, ret2 == 0, ret3 == 0, ret4 == 0])

    # -------------------------- 脉冲/延迟/射频幅度设置（修正后） --------------------------
    def set_pulse_width(self, id: PulseID, pw: float) -> bool:
        param_name = f"pulseWidth{id}".encode("utf-8")
        ret = self.lib.SetParameter(param_name, pw)
        if ret != 0:
            print(f"警告：参数{param_name.decode()}不存在于当前par文件，错误码{ret}")
        return ret == 0

    def set_delay(self, id: DelayID, d: float) -> bool:
        param_name = f"delay{id}".encode("utf-8")
        ret = self.lib.SetParameter(param_name, d)
        if ret != 0:
            print(f"警告：参数{param_name.decode()}不存在于当前par文件，错误码{ret}")
        return ret == 0

    def set_RFA(self, id: RFAID, rfa: float) -> bool:
        """设置发射衰减（手册2.10节 SetTxATT，删除无依据的20-rfa逻辑）"""
        _, box, board, ch = self._float_id_map[IDPF_RFA0]
        ret = self.lib.SetTxATT(box, board, ch, float(rfa))
        return ret == 0

    def set_freq(self, id: FreqID, fq: float) -> bool:
        param_name = f"rfFreq{id}".encode("utf-8")
        ret = self.lib.SetParameter(param_name, fq)
        if ret != 0:
            print(f"警告：参数{param_name.decode()}不存在于当前par文件，错误码{ret}")
        return ret == 0

    def set_RFShape(self, id: RFShapeID, shape: str) -> bool:
        """设置发射波形（手册2.10.5 SetRFWaves）"""
        shape_path = os.path.join(self.SEQ_DIR, f"{shape}.rfwave")
        if not os.path.exists(shape_path):
            print(f"波形文件不存在：{shape_path}")
            return False
        ret = self.lib.SetRFWaves(shape_path.encode("utf-8"))
        return ret == 0

    def set_const(self, id: ConstID, c: int) -> bool:
        param_name = f"const{id}".encode("utf-8")
        ret = self.lib.SetParameter(param_name, float(c))
        if ret != 0:
            print(f"警告：参数{param_name.decode()}不存在于当前par文件，错误码{ret}")
        return ret == 0

    def set_GA(self, id: GradAmpID, ga: float) -> bool:
        param_name = f"gradAmp{id}".encode("utf-8")
        ret = self.lib.SetParameter(param_name, ga)
        if ret != 0:
            print(f"警告：参数{param_name.decode()}不存在于当前par文件，错误码{ret}")
        return ret == 0

    # -------------------------- 列表参数设置（修正后） --------------------------
    def set_GShape(self, id: GradShapeID, shape: np.ndarray) -> bool:
        param_name = self._float_list_map[IDPF_GSH0 + id]
        shape_c = (c_float * len(shape))(*shape.astype(float))
        ret = self.lib.SetParameterArray(param_name, shape_c, len(shape))
        return ret == 0

    def set_VDL(self, id: VDLID, v: np.ndarray) -> bool:
        param_name = self._float_list_map[IDPF_VDL0 + id]
        v_c = (c_float * len(v))(*v.astype(float))
        ret = self.lib.SetParameterArray(param_name, v_c, len(v))
        return ret == 0

    def set_RFAL(self, id: RFALID, rfal: np.ndarray) -> bool:
        param_name = self._float_list_map[IDPF_RFAL0 + id]
        rfal_c = (c_float * len(rfal))(*rfal.astype(float))
        ret = self.lib.SetParameterArray(param_name, rfal_c, len(rfal))
        return ret == 0

    def set_FQL(self, id: FQLID, fql: np.ndarray) -> bool:
        param_name = self._float_list_map[IDPF_FQL0 + id]
        fql_c = (c_float * len(fql))(*fql.astype(float))
        ret = self.lib.SetParameterArray(param_name, fql_c, len(fql))
        return ret == 0

    def set_PHL(self, id: PHLID, phl: np.ndarray) -> bool:
        param_name = self._float_list_map[IDPF_PHL0 + id]
        phl_c = (c_float * len(phl))(*phl.astype(float))
        ret = self.lib.SetParameterArray(param_name, phl_c, len(phl))
        return ret == 0

    def set_PCL(self, id: PCLID, pcl: np.ndarray) -> bool:
        param_name = self._float_list_map[IDPF_PCL0 + id]
        pcl_c = (c_float * len(pcl))(*pcl.astype(float))
        ret = self.lib.SetParameterArray(param_name, pcl_c, len(pcl))
        return ret == 0

    def set_GL(self, id: GLID, gl: np.ndarray) -> bool:
        param_name = self._float_list_map[IDPF_GL0 + id]
        gl_c = (c_float * len(gl))(*gl.astype(float))
        ret = self.lib.SetParameterArray(param_name, gl_c, len(gl))
        return ret == 0

    def set_DBL(self, id: DBLID, dbl: np.ndarray) -> bool:
        param_name = self._int_list_map[IDPI_DBL_D2 + id]
        dbl_c = (c_float * len(dbl))(*dbl.astype(float))
        ret = self.lib.SetParameterArray(param_name, dbl_c, len(dbl))
        return ret == 0

    def set_GML(self, gml: np.ndarray) -> bool:
        param_name = self._float_list_map[IDPF_GML]
        gml_c = (c_float * len(gml))(*gml.astype(float))
        ret = self.lib.SetParameterArray(param_name, gml_c, len(gml))
        return ret == 0

    # -------------------------- 目录/状态获取（修正后，删除虚构函数） --------------------------
    def get_root_dir(self) -> str:
        """获取DLL根目录（手册2.15.4 GetDLLPath）"""
        return self.lib.GetDLLPath()

    def get_seq_dir(self, seq: str) -> str:
        """获取序列目录（本地路径实现，无虚构DLL调用）"""
        seq_dir = os.path.join(self.SEQ_DIR, seq)
        if not os.path.exists(seq_dir):
            os.makedirs(seq_dir, exist_ok=True)
        return os.path.normpath(seq_dir)

    def RTZG(self):
        """梯度归零（设置单位矩阵）"""
        unit_matrix = np.eye(3).flatten()
        self.set_GM(unit_matrix)
        print("梯度矩阵已归零（单位矩阵）")

    def is_scanning(self) -> bool:
        """判断是否扫描中（手册2.5 ScanStatus）"""
        status = self.lib.ScanStatus()
        return status in (1, 2)  # 1=扫描中，2=数据上传中

    def start_scan(self) -> bool:
        """启动序列扫描（补充缺失的核心接口）"""
        if not self.current_par_file:
            print("请先加载序列文件")
            return False
        # 扫描前准备
        prep_ret = self.lib.PrepareRun(False)
        if prep_ret != 0:
            print(f"扫描准备失败，错误码：{prep_ret}")
            return False
        # 启动扫描
        run_ret = self.lib.Run()
        if run_ret != 0:
            print(f"扫描启动失败，错误码：{run_ret}")
            return False
        print("扫描已启动")
        return True

    def __del__(self):
        """析构函数：安全关闭系统"""
        if hasattr(self, 'lib') and self.is_connected:
            try:
                self.lib.Abort()
                self.lib.CloseSys()
                print("菲特谱仪连接已安全关闭")
            except Exception as e:
                print(f"关闭谱仪时出错：{str(e)}")
    
    def setParaFile(self, pth: str) -> bool:
        
        # txt = pth.decode('uft-8-sig')
        tmpPth = pth.encode('utf-8')
        # tmpPth = pth.encode('ascii', errors='replace')
        # print(f'the 8p is {tmpPth}')
        ret = self.lib.SetParameterFile(tmpPth, False)
        print(f'ret after par file set is {ret}')
        if ret != 0:
            print(f"setParaFile 序列加载失败，错误码：{ret}（手册P29错误码说明）")
            return False
        self.current_par_file = tmpPth
        print(f"序列{tmpPth} 加载成功 ")
        return True

# -------------------------- 测试代码（适配修正后的代码） --------------------------
if __name__ == "__main__":
    print(os.getcwd())
    plt.figure()
    plt.close()


    print("test 1")
    # 创建console实例
    try:
        c = console()
        print("initialized successfully, device is connected")
    except Exception as e:
        print(f"initialize failed: {str(e)}")
        sys.exit(1)
    print("test 2")    
        
    
    # 基础接口测试
    s = c.get_root_dir()
    #safe_s = s.encode('utf-8', errors='replace').decode('utf-8')
    print(f"the dll root path: {s}")

    # 加载序列（匹配实际提供的par文件名）
    #test_seq_list = ["1pulse", "pre_adj_demo_ft_X", "pre_adj_demo_ft_Y", "pre_adj_demo_ft_Z"]
    #for seq in test_seq_list:
    #    if c.set_sequence(seq):
    #        print(f"sequence: {seq} loading successfully")
    #        break

    c.lib.SetAverageMode(0)
    c.lib.SetSaveMode(1)

    if not c.setParaFile(TMP_PTH):
        print('par setting failed')
    # 测试参数设置
    #c.set_td(512)
    #c.set_sw(20.0)  # 设置采样周期20us
    #c.set_RG1(20)
    #c.set_repeat(1)
    #c.set_GM(np.array([1,0,0, 0,1,0, 0,0,1]))
    #c.set_grad_offset(0.0, 0.0, 0.0)  # 梯度偏移归零
    ret = c.lib.SetChannelValid('255'.encode('utf-8'))
    ret |= c.lib.SetParameter('viewBlock'.encode('utf-8'), 1)
    ret |= c.lib.SetParameter('TR'.encode('utf-8'), 500)
    for i in range(8):
        ret |= c.lib.SetTxCenterFre(0, 4, i, 50)
    c.lib.SetChannelValue(0, 1000)
    c.lib.SetChannelValue(1, 1000)
    c.lib.SetChannelValue(2, 1000)

    if ret != 0:
        print(f'here is an error occur, {ret}')
    

    if c.is_scanning():
        print(r'is scanning')
    else:
        print(r'is not scanning')
    
    ret = c.lib.Run()
    if ret != 0:
        print(f'error in Run {ret}')
    
    while True:
        scanStatus = c.lib.ScanCompleted()
        if scanStatus == 0 or scanStatus == 3:
            break
        time.sleep(3)
        print(f'scan completed is {scanStatus}')
        print(f'total scan is {c.lib.GetTotalScanNo()}')
        print(f'current scan no is {c.lib.GetCurrentScanNo()}')

    # dim1 = c.get_dim1_count()
    # dim2 = c.get_dim2_count()
    # dim3 = c.get_dim3_count()
    # dim4 = c.get_dim4_count()
    # print(f'dim1: {dim1}, dim2: {dim2}, dim3: {dim3}, dim4: {dim4}')
    # fid = c.get_all_fid(0, dim1, dim2, dim3, dim4)
    # print(fid)

    # 查看接收衰减信息
    print(f"接收衰减信息：{c.get_RxAttInfo()}")

    print("测试完成")