#pragma once

#include "MriSdkLoader.h"
#include "MriSdkTypes.h"
#include "SceneTemplate.h"

#include <QElapsedTimer>
#include <QObject>
#include <QHash>
#include <QTimer>

struct PrecheckResult {
    bool passed = false;
    int connection = 0;
    double temperature = 0.0;
    int scanStatus = 0;
    QString message;
};

class PrecheckTicket {
public:
    bool isValid() const { return m_id != 0; }
private:
    quint64 m_id = 0;
    quint64 m_generation = 0;
    quint64 m_issuerId = 0;
    ExecutionSelectionKind m_selectionKind = ExecutionSelectionKind::ScientificScene;
    QString m_selectionId;
    ExecutionGate m_gate = ExecutionGate::Hold;
    BaselineIdentityProof m_identityProof;
    friend class DeviceBridge;
};

class DeviceBridge : public QObject {
    Q_OBJECT

public:
    explicit DeviceBridge(QObject* parent = nullptr);
    ~DeviceBridge() override;

    MriSdkResult loadSdk(const QString& dllPath, const BaselineIdentityProof& identityProof = {});
    bool initialize(const QString& initPath, const QString& outputPath, const QString& parPath);
    MriSdkResult connectDevice(const MriSdkConfig& config);
    void connectDevice();
    PrecheckResult precheck();
    void dryRunScene(const SceneTemplate& scene);
    MriSdkResult startScan(const PrecheckTicket& ticket);
    void selectVerifiedBaseline();
    void selectScientificScene(const SceneTemplate& scene);
    void pauseScan();
    void resumeScan();
    void abortScan();

    MriSdkSessionState sessionState() const;
    MriSdkResult lastErrorResult() const;
    QString lastRawFile() const;
    QString connectionState() const;
    QString transferState() const;
    QString abnormalState() const;
    QString temperatureState() const;
    QString scanState() const;
    QString scanProgress() const;
    QString sdkModeLabel() const;
    QString sdkPathLabel() const;
    QString lastError() const;
    PrecheckTicket precheckTicket() const;
    PrecheckResult precheckResult() const;
    ExecutionGate executionGate() const;

public slots:
    void refreshStatus();

signals:
    void logAppended(const QString& line);
    void badgesChanged(const QString& connection, const QString& transfer, const QString& abnormal);
    void scanStatusChanged(const QString& scanState, const QString& scanProgress);
    void metricsChanged(const QString& snr, const QString& uniformity, const QString& peak, const QString& area);
    void temperatureChanged(const QString& temperature);
    void sdkStatusChanged(const QString& modeLabel, const QString& pathLabel, const QString& errorLabel);
    void sdkDiagnosticChanged(const QString& status, const QString& filePath, const QString& details);
    void sessionStateChanged(MriSdkSessionState state);
    void operationFailed(const MriSdkResult& result);
    void rawFileReady(const QString& filePath);
    void deviceStatusChanged(const MriSdkStatus& status);
    void executionAuthorizationChanged();

private:
    void syncSdkStatus();
    void applyDemoMetrics(const SceneTemplate& scene);
    void setBadges(const QString& connection, const QString& transfer, const QString& abnormal);
    void setScan(const QString& scanState, const QString& progress);
    void setSessionState(MriSdkSessionState state);
    MriSdkResult reject(const QString& stage, const QString& function, const QString& message);
    MriSdkResult fail(const QString& stage, const QString& function, int code, const QString& message);
    QHash<QString, QString> rawFilesInOutput() const;
    QString findNewRawFile() const;
    bool outputPathIsWritable() const;
    void invalidatePrecheck(const QString& reason);

    MriSdkLoader m_loader;
    MriSdkConfig m_config;
    QTimer m_pollTimer;
    QElapsedTimer m_scanElapsed;
    QElapsedTimer m_stopElapsed;
    QElapsedTimer m_rawSettleElapsed;
    QHash<QString, QString> m_rawFilesBeforeScan;
    bool m_sawActiveScan = false;
    bool m_scanCompletionObserved = false;
    MriSdkSessionState m_state = MriSdkSessionState::Unloaded;
    MriSdkResult m_lastErrorResult;
    QString m_lastRawFile;
    QString m_connectionState;
    QString m_transferState;
    QString m_abnormalState;
    QString m_temperatureState;
    QString m_scanState;
    QString m_scanProgress;
    QString m_sdkModeLabel;
    QString m_sdkPathLabel;
    QString m_lastError;
    QString m_lastDryRunStatus;
    QString m_lastDryRunPath;
    BaselineIdentityProof m_identityProof;
    ExecutionSelection m_executionSelection;
    PrecheckResult m_precheckResult;
    PrecheckTicket m_precheckTicket;
    quint64 m_generation = 1;
    quint64 m_nextTicketId = 1;
    quint64 m_instanceId = 0;
};
