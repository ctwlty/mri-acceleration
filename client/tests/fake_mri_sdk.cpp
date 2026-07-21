#include <cstring>
#include <string>

#ifdef _WIN32
#define MRI_EXPORT extern "C" __declspec(dllexport)
#else
#define MRI_EXPORT extern "C"
#endif

namespace {
std::string calls;
std::string failureFunction;
int failureCode = 0;
int scanStatus = 0;

int resultFor(const char* functionName)
{
    if (!calls.empty()) {
        calls += '|';
    }
    calls += functionName;
    return failureFunction == functionName ? failureCode : 0;
}

void record(const char* functionName)
{
    static_cast<void>(resultFor(functionName));
}
}

MRI_EXPORT void FakeReset()
{
    calls.clear();
    failureFunction.clear();
    failureCode = 0;
    scanStatus = 0;
}

MRI_EXPORT void FakeSetFailure(const char* functionName, int code)
{
    failureFunction = functionName ? functionName : "";
    failureCode = code;
}

MRI_EXPORT const char* FakeCalls()
{
    return calls.c_str();
}

MRI_EXPORT void FakeSetScanStatus(int status)
{
    scanStatus = status;
}

MRI_EXPORT int Init(const char*) { return resultFor("Init"); }
MRI_EXPORT int ConfigFile(const char*) { return resultFor("ConfigFile"); }
MRI_EXPORT int SetOutputPath(const char*) { return resultFor("SetOutputPath"); }
MRI_EXPORT int SetParameterFile(const char*, bool) { return resultFor("SetParameterFile"); }
MRI_EXPORT void SetSaveMode(int) { record("SetSaveMode"); }
MRI_EXPORT void SetSystemSel(int) { record("SetSystemSel"); }
MRI_EXPORT int SetChannelValid(const char*) { return resultFor("SetChannelValid"); }
MRI_EXPORT int SetParameter(const char*, double) { return resultFor("SetParameter"); }
MRI_EXPORT int SetTxCenterFre(int, int, int, double) { return resultFor("SetTxCenterFre"); }
MRI_EXPORT void SetChannelValue(int, float) { record("SetChannelValue"); }
MRI_EXPORT int SaveParameterFile(const char*) { return resultFor("SaveParameterFile"); }
MRI_EXPORT int Run() { return resultFor("Run"); }
MRI_EXPORT void Abort() { record("Abort"); }
MRI_EXPORT void CloseSys() { record("CloseSys"); }
MRI_EXPORT int ScanStatus() { record("ScanStatus"); return scanStatus; }
MRI_EXPORT int ScanCompleted() { record("ScanCompleted"); return scanStatus == 3 ? 1 : 0; }
MRI_EXPORT int GetTotalScanNo() { record("GetTotalScanNo"); return 8; }
MRI_EXPORT int GetCurrentScanNo() { record("GetCurrentScanNo"); return 0; }
MRI_EXPORT double GetTemperature() { record("GetTemperature"); return 31.4; }
MRI_EXPORT int GetConnectStatus(int) { record("GetConnectStatus"); return 1; }
