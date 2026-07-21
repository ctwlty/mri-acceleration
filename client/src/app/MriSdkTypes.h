#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>

class MriRuntimeResolver;

class BaselineIdentityProof {
public:
    BaselineIdentityProof() = default;
    bool isValid() const { return m_token != 0; }
#ifdef MRI_RUNTIME_RESOLVER_TESTING
    static BaselineIdentityProof testOnly()
    {
        return BaselineIdentityProof(1);
    }
#endif
private:
    explicit BaselineIdentityProof(quint64 token) : m_token(token) {}
    quint64 m_token = 0;
    friend class MriRuntimeResolver;
};

enum class ExecutionGate {
    Hold,
    VerifiedBaseline,
    VerifiedScene
};

struct ExecutionContext {
    ExecutionGate gate = ExecutionGate::Hold;
    BaselineIdentityProof identityProof;
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
