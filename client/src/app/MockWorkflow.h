#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

enum class DataSourceKind {
    Mock,
    HistoricalRaw,
    LiveBlocked
};

enum class MockWorkflowState {
    Empty,
    Prepared,
    Running,
    Paused,
    Cancelled,
    Processing,
    Reconstructed,
    QcReady,
    Packaged,
    Failed
};

QString dataSourceKindName(DataSourceKind kind);
QString mockWorkflowStateName(MockWorkflowState state);

struct MockActionResult {
    bool ok = false;
    QString error;

    static MockActionResult success();
    static MockActionResult failure(const QString& error);
};

struct MockParameterDraft {
    QString scene;
    QString object;
    QString sampleId;
    QString templateId;
    QString templateName;
    QStringList protocolChain;
    QString orientation;
    QString imagingTarget;
    double fovReadMm = 0.0;
    double fovPhaseMm = 0.0;
    int matrixRead = 0;
    int matrixPhase = 0;
    double trMs = 0.0;
    double teMs = 0.0;
    double sliceThicknessMm = 0.0;
    double sliceGapMm = 0.0;
    int sliceCount = 0;
    int nex = 0;
    bool readPhaseSwapped = false;
    bool planningCoverageModified = false;
    double coverageX = 0.0;
    double coverageY = 0.0;
    double coverageWidth = 0.0;
    double coverageHeight = 0.0;
    double coverageCenterX = 0.0;
    double coverageCenterY = 0.0;
    double slicePosition = 0.0;
    QString outputRoot;

    QStringList validationErrors() const;
    QJsonObject toJson() const;
};

struct MockPreparationEvidence {
    bool preparationConfirmed = false;
    bool protocolConfirmed = false;
    bool localizationConfirmed = false;
    bool outputRootWritable = false;
    QString outputRootError;

    QStringList missingReasons() const;
};

struct MockStartConfirmations {
    bool mockSourceConfirmed = false;
    bool outputConfirmed = false;
    bool noDeviceSideEffectsConfirmed = false;

    bool allConfirmed() const;
    QStringList missingReasons() const;
};

struct MockParameterSnapshot {
    QString snapshotId;
    DataSourceKind dataSource = DataSourceKind::Mock;
    MockParameterDraft parameters;

    QJsonObject toJson() const;
};

struct MockAuditEvent {
    QString name;
    QDateTime occurredAtUtc;
    QString runId;
    MockWorkflowState state = MockWorkflowState::Empty;
    DataSourceKind dataSource = DataSourceKind::Mock;
    QJsonObject details;

    QJsonObject toJson() const;
};

struct MockReconstructionArtifact {
    QString logicalSource;
    QByteArray pngSha256;
    qint64 byteSize = 0;

    bool isValid() const;
    QJsonObject toJson() const;
};

struct MockQcMetrics {
    double snrDb = 0.0;
    double uniformityPercent = 0.0;
    QSize objectSizePixels;
    QByteArray imageSha256;

    bool isValid() const;
    QJsonObject toJson() const;
};

struct MockWorkflowDependencies {
    std::function<QString()> nextRunId;
    std::function<QString()> nextSnapshotId;
    std::function<QDateTime()> nowUtc;
};

class MockWorkflow final {
public:
    explicit MockWorkflow(MockWorkflowDependencies dependencies = {});

    MockActionResult selectDataSource(DataSourceKind kind);
    MockActionResult prepare(const MockParameterDraft& draft,
                             const MockPreparationEvidence& preparation);
    MockActionResult start(const MockStartConfirmations& confirmations);
    MockActionResult setProgress(int percent);
    MockActionResult pause();
    MockActionResult resume();
    MockActionResult cancel();
    MockActionResult fail(const QString& error);
    MockActionResult retryProcessing();
    MockActionResult bindReconstruction(
        const MockReconstructionArtifact& reconstruction);
    MockActionResult recordQcSuccess(const MockQcMetrics& qc);
    MockActionResult recordQcFailure(const QString& error);
    MockActionResult confirmResult();
    MockActionResult markPackaged(const QString& packagePath);
    MockActionResult recordPackageFailure(const QString& error);

    DataSourceKind dataSource() const;
    MockWorkflowState state() const;
    QStringList executionBlockReasons() const;
    QString runId() const;
    QString snapshotId() const;
    const MockParameterSnapshot& snapshot() const;
    int progressPercent() const;
    const QVector<MockAuditEvent>& auditEvents() const;
    bool hasReconstruction() const;
    const std::optional<MockReconstructionArtifact>& reconstruction() const;
    bool hasQc() const;
    const std::optional<MockQcMetrics>& qc() const;
    bool resultConfirmed() const;
    QString packagePath() const;
    QString lastError() const;

private:
    void appendAudit(const QString& name,
                     MockWorkflowState state,
                     const QJsonObject& details = {});
    void clearResultData();

    MockWorkflowDependencies m_dependencies;
    DataSourceKind m_dataSource = DataSourceKind::Mock;
    MockWorkflowState m_state = MockWorkflowState::Empty;
    MockParameterDraft m_draft;
    MockPreparationEvidence m_preparation;
    QString m_runId;
    MockParameterSnapshot m_snapshot;
    int m_progressPercent = 0;
    QVector<MockAuditEvent> m_auditEvents;
    std::optional<MockReconstructionArtifact> m_reconstruction;
    std::optional<MockQcMetrics> m_qc;
    bool m_resultConfirmed = false;
    QString m_packagePath;
    QString m_lastError;
};
