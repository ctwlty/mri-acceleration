#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>

enum class MriSdkSessionState {
    Unloaded,
    Loaded,
    Initializing,
    Ready,
    Scanning,
    Stopping,
    Fault,
    Closed
};

struct MriSdkResult {
    bool ok = false;
    QString stage;
    QString function;
    int code = 0;
    QString message;

    static MriSdkResult success(const QString& stageName)
    {
        MriSdkResult result;
        result.ok = true;
        result.stage = stageName;
        return result;
    }

    static MriSdkResult failure(
        const QString& stageName,
        const QString& functionName,
        int errorCode,
        const QString& errorMessage)
    {
        MriSdkResult result;
        result.stage = stageName;
        result.function = functionName;
        result.code = errorCode;
        result.message = errorMessage;
        return result;
    }
};

struct MriSdkConfig {
    QString initPath;
    QString parameterPath;
    QString outputPath;
    QByteArray outputPrefix = "PTMRIData";
    int systemSelection = 3;
    int pollIntervalMs = 1000;
    int scanTimeoutMs = 30 * 60 * 1000;
};

struct MriSdkStatus {
    int connection = 0;
    double temperature = 0.0;
    int scan = 0;
    int currentScan = 0;
    int totalScans = 0;
};

Q_DECLARE_METATYPE(MriSdkSessionState)
Q_DECLARE_METATYPE(MriSdkResult)
Q_DECLARE_METATYPE(MriSdkStatus)
