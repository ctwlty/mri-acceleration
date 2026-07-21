#pragma once

#include "MriSdkTypes.h"

#include <QString>

class DeviceBridge;

struct MriRuntimeOverrides {
    QString sdkPath;
    QString initPath;
    QString parameterPath;
    QString outputPath;
};

struct MriRuntimePaths {
    QString runtimeDirectory;
    QString sdkPath;
    QString initPath;
    QString parameterPath;
    QString outputPath;
    QString error;
    BaselineIdentityProof identityProof;

    bool isValid() const;
};

#ifdef MRI_RUNTIME_RESOLVER_TESTING
struct MriRuntimeExpectations {
    QString dllSha256;
    QString initSha256;
    QString parameterSha256;
    int hwCfgFileCount = 0;
    qint64 hwCfgTotalBytes = 0;
    QString hwCfgManifestSha256;
};
#endif

class MriRuntimeResolver {
public:
    static MriRuntimePaths resolve(const QString& applicationDir, const MriRuntimeOverrides& overrides);

#ifdef MRI_RUNTIME_RESOLVER_TESTING
    static MriRuntimePaths resolveForTesting(
        const QString& applicationDir,
        const MriRuntimeOverrides& overrides,
        const MriRuntimeExpectations& expectations);
#endif

private:
    static bool verifyBaselineIdentityNow(
        const QString& sdkPath,
        const MriSdkConfig& config,
        const BaselineIdentityProof& proof);
    static bool verifyBaselineIdentityNow(
        const QString& sdkPath,
        const BaselineIdentityProof& proof);
    static bool proofMatchesActualIdentity(const BaselineIdentityProof& proof);
    static BaselineIdentityProof mintIdentityProof(
        const MriRuntimePaths& paths,
        const QString& dllSha256,
        const QString& initSha256,
        const QString& parameterSha256,
        int hwCfgFileCount,
        qint64 hwCfgTotalBytes,
        const QString& hwCfgManifestSha256);
    friend class DeviceBridge;
};
