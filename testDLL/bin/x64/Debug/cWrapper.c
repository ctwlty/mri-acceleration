#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

// 定义函数指针类型（与DLL导出函数匹配）
typedef int (*InitFunc)(const char*);
typedef int (*ConfigFileFunc)(const char*);
typedef void (*SetAverageModeFunc)(int);
typedef void (*SetSaveModeFunc)(int);
typedef int (*SetParameterFileFunc)(const char*, int); // c_bool对应C的int
typedef int (*SetOutputPathFunc)(const char*);
typedef int (*SetOutputPrefixFunc)(const char*);
typedef int (*SetChannelValidFunc)(const char*);
typedef int (*SetParameterFunc)(const char*, double);
typedef int (*SetTxCenterFreFunc)(int, int, int, double);
typedef int (*SaveParameterFileFunc)(const char*);
typedef void (*SetChannelValueFunc)(int, float);
typedef int (*C_PrepareRunFunc)(int);
typedef int (*RunFunc)();
typedef int (*ScanCompletedFunc)();
typedef void (*AbortFunc)();
typedef void (*CloseSysFunc)();
typedef int (*GetTotalScanNoFunc)();
typedef int (*GetCurrentScanNoFunc)();

// 全局句柄和函数指针
static HMODULE mri_dll_handle = NULL;
static InitFunc Init = NULL;
static ConfigFileFunc ConfigFile = NULL;
static SetAverageModeFunc SetAverageMode = NULL;
static SetSaveModeFunc SetSaveMode = NULL;
static SetParameterFileFunc SetParameterFile = NULL;
static SetOutputPathFunc SetOutputPath = NULL;
static SetOutputPrefixFunc SetOutputPrefix = NULL;
static SetChannelValidFunc SetChannelValid = NULL;
static SetParameterFunc SetParameter = NULL;
static SetTxCenterFreFunc SetTxCenterFre = NULL;
static SaveParameterFileFunc SaveParameterFile = NULL;
static SetChannelValueFunc SetChannelValue = NULL;
static C_PrepareRunFunc C_PrepareRun = NULL;
static RunFunc Run = NULL;
static ScanCompletedFunc ScanCompleted = NULL;
static AbortFunc Abort = NULL;
static CloseSysFunc CloseSys = NULL;
static GetTotalScanNoFunc GetTotalScanNo = NULL;
static GetCurrentScanNoFunc GetCurrentScanNo = NULL;

/**
 * 加载mridll.dll并绑定所有函数指针
 * @return 0成功，-1失败
 */
int MRI_Wrapper_Init(const char* dll_path) {
    if (mri_dll_handle != NULL) {
        printf("DLL已加载\n");
        return 0;
    }

    // 加载DLL
    mri_dll_handle = LoadLibraryA(dll_path);
    if (mri_dll_handle == NULL) {
        printf("加载DLL失败，错误码：%d\n", GetLastError());
        return -1;
    }

    // 绑定所有函数指针（注意函数名需与DLL导出名一致）
    Init = (InitFunc)GetProcAddress(mri_dll_handle, "Init");
    ConfigFile = (ConfigFileFunc)GetProcAddress(mri_dll_handle, "ConfigFile");
    SetAverageMode = (SetAverageModeFunc)GetProcAddress(mri_dll_handle, "SetAverageMode");
    SetSaveMode = (SetSaveModeFunc)GetProcAddress(mri_dll_handle, "SetSaveMode");
    SetParameterFile = (SetParameterFileFunc)GetProcAddress(mri_dll_handle, "SetParameterFile");
    SetOutputPath = (SetOutputPathFunc)GetProcAddress(mri_dll_handle, "SetOutputPath");
    SetOutputPrefix = (SetOutputPrefixFunc)GetProcAddress(mri_dll_handle, "SetOutputPrefix");
    SetChannelValid = (SetChannelValidFunc)GetProcAddress(mri_dll_handle, "SetChannelValid");
    SetParameter = (SetParameterFunc)GetProcAddress(mri_dll_handle, "SetParameter");
    SetTxCenterFre = (SetTxCenterFreFunc)GetProcAddress(mri_dll_handle, "SetTxCenterFre");
    SaveParameterFile = (SaveParameterFileFunc)GetProcAddress(mri_dll_handle, "SaveParameterFile");
    SetChannelValue = (SetChannelValueFunc)GetProcAddress(mri_dll_handle, "SetChannelValue");
    C_PrepareRun = (C_PrepareRunFunc)GetProcAddress(mri_dll_handle, "C_PrepareRun");
    Run = (RunFunc)GetProcAddress(mri_dll_handle, "Run");
    ScanCompleted = (ScanCompletedFunc)GetProcAddress(mri_dll_handle, "ScanCompleted");
    Abort = (AbortFunc)GetProcAddress(mri_dll_handle, "Abort");
    CloseSys = (CloseSysFunc)GetProcAddress(mri_dll_handle, "CloseSys");
    GetTotalScanNo = (GetTotalScanNoFunc)GetProcAddress(mri_dll_handle, "GetTotalScanNo");
    GetCurrentScanNo = (GetCurrentScanNoFunc)GetProcAddress(mri_dll_handle, "GetCurrentScanNo");

    // 检查关键函数是否绑定成功
    if (Init == NULL || Run == NULL || CloseSys == NULL) {
        printf("部分函数绑定失败\n");
        FreeLibrary(mri_dll_handle);
        mri_dll_handle = NULL;
        return -1;
    }

    printf("DLL加载并绑定函数成功\n");
    return 0;
}

