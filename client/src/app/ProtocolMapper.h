#pragma once

#include "SceneTemplate.h"

#include <QString>
#include <QStringList>
#include <QVector>

class ProtocolMapper {
public:
    struct FieldMapping {
        QString protocolId;
        QString presetName;
        QString sdkField;
        QString presetValue;
        QString status;
        QString note;
    };

    struct DryRunResult {
        QString status;
        QString filePath;
        QString summary;
        QStringList diagnostics;
        QVector<FieldMapping> fields;
        bool ok = false;
    };

    static QVector<FieldMapping> mapScene(const SceneTemplate& scene);
    static DryRunResult generateDryRun(const SceneTemplate& scene, const QString& outputDir);
    static QString diagnosticsText(const DryRunResult& result);

private:
    static QStringList protocolIds(const SceneTemplate& scene);
    static QVector<FieldMapping> whitelistForProtocol(const QString& protocolId);
    static QString safeFileStem(const QString& value);
};
