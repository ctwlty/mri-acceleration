#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
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
int connectionStatus = 0;
int rawMode = 1;
std::string outputPath;
std::string initPath;
std::string parameterPath;

void appendCall(const std::string& call)
{
    if (!calls.empty()) {
        calls += '|';
    }
    calls += call;

    const char* callLog = std::getenv("FAKE_CALL_LOG");
    if (callLog && *callLog) {
        std::ofstream log(callLog, std::ios::app);
        log << call << '\n';
    }
}

int resultFor(const char* functionName, const std::string& call = {})
{
    appendCall(call.empty() ? functionName : call);
    return failureFunction == functionName ? failureCode : 0;
}

void record(const char* functionName)
{
    static_cast<void>(resultFor(functionName));
}

void writeRawFile()
{
    if (rawMode == 0 || outputPath.empty()) {
        return;
    }
    const std::string separator = outputPath.back() == '/' || outputPath.back() == '\\' ? "" : "/";
    std::ofstream raw(outputPath + separator + "PTMRIData_fake.raw", std::ios::binary);
    if (rawMode == 1) {
        raw << "MRI_RAW_TEST_DATA";
    }
}
}

MRI_EXPORT void FakeReset()
{
    calls.clear();
    failureFunction.clear();
    failureCode = 0;
    scanStatus = 0;
    connectionStatus = 0;
    rawMode = 1;
    outputPath.clear();
    initPath.clear();
    parameterPath.clear();
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

MRI_EXPORT const char* FakeInitPath() { return initPath.c_str(); }
MRI_EXPORT const char* FakeOutputPath() { return outputPath.c_str(); }
MRI_EXPORT const char* FakeParameterPath() { return parameterPath.c_str(); }

MRI_EXPORT void FakeSetScanStatus(int status)
{
    scanStatus = status;
}
MRI_EXPORT void FakeSetConnectionStatus(int status)
{
    connectionStatus = status;
}

MRI_EXPORT void FakeSetRawMode(int mode)
{
    rawMode = mode;
}

MRI_EXPORT void FakeWriteRaw()
{
    const int previousMode = rawMode;
    rawMode = 1;
    writeRawFile();
    rawMode = previousMode;
}

MRI_EXPORT int Init(const char* value)
{
    initPath = value ? value : "";
    return resultFor("Init");
}
MRI_EXPORT int ConfigFile(const char*) { return resultFor("ConfigFile"); }
MRI_EXPORT int SetOutputPath(const char* value)
{
    outputPath = value ? value : "";
    return resultFor("SetOutputPath");
}
MRI_EXPORT int SetParameterFile(const char* value, bool)
{
    parameterPath = value ? value : "";
    return resultFor("SetParameterFile");
}
MRI_EXPORT void SetSaveMode(int value) { appendCall("SetSaveMode:" + std::to_string(value)); }
MRI_EXPORT void SetSystemSel(int value) { appendCall("SetSystemSel:" + std::to_string(value)); }
MRI_EXPORT int SetChannelValid(const char* value) { return resultFor("SetChannelValid", "SetChannelValid:" + std::string(value ? value : "")); }
MRI_EXPORT int SetParameter(const char*, double) { return resultFor("SetParameter"); }
MRI_EXPORT int SetTxCenterFre(int, int, int, double) { return resultFor("SetTxCenterFre"); }
MRI_EXPORT void SetChannelValue(int, float) { record("SetChannelValue"); }
MRI_EXPORT int SaveParameterFile(const char*) { return resultFor("SaveParameterFile"); }
MRI_EXPORT int SetOutputPrefix(const char* value) { return resultFor("SetOutputPrefix", "SetOutputPrefix:" + std::string(value ? value : "")); }
MRI_EXPORT int SetAllPreempValue() { return resultFor("SetAllPreempValue"); }
MRI_EXPORT int SetAllGraAnalogDelay() { return resultFor("SetAllGraAnalogDelay"); }
MRI_EXPORT int SetSingleGraGmax(int axis, float value)
{
    std::ostringstream call;
    call << "SetSingleGraGmax:" << axis << ':' << value;
    return resultFor("SetSingleGraGmax", call.str());
}
MRI_EXPORT int SetPreempCross(int value) { return resultFor("SetPreempCross", "SetPreempCross:" + std::to_string(value)); }
MRI_EXPORT int SetPreempValue(int axis, int index, float value)
{
    std::ostringstream call;
    call << "SetPreempValue:" << axis << ':' << index << ':' << value;
    return resultFor("SetPreempValue", call.str());
}
MRI_EXPORT int Run()
{
    const int result = resultFor("Run");
    if (result != 0) {
        return result;
    }
    const char* autoComplete = std::getenv("FAKE_AUTO_COMPLETE");
    scanStatus = autoComplete && std::string(autoComplete) == "1" ? 3 : 1;
    writeRawFile();
    return 0;
}
MRI_EXPORT void Abort() { record("Abort"); }
MRI_EXPORT void CloseSys() { record("CloseSys"); }
MRI_EXPORT int ScanStatus() { record("ScanStatus"); return scanStatus; }
MRI_EXPORT int ScanCompleted() { record("ScanCompleted"); return scanStatus == 3 ? 1 : 0; }
MRI_EXPORT int GetTotalScanNo() { record("GetTotalScanNo"); return 8; }
MRI_EXPORT int GetCurrentScanNo() { record("GetCurrentScanNo"); return 0; }
MRI_EXPORT double GetTemperature() { record("GetTemperature"); return 31.4; }
MRI_EXPORT int GetConnectStatus(int) { record("GetConnectStatus"); return connectionStatus; }