/**
 * 封装Python中的simpleRun逻辑
 * @param init_ini_path 初始化INI路径
 * @param par_path 参数文件路径
 * @param save_path 输出路径
 * @param prefix 输出前缀
 * @param save_par_path 保存参数文件路径
 * @return 0成功，非0失败
 */
int MRI_SimpleRun(const char* init_ini_path, const char* par_path, 
                  const char* save_path, const char* prefix, const char* save_par_path) {
    if (mri_dll_handle == NULL) {
        printf("DLL未加载\n");
        return -1;
    }

    int ret = 0;
    // 1. 初始化
    ret = Init(init_ini_path);
    printf("init %d\n", ret);
    if (ret != 0) return ret;

    // 2. 配置文件
    ret = ConfigFile(init_ini_path);
    printf("config %d\n", ret);
    if (ret != 0) return ret;

    // 3. 设置模式
    SetAverageMode(0);
    SetSaveMode(1);

    // 4. 设置参数文件
    ret = SetParameterFile(par_path, 0); // False对应0
    printf("par %d\n", ret);
    if (ret != 0) return ret;

    // 5. 设置输出路径和前缀
    ret = SetOutputPath(save_path);
    printf("output pth %d\n", ret);
    if (ret != 0) return ret;

    ret = SetOutputPrefix(prefix);
    printf("prefix %d\n", ret);
    if (ret != 0) return ret;

    // 6. 设置通道有效
    ret = SetChannelValid("255");
    printf("channel valid %d\n", ret);
    if (ret != 0) return ret;

    // 7. 设置参数
    ret = SetParameter("viewBlock", 1.0);
    ret |= SetParameter("TR", 500.0);
    printf("set para %d\n", ret);
    if (ret != 0) return ret;

    // 8. 设置发射中心频率
    for (int i = 0; i < 8; i++) {
        ret |= SetTxCenterFre(0, 4, i, 50.0);
    }
    printf("txcenterfre %d\n", ret);
    if (ret != 0) return ret;

    // 9. 保存参数文件
    ret = SaveParameterFile(save_par_path);
    printf("save para file %d\n", ret);
    if (ret != 0) return ret;

    // 10. 设置通道值
    SetChannelValue(0, 1000.0f);
    SetChannelValue(1, 1000.0f);
    SetChannelValue(2, 1000.0f);

    // 11. 运行扫描
    ret = Run();
    printf("run %d\n", ret);
    if (ret != 0) return ret;

    // 12. 等待扫描完成
    while (1) {
        int com = ScanCompleted();
        if (com == 0 || com == 3) break;
        Sleep(3000); // 休眠3秒
        printf("complete is %d\n", com);
        printf("total is %d\n", GetTotalScanNo());
        printf("current is %d\n", GetCurrentScanNo());
    }

    return 0;
}

/**
 * 安全关闭DLL
 */
void MRI_Wrapper_Close() {
    if (mri_dll_handle != NULL) {
        if (Abort != NULL) Abort();
        if (CloseSys != NULL) CloseSys();
        FreeLibrary(mri_dll_handle);
        mri_dll_handle = NULL;
        printf("菲特谱仪连接已安全关闭\n");
    }
}

// 测试主函数（可选）
int main() {
    // 示例调用
    const char* dll_path = "D:\\123pan\\Downloads\\testDLL\\bin\\x64\\Debug\\mridll.dll";
    const char* init_ini = "D:\\123pan\\Downloads\\testDLL\\bin\\x64\\Debug\\hw_cfg\\init.ini";
    const char* par_path = "D:\\123pan\\Downloads\\testDLL\\bin\\x64\\Debug\\PTScan.par";
    const char* save_path = "D:\\mri_data";
    const char* prefix = "tmp1";
    const char* save_par_path = "D:\\mri_data\\new.par";

    // 加载DLL
    if (MRI_Wrapper_Init(dll_path) != 0) {
        return -1;
    }

    // 执行扫描
    int ret = MRI_SimpleRun(init_ini, par_path, save_path, prefix, save_par_path);
    printf("扫描执行结果：%d\n", ret);

    // 关闭DLL
    MRI_Wrapper_Close();
    return 0;
}