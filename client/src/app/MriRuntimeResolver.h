#pragma once

#include <QString>

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

    bool isValid() const;
};

class MriRuntimeResolver {
public:
    static MriRuntimePaths resolve(const QString& applicationDir, const MriRuntimeOverrides& overrides);
};
