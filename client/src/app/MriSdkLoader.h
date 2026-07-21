#pragma once

#include "MriSdkTypes.h"
#include "SceneTemplate.h"

#include <QString>
#include <QVector>

class MriSdkLoader {
public:
    struct FrameData {
        QVector<double> samples;
        QString kind;
    };

    enum class Mode {
        Demo,
        Real
    };

    MriSdkLoader();
    ~MriSdkLoader();

    MriSdkResult load(const QString& dllPath);
    void unload();
    bool isLoaded() const;
    MriSdkSessionState sessionState() const;
    Mode mode() const;
    QString lastError() const;
    QString dllPath() const;

    bool initialize(const QString& initPath, const QString& outputPath, const QString& parPath, bool saveMode);
    void shutdown();
    int connectStatus(int boxType = 0) const;
    double temperature() const;
    int scanStatus() const;
    int scanCompleted() const;
    int totalScanNo() const;
    int currentScanNo() const;
    int prepareForScene(const SceneTemplate& scene);
    int run();
    void abort();

    FrameData fetchDemoFrame(const SceneTemplate& scene) const;
    QString defaultInitPath() const;
    QString defaultOutputPath() const;
    QString defaultParPath() const;

private:
    using InitFunc = int (*)(const char*);
    using ConfigFileFunc = int (*)(const char*);
    using SetOutputPathFunc = int (*)(const char*);
    using SetParameterFileFunc = int (*)(const char*, bool);
    using SetSaveModeFunc = void (*)(int);
    using SetSystemSelFunc = void (*)(int);
    using SetChannelValidFunc = int (*)(const char*);
    using SetParameterFunc = int (*)(const char*, double);
    using SetTxCenterFreFunc = int (*)(int, int, int, double);
    using SetChannelValueFunc = void (*)(int, float);
    using SaveParameterFileFunc = int (*)(const char*);
    using RunFunc = int (*)();
    using AbortFunc = void (*)();
    using CloseSysFunc = void (*)();
    using ScanStatusFunc = int (*)();
    using ScanCompletedFunc = int (*)();
    using GetTotalScanNoFunc = int (*)();
    using GetCurrentScanNoFunc = int (*)();
    using GetTemperatureFunc = double (*)();
    using GetConnectStatusFunc = int (*)(int);

    template <typename T>
    bool bind(const char* name, T& target);
    bool bindAll();
    void setError(const QString& error);
    void applyDemoState();

    void* m_handle = nullptr;
    QString m_dllPath;
    QString m_error;
    Mode m_mode = Mode::Demo;
    MriSdkSessionState m_sessionState = MriSdkSessionState::Unloaded;

    InitFunc m_init = nullptr;
    ConfigFileFunc m_configFile = nullptr;
    SetOutputPathFunc m_setOutputPath = nullptr;
    SetParameterFileFunc m_setParameterFile = nullptr;
    SetSaveModeFunc m_setSaveMode = nullptr;
    SetSystemSelFunc m_setSystemSel = nullptr;
    SetChannelValidFunc m_setChannelValid = nullptr;
    SetParameterFunc m_setParameter = nullptr;
    SetTxCenterFreFunc m_setTxCenterFre = nullptr;
    SetChannelValueFunc m_setChannelValue = nullptr;
    SaveParameterFileFunc m_saveParameterFile = nullptr;
    RunFunc m_run = nullptr;
    AbortFunc m_abort = nullptr;
    CloseSysFunc m_closeSys = nullptr;
    ScanStatusFunc m_scanStatus = nullptr;
    ScanCompletedFunc m_scanCompleted = nullptr;
    GetTotalScanNoFunc m_getTotalScanNo = nullptr;
    GetCurrentScanNoFunc m_getCurrentScanNo = nullptr;
    GetTemperatureFunc m_getTemperature = nullptr;
    GetConnectStatusFunc m_getConnectStatus = nullptr;

    int m_demoScanStatus = 0;
    int m_demoScanCompleted = 0;
    int m_demoTotalScanNo = 8;
    int m_demoCurrentScanNo = 0;
};
