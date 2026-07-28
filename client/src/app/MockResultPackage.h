#pragma once

#include "MockWorkflow.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

enum class PackageIntegrity {
    Valid,
    Warning,
    Error
};

struct PackageArtifactInfo {
    QString relativePath;
    qint64 byteSize = 0;
    QByteArray sha256;

    QJsonObject toJson() const;
};

struct MockPackageInput {
    QString rootDirectory;
    QString softwareCommit;
    QString runId;
    MockParameterSnapshot snapshot;
    MockReconstructionArtifact reconstruction;
    QByteArray standardResultPng;
    MockQcMetrics qc;
    QVector<MockAuditEvent> auditEvents;
    QString taskNote;
    QDateTime createdAtUtc;
};

struct PackageWriteResult {
    bool ok = false;
    QString error;
    QString packageDirectory;
    QVector<PackageArtifactInfo> artifacts;
};

struct PackageVerification {
    PackageIntegrity integrity = PackageIntegrity::Error;
    QString packageDirectory;
    QJsonObject manifest;
    QStringList issues;
    QVector<PackageArtifactInfo> artifacts;
};

struct MockHistoryRecord {
    QString runId;
    QString snapshotId;
    QString sampleId;
    QString templateId;
    QString templateName;
    QString createdAtUtc;
    QString packageDirectory;
    QString previewImagePath;
    PackageIntegrity integrity = PackageIntegrity::Error;
    QStringList issues;
    QJsonObject manifest;
};

struct HistoryLoadResult {
    bool ok = false;
    QString error;
    QVector<MockHistoryRecord> records;
};

class MockResultPackage final {
public:
    static QString defaultRoot();
    static PackageWriteResult write(const MockPackageInput& input);
    static PackageVerification verify(const QString& packageDirectory);
    static HistoryLoadResult loadHistory(const QString& rootDirectory);
};
