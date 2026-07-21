#include <iostream>
#include <Windows.h>

using namespace std;

// const char* DLL_PTH = "C:\\Program Files\\SpectrometerIDE\\NMRDLL.dll";
// const char* DLL_PTH = "bin\\x64\\Debug\\NMRDLL.dll";
const char* DLL_PTH = "bin\\x64\\Debug\\mridll.dll";
float SHIM_VALUE = 1000;
const char* PAR_PATH = "bin\\x64\\Debug\\PTScan.par";
static char* OUTPUT_PATH = "D:\\mri_data";
static char* OUTPUT_PREFIX = "mir_";
static char* CHANNEL_SELECT = "255";
const char* INITFILE = "bin\\x64\\Debug\\hw_cfg\\init.ini";
const char* SAVE_PAR = "D:\\mri_data\\new.par";


typedef int (*InitPtr)(const char *);
typedef int (*ConfigFilePtr)(const char *);
typedef void (*SetAverageModePtr)(int);
typedef void (*SetSaveModePtr)(int);
typedef int (*SetParameterFilePtr)(const char *, bool);
typedef int (*SetOutputPrefixPtr)(char *);
typedef int (*SetChannelValidPtr)(char *);
typedef int (*SetParameterPtr)(char *, double);
typedef int (*SetTxCenterFrePtr)(int, int, int, double);
typedef int (*SaveParameterFilePtr)(const char *);
typedef void (*SetChannelValuePtr)(int, float);
typedef int (*RunPtr)();
typedef int (*ScanCompletedPtr)();
typedef void (*AbortPtr)();
typedef void (*CloseSysPtr)();
typedef int (*SetOutputPathPtr)(char *);
typedef int (*GetTotalScanNoPtr)();
typedef int (*GetCurrentScanNoPtr)();

class dllFunc {
    public:
    InitPtr Init = nullptr;
    ConfigFilePtr ConfigFile = nullptr;
    SetAverageModePtr SetAverageMode = nullptr;
    SetSaveModePtr SetSaveMode = nullptr;
    SetParameterPtr SetParameter = nullptr;
    SetOutputPrefixPtr SetOutputPrefix = nullptr;
    SetChannelValidPtr SetChannelValid = nullptr;
    SetParameterFilePtr SetParameterFile = nullptr;
    SetTxCenterFrePtr SetTxCenterFre = nullptr;
    SaveParameterFilePtr SaveParameterFile = nullptr;
    SetChannelValuePtr SetChannelValue = nullptr;
    RunPtr Run = nullptr;
    ScanCompletedPtr ScanCompleted = nullptr;
    AbortPtr Abort = nullptr;
    CloseSysPtr CloseSys = nullptr;
    SetOutputPathPtr SetOutputPath = nullptr;
    GetTotalScanNoPtr GetTotalScanNo = nullptr;
    GetCurrentScanNoPtr GetCurrentScanNo = nullptr;

};

const char *FUNC_NAME[] = {"Init", "ConfigFile", "SetAverageMode", 
        "SetSaveMode", "SetParameterFile", "SetOutputPrefix", 
        "SetChannelValid", "SetParameter", "SetTxCenterFre",
        "SaveParameterFile", "SetChannelValue", "Run", "Abort",
        "ScanCompleted", "CloseSys", "SetOutputPath", 
        "GetTotalScanNo", "GetCurrentScanNo"};

