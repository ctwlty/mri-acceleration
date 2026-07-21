#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <utility>

class MriRuntimeResolver;

class BaselineIdentityProof {
public:
    BaselineIdentityProof() = default;
    bool isValid() const { return m_token != 0; }
private:
    BaselineIdentityProof(
        QString sdkPath,
        QString initPath,
        QString hwCfgPath,
        QString parameterPath,
        QString dllSha256,
        QString initSha256,
        QString parameterSha256,
        int hwCfgFileCount,
        qint64 hwCfgTotalBytes,
        QString hwCfgManifestSha256)
        : m_token(1)
        , m_sdkPath(std::move(sdkPath))
        , m_initPath(std::move(initPath))
        , m_hwCfgPath(std::move(hwCfgPath))
        , m_parameterPath(std::move(parameterPath))
        , m_dllSha256(std::move(dllSha256))
        , m_initSha256(std::move(initSha256))
        , m_parameterSha256(std::move(parameterSha256))
        , m_hwCfgFileCount(hwCfgFileCount)
        , m_hwCfgTotalBytes(hwCfgTotalBytes)
        , m_hwCfgManifestSha256(std::move(hwCfgManifestSha256))
    {}
    quint64 m_token = 0;
    QString m_sdkPath;
    QString m_initPath;
    QString m_hwCfgPath;
    QString m_parameterPath;
    QString m_dllSha256;
    QString m_initSha256;
    QString m_parameterSha256;
    int m_hwCfgFileCount = 0;
    qint64 m_hwCfgTotalBytes = 0;
    QString m_hwCfgManifestSha256;
    friend class MriRuntimeResolver;
};

enum class ExecutionGate {
    Hold,
    VerifiedBaseline,
    VerifiedScene
};

enum class ExecutionSelectionKind {
    ScientificScene,
    VerifiedBaseline
};

struct ExecutionSelection {
    ExecutionSelectionKind kind = ExecutionSelectionKind::ScientificScene;
    QString id;
    ExecutionGate gate = ExecutionGate::Hold;
};

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
    int stopTimeoutMs = 30 * 1000;
    int rawSettleTimeoutMs = 10 * 1000;
    BaselineIdentityProof identityProof;
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
