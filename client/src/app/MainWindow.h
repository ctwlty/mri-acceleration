#pragma once

#include "DeviceBridge.h"
#include "SceneCatalog.h"

#include <QMainWindow>

class QLabel;
class QComboBox;
class QGridLayout;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    explicit MainWindow(const QString& bundledRuntimeDirectory, QWidget* parent = nullptr);
    MriSdkResult loadSdkAndConnect(const QString& dllPath, const MriSdkConfig& config);
    MriSdkSessionState deviceSessionState() const;

private slots:
    void handlePrimarySceneChanged();
    void handleTargetChanged();
    void handleTemplateSearchChanged();
    void handleSceneChanged();
    void handleExecutionGateChanged();
    void handleConnect();
    void handleLoadSdk();
    void handlePrecheck();
    void handleDryRun();
    void handleStart();
    void handlePause();
    void handleResume();
    void handleAbort();
    void appendLog(const QString& line);
    void updateBadges(const QString& connection, const QString& transfer, const QString& abnormal);
    void updateScan(const QString& scanState, const QString& scanProgress);
    void updateMetrics(const QString& snr, const QString& uniformity, const QString& peak, const QString& area);
    void updateTemperature(const QString& temperature);
    void updateSdkStatus(const QString& modeLabel, const QString& pathLabel, const QString& errorLabel);
    void updateSdkDiagnostic(const QString& status, const QString& filePath, const QString& details);
    void updateSessionState(MriSdkSessionState state);

private:
    QWidget* buildHeader();
    QWidget* buildLeftPane();
    QWidget* buildCenterPane();
    QWidget* buildRightPane();
    QWidget* buildFooter();
    QWidget* makeMetricCard(const QString& name, QLabel*& valueLabel);
    QWidget* makeOperationNode(const QString& step, const QString& title, QLabel*& detailLabel);
    QWidget* makeDarkViewport(const QString& title, const QString& subtitle);
    void addInfoRow(QGridLayout* form, QWidget* parent, int row, const QString& labelText, QLabel*& valueLabel);
    void applyScene(const SceneTemplate& scene);
    SceneTemplate currentScene() const;
    QString selectedPrimaryScene() const;
    QString selectedTarget() const;
    void populatePrimaryScenes();
    void populateTargetsForScene();
    void populateTemplatesForSelection();
    void setOperationChain(const SceneTemplate& scene);

    QComboBox* m_primarySceneCombo = nullptr;
    QComboBox* m_executionGateCombo = nullptr;
    QComboBox* m_targetCombo = nullptr;
    QLineEdit* m_templateSearchEdit = nullptr;
    QListWidget* m_sceneList = nullptr;
    QLabel* m_sceneTitle = nullptr;
    QLabel* m_sceneTarget = nullptr;
    QLabel* m_sceneSequence = nullptr;
    QLabel* m_sceneStepA = nullptr;
    QLabel* m_sceneStepB = nullptr;
    QLabel* m_sceneProcessing = nullptr;
    QLabel* m_sceneAnalysis = nullptr;
    QLabel* m_sceneNote = nullptr;

    QLabel* m_connectionBadge = nullptr;
    QLabel* m_transferBadge = nullptr;
    QLabel* m_abnormalBadge = nullptr;
    QLabel* m_scanStateBadge = nullptr;
    QLabel* m_scanProgressBadge = nullptr;
    QLabel* m_temperatureBadge = nullptr;

    QLabel* m_snrValue = nullptr;
    QLabel* m_uniformityValue = nullptr;
    QLabel* m_peakValue = nullptr;
    QLabel* m_areaValue = nullptr;

    QLabel* m_headerSceneValue = nullptr;
    QLabel* m_headerSdkValue = nullptr;
    QLabel* m_footerConnectionValue = nullptr;
    QLabel* m_footerScanValue = nullptr;
    QLabel* m_footerTemperatureValue = nullptr;
    QLabel* m_footerAbnormalValue = nullptr;
    QLabel* m_footerSdkValue = nullptr;

    QLabel* m_operationDetails[6] = {nullptr};
    QLabel* m_chainSummary = nullptr;
    QLabel* m_presetVersionValue = nullptr;
    QLabel* m_parameterStatusValue = nullptr;
    QLabel* m_runGateValue = nullptr;
    QLabel* m_sdkMappingValue = nullptr;
    QLabel* m_physicsCheckValue = nullptr;
    QLabel* m_handoffValue = nullptr;
    QPlainTextEdit* m_parameterDetailsView = nullptr;
    QPlainTextEdit* m_sdkDiagnosticView = nullptr;
    QPlainTextEdit* m_logView = nullptr;
    QPushButton* m_loadSdkButton = nullptr;
    QPushButton* m_connectButton = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_pauseButton = nullptr;
    QPushButton* m_abortButton = nullptr;
    DeviceBridge* m_bridge = nullptr;
    QString m_bundledRuntimeDirectory;
    QString m_selectedDllPath;
    QList<SceneTemplate> m_catalog;
};