int tieFunc(HMODULE dll, dllFunc& df) {
    FARPROC addrs[size(FUNC_NAME)];
    for(int i = 0; i < size(FUNC_NAME); ++i) {
        FARPROC addr = GetProcAddress(dll, FUNC_NAME[i]);
        if (addr == NULL) {
            cout << "get func address failed on " << i << " error: " <<
                GetLastError() << endl;
                return 1;
        }
        addrs[i] = addr;
    }
    df.Init = reinterpret_cast<InitPtr>(addrs[0]);
    df.ConfigFile = reinterpret_cast<ConfigFilePtr>(addrs[1]);
    df.SetAverageMode = reinterpret_cast<SetAverageModePtr>(addrs[2]);
    df.SetSaveMode = reinterpret_cast<SetSaveModePtr>(addrs[3]);
    df.SetParameter = reinterpret_cast<SetParameterPtr>(addrs[4]);
    df.SetOutputPrefix = reinterpret_cast<SetOutputPrefixPtr>(addrs[5]);
    df.SetChannelValid = reinterpret_cast<SetChannelValidPtr>(addrs[6]);
    df.SetParameterFile = reinterpret_cast<SetParameterFilePtr>(addrs[7]);
    df.SetTxCenterFre = reinterpret_cast<SetTxCenterFrePtr>(addrs[8]);
    df.SaveParameterFile = reinterpret_cast<SaveParameterFilePtr>(addrs[9]);
    df.SetChannelValue = reinterpret_cast<SetChannelValuePtr>(addrs[10]);
    df.Run = reinterpret_cast<RunPtr>(addrs[11]);
    df.ScanCompleted = reinterpret_cast<ScanCompletedPtr>(addrs[12]);
    df.Abort = reinterpret_cast<AbortPtr>(addrs[13]);
    df.CloseSys = reinterpret_cast<CloseSysPtr>(addrs[14]);
    df.SetOutputPath = reinterpret_cast<SetOutputPathPtr>(addrs[15]);
    df.GetTotalScanNo = reinterpret_cast<GetTotalScanNoPtr>(addrs[16]);
    df.GetCurrentScanNo = reinterpret_cast<GetCurrentScanNoPtr>(addrs[17]);
    return 0;
}

int getFuncFromDll(dllFunc& df) {
    HMODULE dll = LoadLibraryA(DLL_PTH);
    if (dll == NULL) {
        cout << "loading dll failed, the error code:" << 
            GetLastError() << endl;
        return 1;
    }
    tieFunc(dll, df);
    return 0;
}

int main(){
    cout << "starting" << endl;
    dllFunc df;
    getFuncFromDll(df);
    if (df.Init == nullptr) {
        cout << "get func failed" << endl;
        return 0;
    }
    cout << "get func achieve" << endl;

    int ret = df.Init(INITFILE);
    ret |= df.ConfigFile(INITFILE);
    if (ret != 0) {
        cout << "there is an error occured when initialize the mri" <<
            "the error is "<< ret << " " << GetLastError() << endl;
        return 1;
    }
    cout << "init over" << endl;
    df.SetAverageMode(0);
    df.SetSaveMode(1);

    ret = df.SetParameterFile(PAR_PATH, false);
    cout << "the ret in par pth set is " << ret << endl;
    ret = df.SetOutputPath(OUTPUT_PATH);
    cout << "the ret in out put pth is " << ret << endl;
    ret = df.SetOutputPrefix(OUTPUT_PREFIX);
    cout << "the ret in out put prefix is " << ret << endl;
    ret = df.SetChannelValid(CHANNEL_SELECT);
    cout << "the ret in channel valid is " << ret << endl;
    ret = df.SetParameter("viewBlock", 1);
    cout << "the ret in view block is " << ret << endl;
    ret = df.SetParameter("TR", 500);
    cout << "the ret TR is " << ret << endl;
    for (int i = 0; i < 8; ++i) {
        ret |= df.SetTxCenterFre(0, 4, i, 50);
    }
    cout << "the ret TxCenterFre is " << ret << endl;
    ret = df.SaveParameterFile(SAVE_PAR);
    cout << "the ret save par is " << ret << endl;
    df.SetChannelValue(0, SHIM_VALUE);
    df.SetChannelValue(1, SHIM_VALUE);
    df.SetChannelValue(2, SHIM_VALUE);
    if (ret != 0) {
        cout << "there has some errors in the setttings: " << 
            ret << " " << GetLastError() << endl;
        return 2;
    }

    ret = df.Run();
    if (ret != 0) {
        cout << "error occured when running " << ret << " " << 
            GetLastError() << endl;
        return 3;
    }

    while((df.ScanCompleted() != 0) && (df.ScanCompleted() != 3)) {
        Sleep(100);
        cout << "ScanCompleted is " << df.ScanCompleted() << endl;
        cout << "TotalScanNo is " << df.GetTotalScanNo() << endl;
        cout << "GetCurrentScanNo" << df.GetCurrentScanNo() << endl;
    }
    
    df.Abort();
    df.CloseSys();

    return 0;
}