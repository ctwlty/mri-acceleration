import ctypes
import time
from ctypes import wintypes

# -------------------------- 常量定义 --------------------------
# DLL路径：必须用 字符串(str)，ctypes要求
DLL_PTH = "D:\\123pan\\Downloads\\testDLL\\bin\\x64\\Debug\\mridll.dll"
# 所有传入DLL函数的字符串 → 统一 UTF-8 编码
SHIM_VALUE = 1000.0
PAR_PATH = "D:\\123pan\\Downloads\\testDLL\\bin\\x64\\Debug\\PTScan.par".encode('utf-8')
OUTPUT_PATH = "D:\\mri_data".encode('utf-8')
OUTPUT_PREFIX = "mir_".encode('utf-8')
CHANNEL_SELECT = "255".encode('utf-8')
INITFILE = "D:\\123pan\\Downloads\\testDLL\\bin\\x64\\Debug\\hw_cfg\\init.ini".encode('utf-8')
SAVE_PAR = "D:\\mri_data\\new.par".encode('utf-8')

# -------------------------- 函数类型定义 --------------------------
class DllFunc:
    def __init__(self):
        # 初始化所有函数句柄为None
        self.Init = None
        self.ConfigFile = None
        self.SetAverageMode = None
        self.SetSaveMode = None
        self.SetParameterFile = None
        self.SetOutputPrefix = None
        self.SetChannelValid = None
        self.SetParameter = None
        self.SetTxCenterFre = None
        self.SaveParameterFile = None
        self.SetChannelValue = None
        self.Run = None
        self.Abort = None
        self.ScanCompleted = None
        self.CloseSys = None
        self.SetOutputPath = None
        self.GetTotalScanNo = None
        self.GetCurrentScanNo = None

# 函数名列表（与原C++保持一致）
FUNC_NAMES = [
    "Init",              # 0
    "ConfigFile",        # 1
    "SetAverageMode",    # 2
    "SetSaveMode",       # 3
    "SetParameterFile",  # 4
    "SetOutputPrefix",   # 5
    "SetChannelValid",   # 6
    "SetParameter",      # 7
    "SetTxCenterFre",    # 8
    "SaveParameterFile", # 9
    "SetChannelValue",   # 10
    "Run",               # 11
    "Abort",             # 12
    "ScanCompleted",     # 13
    "CloseSys",          # 14
    "SetOutputPath",     # 15
    "GetTotalScanNo",    # 16
    "GetCurrentScanNo"   # 17
]

# -------------------------- 加载DLL并绑定函数 --------------------------
def tie_func(dll, df):
    """绑定DLL中的函数到DllFunc实例（修复：移除错误的GetProcAddress）"""
    # 定义每个函数的参数类型和返回值类型
    func_specs = [
        # (返回值类型, (参数类型1, 参数类型2, ...))
        (wintypes.INT, (ctypes.c_char_p,)),          # Init
        (wintypes.INT, (ctypes.c_char_p,)),          # ConfigFile
        (None, (wintypes.INT,)),                     # SetAverageMode
        (None, (wintypes.INT,)),                     # SetSaveMode
        (wintypes.INT, (ctypes.c_char_p, wintypes.BOOL)), # SetParameterFile
        (wintypes.INT, (ctypes.c_char_p,)),          # SetOutputPrefix
        (wintypes.INT, (ctypes.c_char_p,)),          # SetChannelValid
        (wintypes.INT, (ctypes.c_char_p, ctypes.c_double)), # SetParameter
        (wintypes.INT, (wintypes.INT, wintypes.INT, wintypes.INT, ctypes.c_double)), # SetTxCenterFre
        (wintypes.INT, (ctypes.c_char_p,)),          # SaveParameterFile
        (None, (wintypes.INT, ctypes.c_float)),      # SetChannelValue
        (wintypes.INT, ()),                          # Run
        (None, ()),                                  # Abort
        (wintypes.INT, ()),                          # ScanCompleted
        (None, ()),                                  # CloseSys
        (wintypes.INT, (ctypes.c_char_p,)),          # SetOutputPath
        (wintypes.INT, ()),                          # GetTotalScanNo
        (wintypes.INT, ())                           # GetCurrentScanNo
    ]

    for idx, func_name in enumerate(FUNC_NAMES):
        try:
            # ✅ 修复：直接从DLL获取函数（ctypes原生用法，无需GetProcAddress）
            func = getattr(dll, func_name)
            # 设置函数参数/返回值类型
            restype, argtypes = func_specs[idx]
            func.restype = restype
            func.argtypes = argtypes
            # 绑定到对象
            setattr(df, func_name, func)
        except AttributeError:
            print(f"获取函数失败: {func_name} (索引{idx})")
            return 1
    return 0

