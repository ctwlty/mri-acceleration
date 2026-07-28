#pragma once

#include "DeviceBridge.h"
#include "EggControllerProcess.h"
#include "SceneCatalog.h"

#include <QMainWindow>

class QLabel;
class QComboBox;
class QGridLayout;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QCloseEvent;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    MriSdkResult loadSdkAndConnect(const QString& dllPath, const MriSdkConfig& config);
    MriSdkSessionState deviceSessionState() const;
    void configureEggController(const EggControllerLaunchConfig& config);
    void setMockWorkflowStep(int step);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void handlePrimarySceneChanged();
    void handleTargetChanged();
    void handleTemplateSearchChanged();
    void handleSceneChanged();
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
    void updateControlMode();
    void updatePrecheckStatus(const MriSdkStatus& status);

private:
    QWidget* buildHeader();
    QWidget* buildLeftPane();
    QWidget* buildCenterPane();
    QWidget* buildRightPane();
    QWidget* buildLegacyRightPane();
    QWidget* buildFooter();
    QWidget* makeWorkflowPage(int step);
    QWidget* makeLegacyWorkflowPage(int step);
    QWidget* makeWorkflowRightPage(int step);
    QWidget* makeMetricCard(const QString& name, QLabel*& valueLabel);
    QWidget* makeOperationNode(const QString& step, const QString& title, QLabel*& detailLabel);
    QWidget* makeProtocolTimelineViewport();
    QWidget* makePrecheckViewport();
    QWidget* makeLocalizationViewport();
    QWidget* makeReconstructionViewport();
    QWidget* makeImageViewport(const QString& title, const QString& subtitle, QLabel*& imageView);
    void addInfoRow(QGridLayout* form, QWidget* parent, int row, const QString& labelText, QLabel*& valueLabel);
    void applyScene(const SceneTemplate& scene);
    SceneTemplate currentScene() const;
    QString selectedPrimaryScene() const;
    QString selectedTarget() const;
    void populatePrimaryScenes();
    void populateTargetsForScene();
    void populateTemplatesForSelection();
    void setOperationChain(const SceneTemplate& scene);
    void showEggControllerArtifacts(const EggControllerArtifacts& artifacts);
    void resetRunConfirmations();
    void setWorkflowStep(int step);
    void refreshWorkflow();
    bool isEggControllerMode() const;

    QComboBox* m_primarySceneCombo = nullptr;
    QComboBox* m_targetCombo = nullptr;
    QComboBox* m_controlModeCombo = nullptr;
    QLineEdit* m_templateSearchEdit = nullptr;
    QListWidget* m_sceneList = nullptr;
    QPushButton* m_useSelectedTemplateButton = nullptr;
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

    QLabel* m_operationDetails[4] = {nullptr};
    QLabel* m_chainSummary = nullptr;
    QLabel* m_automationStatusLabel = nullptr;
    QLabel* m_sequenceProtocolSummary = nullptr;
    QLabel* m_sequenceTimingSummary = nullptr;
    QLabel* m_precheckStatusLabel = nullptr;
    QLabel* m_precheckSampleStatus = nullptr;
    QLabel* m_precheckCoilStatus = nullptr;
    QLabel* m_precheckStorageStatus = nullptr;
    QLabel* m_precheckDeviceStatus = nullptr;
    QLabel* m_localizationImageView = nullptr;
    QLabel* m_localizationCoverageLabel = nullptr;
    QLabel* m_reconstructionEvidenceLabel = nullptr;
    QLabel* m_kspaceImageView = nullptr;
    QLabel* m_finalImageView = nullptr;
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
    QPushButton* m_leftMockStartButton = nullptr;
    QPushButton* m_leftMockStopButton = nullptr;
    QStackedWidget* m_workflowPages = nullptr;
    QStackedWidget* m_workflowRightPages = nullptr;
    QLabel* m_workflowStatusLabel = nullptr;
    QLabel* m_workflowCurrentStepLabel = nullptr;
    QLabel* m_workflowBodyLabel = nullptr;
    QLabel* m_protocolChainLabel = nullptr;
    QLabel* m_scanPlanChainLabel = nullptr;
    QLabel* m_workflowOutputSummary = nullptr;
    QPushButton* m_workflowBackButton = nullptr;
    QPushButton* m_workflowNextButton = nullptr;
    QPushButton* m_realRunButton = nullptr;
    QPushButton* m_addComparisonButton = nullptr;
    QPushButton* m_mockAcquireButton = nullptr;
    QTimer* m_mockAcquisitionTimer = nullptr;
    QPushButton* m_openHistoryButton = nullptr;
    QPushButton* m_backToResultsButton = nullptr;
    QWidget* m_localizationPlanner = nullptr;
    QLabel* m_historySelectionSummary = nullptr;
    EggControllerProcess* m_eggController = nullptr;
    DeviceBridge* m_bridge = nullptr;
    EggControllerLaunchConfig m_eggControllerConfig;
    QString m_selectedDllPath;
    bool m_precheckRequested = false;
    int m_workflowStep = 1;
    int m_mockAcquisitionRemainingMs = 3200;
    bool m_comparisonEnabled = false;
    QList<SceneTemplate> m_catalog;
};
