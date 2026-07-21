#pragma once

#include "MriSdkLoader.h"
#include "SceneTemplate.h"

#include <QObject>

class DeviceBridge : public QObject {
    Q_OBJECT

public:
    explicit DeviceBridge(QObject* parent = nullptr);

    bool loadSdk(const QString& dllPath);
    bool initialize(const QString& initPath, const QString& outputPath, const QString& parPath);
    void connectDevice();
    void precheck();
    void dryRunScene(const SceneTemplate& scene);
    void startScan(const SceneTemplate& scene);
    void pauseScan();
    void resumeScan();
    void abortScan();

    QString connectionState() const;
    QString transferState() const;
    QString abnormalState() const;
    QString temperatureState() const;
    QString scanState() const;
    QString scanProgress() const;
    QString sdkModeLabel() const;
    QString sdkPathLabel() const;
    QString lastError() const;

signals:
    void logAppended(const QString& line);
    void badgesChanged(const QString& connection, const QString& transfer, const QString& abnormal);
    void scanStatusChanged(const QString& scanState, const QString& scanProgress);
    void metricsChanged(const QString& snr, const QString& uniformity, const QString& peak, const QString& area);
    void temperatureChanged(const QString& temperature);
    void sdkStatusChanged(const QString& modeLabel, const QString& pathLabel, const QString& errorLabel);
    void sdkDiagnosticChanged(const QString& status, const QString& filePath, const QString& details);

private:
    void syncSdkStatus();
    void applyDemoMetrics(const SceneTemplate& scene);
    void setBadges(const QString& connection, const QString& transfer, const QString& abnormal);
    void setScan(const QString& scanState, const QString& progress);

    MriSdkLoader m_loader;
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
};