def get_func_from_dll(df):
    """加载DLL并绑定所有函数"""
    try:
        # 加载DLL
        dll = ctypes.CDLL(DLL_PTH, use_last_error=True)
    except OSError as e:
        print(f"加载DLL失败: {e}，错误码: {ctypes.GetLastError()}")
        return 1
    
    # 绑定函数
    ret = tie_func(dll, df)
    if ret != 0:
        return ret
    
    # 保存DLL句柄防止被GC回收
    df.dll_handle = dll
    return 0

# -------------------------- 主流程 --------------------------
def main():
    print("starting")
    df = DllFunc()
    
    # 加载DLL并绑定函数
    ret = get_func_from_dll(df)
    if ret != 0 or df.Init is None:
        print("get func failed")
        return 0
    print("get func achieve")


    # df.Abort()
    # df.CloseSys()
    # return

    # 初始化
    ret = df.Init(INITFILE)
    print(f"init ret is {ret}")
    ret |= df.ConfigFile(INITFILE)
    if ret != 0:
        error_code = ctypes.GetLastError()
        print(f"初始化MRI失败，错误码: {ret} {error_code}")
        return 1
    print("init over")

    # 设置基础模式
    df.SetAverageMode(0)
    df.SetSaveMode(1)

    # 设置参数文件
    ret = df.SetParameterFile(PAR_PATH, False)
    print(f"the ret in par pth set is {ret}")

    # 设置输出路径和前缀
    ret = df.SetOutputPath(OUTPUT_PATH)
    print(f"the ret in out put pth is {ret}")
    ret = df.SetOutputPrefix(OUTPUT_PREFIX)
    print(f"the ret in out put prefix is {ret}")

    # 设置通道有效性
    ret = df.SetChannelValid(CHANNEL_SELECT)
    print(f"the ret in channel valid is {ret}")

    # 设置参数（UTF-8编码）
    ret = df.SetParameter("viewBlock".encode('utf-8'), 1.0)
    print(f"the ret in view block is {ret}")
    ret = df.SetParameter("TR".encode('utf-8'), 500.0)
    print(f"the ret TR is {ret}")

    # 循环设置TxCenterFre
    tx_ret = 0
    for i in range(8):
        tx_ret |= df.SetTxCenterFre(0, 4, i, 50.0)
    print(f"the ret TxCenterFre is {tx_ret}")
    ret |= tx_ret

    # 保存参数文件
    ret = df.SaveParameterFile(SAVE_PAR)
    print(f"the ret save par is {ret}")

    # 设置通道值
    df.SetChannelValue(0, SHIM_VALUE)
    df.SetChannelValue(1, SHIM_VALUE)
    df.SetChannelValue(2, SHIM_VALUE)

    # 检查参数设置错误
    if ret != 0:
        error_code = ctypes.GetLastError()
        print(f"参数设置出错: {ret} {error_code}")
        return 2

    # 运行扫描
    ret = df.Run()
    if ret != 0:
        error_code = ctypes.GetLastError()
        print(f"运行扫描出错: {ret} {error_code}")
        return 3

    # 循环检查扫描完成状态
    while True:
        scan_status = df.ScanCompleted()
        if scan_status == 0 or scan_status == 3:
            break
        time.sleep(1)
        print(f"ScanCompleted is {scan_status}")
        print(f"TotalScanNo is {df.GetTotalScanNo()}")
        print(f"GetCurrentScanNo is {df.GetCurrentScanNo()}")

    # 终止并关闭系统
    df.Abort()
    df.CloseSys()

    return 0

if __name__ == "__main__":
    exit(main())