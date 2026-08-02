#include "app/MainWindow.h"
#include "app/EggControllerProcess.h"
#include "app/SceneCatalog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QPixmap>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QStackedWidget>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

namespace {
void writeFile(const QString& path)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("test") > 0);
}

class ScopedEnvironment final {
public:
    ScopedEnvironment(const QByteArray& name, const QByteArray& value)
        : m_name(name), m_previous(qgetenv(name)), m_existed(qEnvironmentVariableIsSet(name))
    {
        qputenv(m_name, value);
    }

    ~ScopedEnvironment()
    {
        if (m_existed)
            qputenv(m_name, m_previous);
        else
            qunsetenv(m_name);
    }

private:
    QByteArray m_name;
    QByteArray m_previous;
    bool m_existed = false;
};

bool advanceToMockRunConfirmation(MainWindow& window)
{
    window.setMockWorkflowStep(4);
    auto* preparation =
        window.findChild<QPushButton*>(QStringLiteral("SavePreparationButton"));
    if (!preparation || !preparation->isEnabled()) return false;
    preparation->click();

    auto* useOnce =
        window.findChild<QPushButton*>(QStringLiteral("ProtocolUseOnceButton"));
    auto* continueProtocol =
        window.findChild<QPushButton*>(QStringLiteral("ContinueProtocolButton"));
    if (!useOnce || !continueProtocol || !useOnce->isEnabled()) return false;
    useOnce->click();
    if (!continueProtocol->isEnabled()) return false;
    continueProtocol->click();

    auto* openPlanning =
        window.findChild<QPushButton*>(QStringLiteral("OpenLocalizationPlanningButton"));
    if (!openPlanning || !openPlanning->isEnabled()) return false;
    openPlanning->click();

    auto* confirmLocalization =
        window.findChild<QPushButton*>(QStringLiteral("ConfirmLocalizationButton"));
    if (!confirmLocalization || !confirmLocalization->isEnabled()) return false;
    confirmLocalization->click();
    auto* currentStep =
        window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    return currentStep && currentStep->text() == QStringLiteral("08");
}
}

class MainWindowTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void sdkCanBeLoadedAndConnectedWithoutFileDialog();
    void defaultSdkStateIsUnloadedAndRealButtonsDisabled();
    void configureEggControllerKeepsProxyUnreachableAndReportsHold();
    void holdHandlersLeaveMockNavigationAndStatusVisible();
    void galleryLayoutUsesStableThreeColumnsAndContextPanels();
    void workflowUsesMockNavigationAndKeepsRealRunOnHold();
    void preparationGuidanceRemainsACompactSingleLineNotice();
    void mockAcquisitionShowsRunningInsteadOfStartState();
    void runConfirmationSnapshotKeepsChecksGrouped();
    void operationalRightStatusUsesReadableRows();
    void enabledWorkflowActionsExposeVisibleFeedback();
    void realActionsExplainWhyTheyAreUnavailable();
    void mockPresentationAssetsDoNotContainEmbeddedDynamicStatus();
    void workflowUsesWaterPhantomTransverseSemantics();
    void visibleWorkflowControlsHaveStableObjectNames();
    void selectedTemplateOffersVisibleContinuationFromAnyWorkflowPage();
    void persistentWorkflowNavigationIsVisibleAtCommonWindowSize();
    void mockAcquisitionRequiresAllRunConfirmations();
    void mockPauseStopsAndResumesAutomaticProgression();
    void runConfirmationsResetAfterLeavingConfirmationStep();
    void backFromProcessingReturnsToRunConfirmation();
    void unsupportedCatalogTemplateIsClearlyBrowseOnly();
    void templateRestartIsUnavailableDuringMockAcquisition();
    void globalNextDelegatesToCanonicalPageActionAndRespectsItsGate();
    void preparationContinuationRecordsOnlyAnInMemoryMockDraft();
    void protocolL2ValidationGatesContinuationAndComputesSummary();
    void unsupportedProtocolVersionPersistenceIsDisabledWithReason();
    void locReferenceClearlyRoutesToInteractivePlanning();
    void localizationActionsExposeFeedbackAndRespectAxialGate();
    void localizationSliceControlsUpdateVisibleGeometryAndProtocolDraft();
    void localizationThumbnailsAndPositioningExposeVisibleFeedback();
    void localizationProtocolRoundTripAndRunSnapshotStayCoherent();
    void localizationGeometryRespondsAndPreservesCoverageInvariants();
    void localizationThumbnailsRemainReadableAndTrackCurrentOrientation();
    void researchParametersNavigatesToExpandedL3();
    void qaJumpAndUnexecutedPagesShowHonestEmptyStates();
    void resultAndHistoryActionsAreBlockedBeforeARealMockPackage();
    void mockVerticalSliceCreatesBoundPackageAndActualHistory();
    void mockSnapshotBindsVisibleProtocolAndPlanning();
    void cancelledMockRunKeepsEvidenceAndCreatesNoSuccessfulArtifacts();
    void captureLocalizationPlanningEvidenceWhenRequested();
    void captureInteractionQaScreensWhenRequested();

private:
    QTemporaryDir m_isolatedMockResultRoot;
    QByteArray m_previousMockResultRoot;
    bool m_hadMockResultRoot = false;
};

void MainWindowTest::initTestCase()
{
    QVERIFY(m_isolatedMockResultRoot.isValid());
    m_hadMockResultRoot =
        qEnvironmentVariableIsSet("SCENARIO_NMR_MOCK_RESULT_ROOT");
    m_previousMockResultRoot =
        qgetenv("SCENARIO_NMR_MOCK_RESULT_ROOT");
    QVERIFY(qputenv("SCENARIO_NMR_MOCK_RESULT_ROOT",
                    QFile::encodeName(m_isolatedMockResultRoot.path())));
}

void MainWindowTest::cleanupTestCase()
{
    if (m_hadMockResultRoot) {
        qputenv("SCENARIO_NMR_MOCK_RESULT_ROOT",
                m_previousMockResultRoot);
    } else {
        qunsetenv("SCENARIO_NMR_MOCK_RESULT_ROOT");
    }
}

void MainWindowTest::sdkCanBeLoadedAndConnectedWithoutFileDialog()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    MriSdkConfig config;
    config.initPath = temp.filePath(QStringLiteral("init.ini"));
    config.parameterPath = temp.filePath(QStringLiteral("PTScan.par"));
    config.outputPath = temp.path();
    writeFile(config.initPath);
    writeFile(config.parameterPath);

    MainWindow window;
    const MriSdkResult result = window.loadSdkAndConnect(
        qEnvironmentVariable("FAKE_MRI_SDK_PATH"), config);

    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(window.deviceSessionState(), MriSdkSessionState::Ready);
}

void MainWindowTest::defaultSdkStateIsUnloadedAndRealButtonsDisabled()
{
    MainWindow window;

    QCOMPARE(window.deviceSessionState(), MriSdkSessionState::Unloaded);
    for (const QString& objectName : {
             QStringLiteral("LoadSdkButton"),
             QStringLiteral("ConnectDeviceButton"),
             QStringLiteral("RealPrecheckButton"),
             QStringLiteral("RealRunButton"),
             QStringLiteral("RealAbortButton")}) {
        auto* button = window.findChild<QPushButton*>(objectName);
        QVERIFY2(button, qPrintable(objectName));
        QVERIFY2(!button->isEnabled(), qPrintable(objectName));
    }
}

void MainWindowTest::configureEggControllerKeepsProxyUnreachableAndReportsHold()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    MainWindow window;
    auto* process = window.findChild<EggControllerProcess*>();
    auto* status = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    auto* mode = window.findChild<QComboBox*>(QStringLiteral("ControlModeCombo"));
    auto* start = window.findChild<QPushButton*>(QStringLiteral("RealRunButton"));
    QVERIFY(process);
    QVERIFY(status);
    QVERIFY(mode);
    QVERIFY(start);

    EggControllerLaunchConfig config;
    config.program = temp.filePath(QStringLiteral("real-eggcontroller-proxy.exe"));
    config.workingDirectory = temp.path();
    window.configureEggController(config);
    QCoreApplication::processEvents();

    QVERIFY(!process->isRunning());
    QVERIFY(status->text().contains(QStringLiteral("HOLD")));
    QVERIFY(!mode->isEnabled());
    QVERIFY(!start->isEnabled());
}

void MainWindowTest::holdHandlersLeaveMockNavigationAndStatusVisible()
{
    MainWindow window;
    auto* currentStep =
        window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    auto* workflowStatus =
        window.findChild<QLabel*>(QStringLiteral("WorkflowStatusStrip"));
    auto* next = window.findChild<QPushButton*>(QStringLiteral("WorkflowNextButton"));
    auto* status = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    QVERIFY(currentStep);
    QVERIFY(workflowStatus);
    QVERIFY(next);
    QVERIFY(status);

    QVERIFY(QMetaObject::invokeMethod(&window, "handleLoadSdk", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "handleConnect", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "handlePrecheck", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "handleStart", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "handleAbort", Qt::DirectConnection));

    QCOMPARE(window.deviceSessionState(), MriSdkSessionState::Unloaded);
    QVERIFY(status->text().contains(QStringLiteral("未通过真实预检")));
    QCOMPARE(currentStep->text(), QStringLiteral("01"));
    QVERIFY(next->isEnabled());
    next->click();
    QCOMPARE(currentStep->text(), QStringLiteral("02"));
    QVERIFY(workflowStatus->text().contains(QStringLiteral("当前")));
}

void MainWindowTest::mockPresentationAssetsDoNotContainEmbeddedDynamicStatus()
{
    const auto sha256 = [](const QString& resourcePath) {
        QFile file(resourcePath);
        if (!file.open(QIODevice::ReadOnly))
            return QByteArray();
        return QCryptographicHash::hash(
                   file.readAll(), QCryptographicHash::Sha256)
            .toHex()
            .toUpper();
    };

    QCOMPARE(
        sha256(QStringLiteral(":/mock-fse-acquisition.png")),
        QByteArrayLiteral(
            "D7F61D2C0F2D6F32D08E3CB28A3F7733E6081893CC860E7559E8C077CACE0F82"));
    QCOMPARE(
        sha256(QStringLiteral(":/mock-reconstruction.png")),
        QByteArrayLiteral(
            "7D7B774311AA99E35772A15DFF3755C1892318AFB5E99A8F1F6737A8CADDBE5F"));

    MainWindow window;
    window.show();

    window.setMockWorkflowStep(1);
    auto* entryStatus =
        window.findChild<QWidget*>(QStringLiteral("RightPage01"));
    QVERIFY(entryStatus);
    QString entryText;
    for (const QLabel* label : entryStatus->findChildren<QLabel*>())
        entryText += label->text() + QLatin1Char('\n');
    QVERIFY(entryText.contains(QStringLiteral("设备告警")));
    QVERIFY(entryText.contains(QStringLiteral("未核验")));
    QVERIFY(!entryText.contains(QStringLiteral("无异常")));

    window.setMockWorkflowStep(5);
    auto* calculation =
        window.findChild<QLabel*>(QStringLiteral("ProtocolAutoResultValue"));
    QVERIFY(calculation);
    QVERIFY(calculation->text().contains(QStringLiteral("未计算")));
    QVERIFY(!calculation->text().contains(QStringLiteral("3分20秒")));

    window.setMockWorkflowStep(7);
    auto* planning =
        window.findChild<QLabel*>(QStringLiteral("LocalizationPlanningSummary"));
    QVERIFY(planning);
    QVERIFY(planning->text().contains(QStringLiteral("预计未计算")));
    QVERIFY(planning->text().contains(QStringLiteral("SNR 未评估")));
    QVERIFY(!planning->text().contains(QStringLiteral("3分20秒")));
}

void MainWindowTest::workflowUsesMockNavigationAndKeepsRealRunOnHold()
{
    MainWindow window;

    QCOMPARE(window.windowTitle(), QStringLiteral("场景化核磁共振控制台"));
    auto* status = window.findChild<QLabel*>(QStringLiteral("WorkflowStatusStrip"));
    auto* currentStep = window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    auto* next = window.findChild<QPushButton*>(QStringLiteral("WorkflowNextButton"));
    auto* realRun = window.findChild<QPushButton*>(QStringLiteral("RealRunButton"));
    auto* protocol = window.findChild<QLabel*>(QStringLiteral("ProtocolChainLabel"));
    auto* addComparison = window.findChild<QPushButton*>(QStringLiteral("AddComparisonButton"));
    auto* planner = window.findChild<QWidget*>(QStringLiteral("LocalizationPlannerView"));
    auto* swapAxes = window.findChild<QPushButton*>(QStringLiteral("ReadPhaseSwapButton"));
    auto* mockAcquire = window.findChild<QPushButton*>(QStringLiteral("MockAcquireButton"));
    auto* openHistory = window.findChild<QPushButton*>(QStringLiteral("OpenHistoryButton"));
    auto* backToResults = window.findChild<QPushButton*>(QStringLiteral("BackToResultsButton"));

    QVERIFY(status);
    QVERIFY(currentStep);
    QVERIFY(next);
    QVERIFY(realRun);
    QVERIFY(protocol);
    QVERIFY(addComparison);
    QVERIFY(planner);
    QVERIFY(swapAxes);
    QVERIFY(mockAcquire);
    QVERIFY(openHistory);
    QVERIFY(backToResults);
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("MockLocImage")));
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("MockAcquisitionImage")));
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("MockResultImage")));
    auto* historyTable = window.findChild<QTableWidget*>(QStringLiteral("HistoryReadOnlyTable"));
    QVERIFY(historyTable);
    QVERIFY(!historyTable->isEnabled());
    QVERIFY(historyTable->toolTip().contains(QStringLiteral("尚无已封存")));
    QVERIFY(historyTable->editTriggers() == QAbstractItemView::NoEditTriggers);
    QVERIFY(status->text().contains(QStringLiteral("已完成：")));
    QVERIFY(status->text().contains(QStringLiteral("当前：")));
    QVERIFY(status->text().contains(QStringLiteral("下一步：")));
    QCOMPARE(currentStep->text(), QStringLiteral("01"));
    QVERIFY(!realRun->isEnabled());
    QVERIFY(!protocol->text().contains(QStringLiteral("FSE B")));
    QVERIFY(window.findChildren<QWidget*>(QStringLiteral("OperationNode")).isEmpty());
    QVERIFY(!planner->property("readPhaseSwapped").toBool());
    swapAxes->click();
    QVERIFY(planner->property("readPhaseSwapped").toBool());
    QTest::mousePress(planner, Qt::LeftButton, Qt::NoModifier, QPoint(40, 60));
    QTest::mouseMove(planner, QPoint(90, 110), 10);
    QTest::mouseRelease(planner, Qt::LeftButton, Qt::NoModifier, QPoint(90, 110));
    QVERIFY(planner->property("planningCoverageModified").toBool());
    next->click();
    QCOMPARE(currentStep->text(), QStringLiteral("02"));
    QVERIFY2(next->isEnabled(),
             "Selecting the default water-phantom template must expose Next.");
    next->click();
    QCOMPARE(currentStep->text(), QStringLiteral("03"));
    addComparison->click();
    QVERIFY(protocol->text().contains(QStringLiteral("FSE B")));
    QVERIFY(!addComparison->isEnabled());
    QVERIFY(addComparison->text().contains(QStringLiteral("已添加")));
    QVERIFY(!realRun->isEnabled());
    QVERIFY(!openHistory->isEnabled());
    window.setMockWorkflowStep(13);
    backToResults->click();
    QCOMPARE(currentStep->text(), QStringLiteral("12"));
}

void MainWindowTest::enabledWorkflowActionsExposeVisibleFeedback()
{
    MainWindow window;
    auto* feedback = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    auto* dryRun = window.findChild<QPushButton*>(QStringLiteral("DryRunButton"));
    QVERIFY(feedback);
    QVERIFY(dryRun);
    const QDir isolatedRoot(
        qEnvironmentVariable("SCENARIO_NMR_MOCK_RESULT_ROOT"));
    const QStringList entriesBeforeDryRun = isolatedRoot.entryList(
        QDir::AllEntries | QDir::NoDotAndDotDot);

    dryRun->click();
    QVERIFY(feedback->text().contains(QStringLiteral("DRY_RUN")));
    QVERIFY(feedback->text().contains(QStringLiteral("未写入 SDK")));
    QCOMPARE(isolatedRoot.entryList(QDir::AllEntries | QDir::NoDotAndDotDot),
             entriesBeforeDryRun);

    window.setMockWorkflowStep(5);
    auto* useOnce = window.findChild<QPushButton*>(QStringLiteral("ProtocolUseOnceButton"));
    auto* saveVersion = window.findChild<QPushButton*>(QStringLiteral("ProtocolSaveVersionButton"));
    QVERIFY(useOnce);
    QVERIFY(saveVersion);
    useOnce->click();
    QVERIFY(feedback->text().contains(QStringLiteral("仅本次使用")));
    QVERIFY(feedback->text().contains(QStringLiteral("未写入 SDK")));
    QVERIFY(!saveVersion->isEnabled());
    QVERIFY(saveVersion->toolTip().contains(QStringLiteral("未纳入 v0.1")));

    window.setMockWorkflowStep(9);
    auto* pause = window.findChild<QPushButton*>(QStringLiteral("MockPauseButton"));
    QVERIFY(pause);
    QVERIFY(!pause->isEnabled());

    window.setMockWorkflowStep(7);
    auto* modifyTarget =
        window.findChild<QPushButton*>(QStringLiteral("ModifyImagingTargetButton"));
    auto* researchParameters =
        window.findChild<QPushButton*>(QStringLiteral("ResearchParametersButton"));
    QVERIFY(modifyTarget);
    QVERIFY(researchParameters);
    modifyTarget->click();
    QVERIFY(feedback->text().contains(QStringLiteral("成像目标")));
    QVERIFY(feedback->text().contains(QStringLiteral("Mock")));
    researchParameters->click();
    QVERIFY(feedback->text().contains(QStringLiteral("科研参数")));
    QVERIFY(feedback->text().contains(QStringLiteral("L3")));

    window.setMockWorkflowStep(12);
    auto* openLocation =
        window.findChild<QPushButton*>(QStringLiteral("OpenResultLocationButton"));
    auto* external =
        window.findChild<QPushButton*>(QStringLiteral("ExternalAnalysisButton"));
    QVERIFY(openLocation);
    QVERIFY(external);
    QVERIFY(!openLocation->isEnabled());
    QVERIFY(openLocation->toolTip().contains(QStringLiteral("尚无")));
    QVERIFY(!external->isEnabled());
    QVERIFY(external->toolTip().contains(QStringLiteral("未配置")));
}

void MainWindowTest::realActionsExplainWhyTheyAreUnavailable()
{
    MainWindow window;
    auto* feedback = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    auto* loadSdk = window.findChild<QPushButton*>(QStringLiteral("LoadSdkButton"));
    auto* connectDevice = window.findChild<QPushButton*>(QStringLiteral("ConnectDeviceButton"));
    auto* precheck = window.findChild<QPushButton*>(QStringLiteral("RealPrecheckButton"));
    auto* realRun = window.findChild<QPushButton*>(QStringLiteral("RealRunButton"));
    auto* abort = window.findChild<QPushButton*>(QStringLiteral("RealAbortButton"));
    QVERIFY(feedback);
    QVERIFY(loadSdk);
    QVERIFY(connectDevice);
    QVERIFY(precheck);
    QVERIFY(realRun);
    QVERIFY(abort);
    QVERIFY(feedback->text().contains(QStringLiteral("等待现场确认")));
    QVERIFY(feedback->text().contains(QStringLiteral("未通过真实预检")));
    QVERIFY(!loadSdk->isEnabled());
    QVERIFY(!connectDevice->isEnabled());
    QVERIFY(!precheck->isEnabled());
    QVERIFY(!realRun->isEnabled());
    QVERIFY(realRun->text().contains(QStringLiteral("等待现场确认")));
    QVERIFY(!abort->isEnabled());
    QVERIFY(abort->isHidden());

    window.setMockWorkflowStep(8);
    auto* workflowRealRun =
        window.findChild<QPushButton*>(QStringLiteral("WorkflowRealRunButton"));
    auto* gateState = window.findChild<QLabel*>(QStringLiteral("RealAcquisitionGateState"));
    QVERIFY(workflowRealRun);
    QVERIFY(gateState);
    QVERIFY(!workflowRealRun->isEnabled());
    QVERIFY(workflowRealRun->text().contains(QStringLiteral("等待现场确认")));
    QVERIFY(gateState->text().contains(QStringLiteral("未通过真实预检")));
    QVERIFY2(gateState->maximumHeight() <= 80,
             "The real-acquisition gate must remain a compact status strip.");
}

void MainWindowTest::workflowUsesWaterPhantomTransverseSemantics()
{
    MainWindow window;
    auto* planner = window.findChild<QWidget*>(QStringLiteral("LocalizationPlannerView"));
    auto* sampleProfile = window.findChild<QLabel*>(QStringLiteral("SampleProfileLabel"));
    auto* acquisitionPlan =
        window.findChild<QLabel*>(QStringLiteral("RealAcquisitionPlanSummary"));
    auto* reconstructionPath =
        window.findChild<QLabel*>(QStringLiteral("ExistingReconstructionPathSummary"));
    auto* protocolChain = window.findChild<QLabel*>(QStringLiteral("ProtocolChainLabel"));
    auto* target = window.findChild<QComboBox*>(QStringLiteral("TargetCombo"));
    auto* templateList = window.findChild<QListWidget*>(QStringLiteral("TemplateList"));
    QVERIFY(planner);
    QVERIFY(sampleProfile);
    QVERIFY(acquisitionPlan);
    QVERIFY(reconstructionPath);
    QVERIFY(protocolChain);
    QVERIFY(target);
    QVERIFY(templateList);
    QCOMPARE(planner->property("selectedOrientation").toString(), QStringLiteral("横断"));
    QVERIFY(sampleProfile->text().contains(QStringLiteral("水模")));
    QVERIFY(acquisitionPlan->text().contains(QStringLiteral("水模")));
    QVERIFY(acquisitionPlan->text().contains(QStringLiteral("横断位")));
    QVERIFY(acquisitionPlan->text().contains(QStringLiteral("FSE A Mock")));
    QVERIFY(acquisitionPlan->text().contains(QStringLiteral("SNAPSHOT-PENDING")));
    QVERIFY(reconstructionPath->text().contains(QStringLiteral("MOCK")));
    QVERIFY(reconstructionPath->text().contains(QStringLiteral("不调用 eggcontrollerV2")));
    QVERIFY(protocolChain->text().contains(QStringLiteral("FSE A")));
    window.setMockWorkflowStep(2);
    QCOMPARE(target->currentData().toString(), QStringLiteral("标准水模"));
    QVERIFY(templateList->currentItem());
    QVERIFY(templateList->currentItem()->text().contains(QStringLiteral("水模横断位")));

    const auto catalog = SceneCatalog::defaults();
    QVERIFY(!catalog.isEmpty());
    QVERIFY(catalog.first().reconstruction.contains(QStringLiteral("合法 Mock 图像资产")));
    QVERIFY(!catalog.first().reconstruction.startsWith(QStringLiteral("复用")));

    window.setMockWorkflowStep(3);
    auto* rightStep3 = window.findChild<QWidget*>(QStringLiteral("RightPage03"));
    QVERIFY(rightStep3);
    QString step3Text;
    for (const QLabel* label : rightStep3->findChildren<QLabel*>())
        step3Text += label->text() + QLatin1Char('\n');
    QVERIFY(step3Text.contains(QStringLiteral("横断位")));
    QVERIFY(step3Text.contains(QStringLiteral("FSE A")));
    QVERIFY(!step3Text.contains(QStringLiteral("2D FSE")));

    window.setMockWorkflowStep(12);
    auto* rightStep12 = window.findChild<QWidget*>(QStringLiteral("RightPage12"));
    QVERIFY(rightStep12);
    QString step12Text;
    for (const QLabel* label : rightStep12->findChildren<QLabel*>())
        step12Text += label->text() + QLatin1Char('\n');
    QVERIFY(step12Text.contains(QStringLiteral("尚无成功 Mock 重建")));
    QVERIFY(step12Text.contains(QStringLiteral("尚无 QC 数值")));
    QVERIFY(step12Text.contains(QStringLiteral("外部分析\n未配置")));
}

void MainWindowTest::visibleWorkflowControlsHaveStableObjectNames()
{
    MainWindow window;
    window.show();
    auto* currentStep = window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    auto* feedback = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    QVERIFY(currentStep);
    QVERIFY(feedback);

    const auto assertNamedVisibleControls = [&window](int step) {
        window.setMockWorkflowStep(step);
        QCoreApplication::processEvents();
        const auto buttons = window.findChildren<QPushButton*>();
        for (QPushButton* button : buttons) {
            if (button->isVisibleTo(&window) && button->isEnabled()) {
                QVERIFY2(!button->objectName().isEmpty(),
                         qPrintable(QStringLiteral("步骤 %1 的可见启用按钮缺少 objectName：%2")
                                        .arg(step)
                                        .arg(button->text())));
            }
        }
        const auto radios = window.findChildren<QRadioButton*>();
        for (QRadioButton* radio : radios) {
            if (radio->isVisibleTo(&window) && radio->isEnabled()) {
                QVERIFY2(!radio->objectName().isEmpty(),
                         qPrintable(QStringLiteral("步骤 %1 的可见单选控件缺少 objectName")
                                        .arg(step)));
            }
        }
        const auto checks = window.findChildren<QCheckBox*>();
        for (QCheckBox* check : checks) {
            if (check->isVisibleTo(&window) && check->isEnabled()) {
                QVERIFY2(!check->objectName().isEmpty(),
                         qPrintable(QStringLiteral("步骤 %1 的可见复选控件缺少 objectName")
                                        .arg(step)));
            }
        }
        const auto combos = window.findChildren<QComboBox*>();
        for (QComboBox* combo : combos) {
            if (combo->isVisibleTo(&window) && combo->isEnabled()) {
                QVERIFY2(!combo->objectName().isEmpty(),
                         qPrintable(QStringLiteral("步骤 %1 的可见下拉控件缺少 objectName")
                                        .arg(step)));
            }
        }
        const auto edits = window.findChildren<QLineEdit*>();
        for (QLineEdit* edit : edits) {
            if (edit->isVisibleTo(&window) && edit->isEnabled()) {
                QVERIFY2(!edit->objectName().isEmpty(),
                         qPrintable(QStringLiteral("步骤 %1 的可见输入控件缺少 objectName")
                                        .arg(step)));
            }
        }
        const auto lists = window.findChildren<QListWidget*>();
        for (QListWidget* list : lists) {
            if (list->isVisibleTo(&window) && list->isEnabled()) {
                QVERIFY2(!list->objectName().isEmpty(),
                         qPrintable(QStringLiteral("步骤 %1 的可见列表控件缺少 objectName")
                                        .arg(step)));
            }
        }
        const auto tables = window.findChildren<QTableWidget*>();
        for (QTableWidget* table : tables) {
            if (table->isVisibleTo(&window) && table->isEnabled()) {
                QVERIFY2(!table->objectName().isEmpty(),
                         qPrintable(QStringLiteral("步骤 %1 的可见表格控件缺少 objectName")
                                        .arg(step)));
            }
        }
    };
    for (int step = 1; step <= 13; ++step) {
        assertNamedVisibleControls(step);
    }
}

void MainWindowTest::selectedTemplateOffersVisibleContinuationFromAnyWorkflowPage()
{
    MainWindow window;
    window.resize(1586, 992);
    window.show();
    window.setMockWorkflowStep(12);
    QCoreApplication::processEvents();

    auto* templateList = window.findChild<QListWidget*>(QStringLiteral("TemplateList"));
    auto* continueButton =
        window.findChild<QPushButton*>(QStringLiteral("UseSelectedTemplateButton"));
    auto* currentStep = window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    QVERIFY(templateList);
    QVERIFY(templateList->currentItem());
    QVERIFY2(continueButton,
             "Selecting a template from the persistent task panel must expose a continuation.");
    QVERIFY(continueButton->isEnabled());
    QVERIFY(continueButton->isVisibleTo(&window));

    const QRect buttonRect(continueButton->mapTo(&window, QPoint(0, 0)),
                           continueButton->size());
    QVERIFY2(window.rect().contains(buttonRect),
             "The template continuation must be fully inside the common viewport.");

    QTest::mouseClick(continueButton, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("03"));
}

void MainWindowTest::persistentWorkflowNavigationIsVisibleAtCommonWindowSize()
{
    MainWindow window;
    window.resize(1280, 760);
    window.show();

    auto* back = window.findChild<QPushButton*>(QStringLiteral("WorkflowBackButton"));
    auto* next = window.findChild<QPushButton*>(QStringLiteral("WorkflowNextButton"));
    auto* currentStep = window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    QVERIFY(back);
    QVERIFY(next);
    QVERIFY(currentStep);

    const auto assertFullyVisible = [&window](QPushButton* button, const char* message) {
        QVERIFY2(button->isVisibleTo(&window), message);
        const QRect buttonRect(button->mapTo(&window, QPoint(0, 0)), button->size());
        QVERIFY2(window.rect().contains(buttonRect), message);
    };

    for (int step = 2; step <= 13; ++step) {
        window.setMockWorkflowStep(step);
        QCoreApplication::processEvents();
        QVERIFY2(back->isEnabled(), qPrintable(QStringLiteral("步骤 %1 的返回按钮不可用").arg(step)));
        assertFullyVisible(back, "Every non-entry workflow page needs a visible Back action.");
    }

    for (int step = 1; step <= 12; ++step) {
        window.setMockWorkflowStep(step);
        QCoreApplication::processEvents();
        assertFullyVisible(next, "Every main workflow page must show the next-state control.");
    }

    window.setMockWorkflowStep(2);
    QVERIFY(next->isEnabled());
    QTest::mouseClick(next, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("03"));

    window.setMockWorkflowStep(8);
    QVERIFY(!next->isEnabled());
    window.setMockWorkflowStep(9);
    QVERIFY(!next->isEnabled());

    window.setMockWorkflowStep(12);
    QTest::mouseClick(back, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("11"));
}

void MainWindowTest::mockAcquisitionRequiresAllRunConfirmations()
{
    MainWindow window;
    window.show();
    window.setMockWorkflowStep(8);

    auto* acquire = window.findChild<QPushButton*>(QStringLiteral("MockAcquireButton"));
    auto* leftStart = window.findChild<QPushButton*>(QStringLiteral("LeftMockStartButton"));
    auto* next = window.findChild<QPushButton*>(QStringLiteral("WorkflowNextButton"));
    auto* currentStep = window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    QVERIFY(acquire);
    QVERIFY(leftStart);
    QVERIFY(next);
    QVERIFY(currentStep);
    QVERIFY(!acquire->isEnabled());
    QVERIFY(!leftStart->isEnabled());
    QVERIFY(!next->isEnabled());

    for (int index = 1; index <= 2; ++index) {
        auto* check = window.findChild<QCheckBox*>(
            QStringLiteral("RunConfirmationCheck%1").arg(index));
        QVERIFY(check);
        QTest::mouseClick(check, Qt::LeftButton);
        QVERIFY(!acquire->isEnabled());
        QVERIFY(!leftStart->isEnabled());
    }

    auto* finalCheck =
        window.findChild<QCheckBox*>(QStringLiteral("RunConfirmationCheck3"));
    QVERIFY(finalCheck);
    QTest::mouseClick(finalCheck, Qt::LeftButton);
    QVERIFY(!acquire->isEnabled());
    QVERIFY(!leftStart->isEnabled());
    QVERIFY(!next->isEnabled());
    QVERIFY(acquire->toolTip().contains(QStringLiteral("准备"))
            || acquire->toolTip().contains(QStringLiteral("定位")));

    QVERIFY(advanceToMockRunConfirmation(window));
    for (int index = 1; index <= 2; ++index) {
        auto* check = window.findChild<QCheckBox*>(
            QStringLiteral("RunConfirmationCheck%1").arg(index));
        QVERIFY(check);
        QTest::mouseClick(check, Qt::LeftButton);
        QVERIFY(!acquire->isEnabled());
    }
    finalCheck =
        window.findChild<QCheckBox*>(QStringLiteral("RunConfirmationCheck3"));
    QVERIFY(finalCheck);
    QTest::mouseClick(finalCheck, Qt::LeftButton);
    QVERIFY(acquire->isEnabled());
    QVERIFY(leftStart->isEnabled());
    QVERIFY(next->isEnabled());

    QTest::mouseClick(next, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("09"));
}

void MainWindowTest::mockPauseStopsAndResumesAutomaticProgression()
{
    MainWindow window;
    window.show();
    QVERIFY(advanceToMockRunConfirmation(window));

    for (int index = 1; index <= 3; ++index) {
        auto* check = window.findChild<QCheckBox*>(
            QStringLiteral("RunConfirmationCheck%1").arg(index));
        QVERIFY(check);
        QTest::mouseClick(check, Qt::LeftButton);
    }

    auto* acquire = window.findChild<QPushButton*>(QStringLiteral("MockAcquireButton"));
    auto* pause = window.findChild<QPushButton*>(QStringLiteral("MockPauseButton"));
    auto* currentStep = window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    QVERIFY(acquire);
    QVERIFY(pause);
    QVERIFY(currentStep);

    QTest::mouseClick(acquire, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("09"));
    QVERIFY(pause->isEnabled());

    QTest::mouseClick(pause, Qt::LeftButton);
    QTest::qWait(3400);
    QCOMPARE(currentStep->text(), QStringLiteral("09"));

    QTest::mouseClick(pause, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(currentStep->text(), QStringLiteral("10"), 3600);
}

void MainWindowTest::runConfirmationsResetAfterLeavingConfirmationStep()
{
    MainWindow window;
    window.show();
    QVERIFY(advanceToMockRunConfirmation(window));

    for (int index = 1; index <= 3; ++index) {
        auto* check = window.findChild<QCheckBox*>(
            QStringLiteral("RunConfirmationCheck%1").arg(index));
        QVERIFY(check);
        QTest::mouseClick(check, Qt::LeftButton);
        QVERIFY(check->isChecked());
    }
    auto* acquire = window.findChild<QPushButton*>(QStringLiteral("MockAcquireButton"));
    QVERIFY(acquire);
    QVERIFY(acquire->isEnabled());

    window.findChild<QPushButton*>(
        QStringLiteral("RunConfirmationBackButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("ConfirmLocalizationButton"))->click();

    for (int index = 1; index <= 3; ++index) {
        auto* check = window.findChild<QCheckBox*>(
            QStringLiteral("RunConfirmationCheck%1").arg(index));
        QVERIFY(check);
        QVERIFY(!check->isChecked());
    }
    QVERIFY(!acquire->isEnabled());

    for (int index = 1; index <= 3; ++index) {
        window.findChild<QCheckBox*>(
            QStringLiteral("RunConfirmationCheck%1").arg(index))->click();
    }
    QVERIFY(acquire->isEnabled());
    auto* primaryScene =
        window.findChild<QComboBox*>(QStringLiteral("PrimarySceneCombo"));
    auto* currentStep =
        window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    QVERIFY(primaryScene);
    QVERIFY(currentStep);
    QVERIFY(primaryScene->count() > 1);
    primaryScene->setCurrentIndex(1);
    QCoreApplication::processEvents();
    QCOMPARE(currentStep->text(), QStringLiteral("02"));
    QVERIFY(!acquire->isEnabled());
    for (int index = 1; index <= 3; ++index) {
        auto* check = window.findChild<QCheckBox*>(
            QStringLiteral("RunConfirmationCheck%1").arg(index));
        QVERIFY(check);
        QVERIFY(!check->isChecked());
        check->setChecked(true);
    }
    QVERIFY(!acquire->isEnabled());
}

void MainWindowTest::backFromProcessingReturnsToRunConfirmation()
{
    MainWindow window;
    window.show();
    window.setMockWorkflowStep(10);

    auto* back = window.findChild<QPushButton*>(QStringLiteral("WorkflowBackButton"));
    auto* currentStep = window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    QVERIFY(back);
    QVERIFY(currentStep);
    QVERIFY(back->isEnabled());

    QTest::mouseClick(back, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("08"));
}

void MainWindowTest::unsupportedCatalogTemplateIsClearlyBrowseOnly()
{
    MainWindow window;
    window.resize(1280, 760);
    window.show();
    window.setMockWorkflowStep(2);

    auto* primaryScene =
        window.findChild<QComboBox*>(QStringLiteral("PrimarySceneCombo"));
    auto* templateList =
        window.findChild<QListWidget*>(QStringLiteral("TemplateList"));
    auto* useSelected =
        window.findChild<QPushButton*>(QStringLiteral("UseSelectedTemplateButton"));
    auto* next =
        window.findChild<QPushButton*>(QStringLiteral("WorkflowNextButton"));
    auto* showRecommended =
        window.findChild<QPushButton*>(QStringLiteral("ShowRecommendedTemplateButton"));
    QVERIFY(primaryScene);
    QVERIFY(templateList);
    QVERIFY(useSelected);
    QVERIFY(next);
    QVERIFY(showRecommended);
    QVERIFY(primaryScene->count() > 1);

    primaryScene->setCurrentIndex(1);
    QCoreApplication::processEvents();

    QVERIFY(templateList->currentItem());
    QVERIFY(templateList->currentItem()->data(Qt::UserRole).toInt() != 0);
    QVERIFY(!useSelected->isEnabled());
    QVERIFY(useSelected->text().contains(QStringLiteral("仅供浏览")));
    QVERIFY(!next->isEnabled());
    QVERIFY(!showRecommended->isEnabled());
    QVERIFY(!showRecommended->toolTip().isEmpty());
    QVERIFY(!next->toolTip().isEmpty());
}

void MainWindowTest::templateRestartIsUnavailableDuringMockAcquisition()
{
    MainWindow window;
    window.show();
    QVERIFY(advanceToMockRunConfirmation(window));
    for (int index = 1; index <= 3; ++index) {
        window.findChild<QCheckBox*>(
            QStringLiteral("RunConfirmationCheck%1").arg(index))->click();
    }
    auto* acquire =
        window.findChild<QPushButton*>(QStringLiteral("MockAcquireButton"));
    QVERIFY(acquire);
    QVERIFY(acquire->isEnabled());
    acquire->click();

    auto* useSelected =
        window.findChild<QPushButton*>(QStringLiteral("UseSelectedTemplateButton"));
    QVERIFY(useSelected);
    QVERIFY(!useSelected->isEnabled());
    QVERIFY(useSelected->text().contains(QStringLiteral("Mock 采集中")));
    QVERIFY(!window.findChild<QComboBox*>(QStringLiteral("PrimarySceneCombo"))->isEnabled());
    QVERIFY(!window.findChild<QComboBox*>(QStringLiteral("TargetCombo"))->isEnabled());
    QVERIFY(!window.findChild<QLineEdit*>(QStringLiteral("TemplateSearchEdit"))->isEnabled());
    QVERIFY(!window.findChild<QListWidget*>(QStringLiteral("TemplateList"))->isEnabled());
    QVERIFY(!window.findChild<QPushButton*>(QStringLiteral("WorkflowBackButton"))->isEnabled());

    window.setMockWorkflowStep(12);
    QVERIFY(useSelected->isEnabled());
    QVERIFY(window.findChild<QComboBox*>(QStringLiteral("PrimarySceneCombo"))->isEnabled());
}

void MainWindowTest::globalNextDelegatesToCanonicalPageActionAndRespectsItsGate()
{
    MainWindow window;
    window.show();
    auto* next = window.findChild<QPushButton*>(QStringLiteral("WorkflowNextButton"));
    auto* currentStep = window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    auto* feedback = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    QVERIFY(next);
    QVERIFY(currentStep);
    QVERIFY(feedback);

    window.setMockWorkflowStep(4);
    QVERIFY(next->isEnabled());
    QTest::mouseClick(next, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("05"));
    QVERIFY2(feedback->text().contains(QStringLiteral("仅内存")),
             "The persistent Next action must delegate to preparation confirmation.");

    auto* continueProtocol =
        window.findChild<QPushButton*>(QStringLiteral("ContinueProtocolButton"));
    QVERIFY(continueProtocol);
    QVERIFY(!continueProtocol->isEnabled());
    QVERIFY(!next->isEnabled());

    window.setMockWorkflowStep(10);
    auto* completeProcessing =
        window.findChild<QPushButton*>(QStringLiteral("CompleteMockProcessingButton"));
    QVERIFY(completeProcessing);
    QVERIFY(!completeProcessing->isEnabled());
    QVERIFY(!next->isEnabled());

    window.setMockWorkflowStep(11);
    auto* confirmResult =
        window.findChild<QPushButton*>(QStringLiteral("ConfirmResultButton"));
    QVERIFY(confirmResult);
    QVERIFY(!confirmResult->isEnabled());
    QVERIFY(!next->isEnabled());
    auto* back = window.findChild<QPushButton*>(QStringLiteral("WorkflowBackButton"));
    QVERIFY(back);
    back->click();
    QCOMPARE(window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"))->text(),
             QStringLiteral("07"));
}

void MainWindowTest::preparationContinuationRecordsOnlyAnInMemoryMockDraft()
{
    MainWindow window;
    window.show();
    window.setMockWorkflowStep(4);

    auto* savePreparation =
        window.findChild<QPushButton*>(QStringLiteral("SavePreparationButton"));
    auto* currentStep = window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    auto* feedback = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    QVERIFY(savePreparation);
    QVERIFY(currentStep);
    QVERIFY(feedback);

    QTest::mouseClick(savePreparation, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("05"));
    QVERIFY(feedback->text().contains(QStringLiteral("Mock")));
    QVERIFY(feedback->text().contains(QStringLiteral("仅内存")));
    QVERIFY(feedback->text().contains(QStringLiteral("未写")));
}

void MainWindowTest::protocolL2ValidationGatesContinuationAndComputesSummary()
{
    MainWindow window;
    window.show();
    window.setMockWorkflowStep(5);

    auto* table = window.findChild<QTableWidget*>(QStringLiteral("ProtocolLevel2Table"));
    auto* fov = window.findChild<QLineEdit*>(QStringLiteral("ProtocolL2Current0"));
    auto* matrix = window.findChild<QLineEdit*>(QStringLiteral("ProtocolL2Current1"));
    auto* useOnce =
        window.findChild<QPushButton*>(QStringLiteral("ProtocolUseOnceButton"));
    auto* continueProtocol =
        window.findChild<QPushButton*>(QStringLiteral("ContinueProtocolButton"));
    auto* next = window.findChild<QPushButton*>(QStringLiteral("WorkflowNextButton"));
    auto* calculation =
        window.findChild<QLabel*>(QStringLiteral("ProtocolAutoResultValue"));
    QVERIFY(table);
    QVERIFY(fov);
    QVERIFY(matrix);
    QVERIFY(useOnce);
    QVERIFY(continueProtocol);
    QVERIFY(next);
    QVERIFY(calculation);

    QVERIFY(useOnce->isEnabled());
    QVERIFY(!continueProtocol->isEnabled());
    QVERIFY(!next->isEnabled());
    QTest::mouseClick(useOnce, Qt::LeftButton);
    QVERIFY(continueProtocol->isEnabled());
    QVERIFY(next->isEnabled());

    fov->setText(QStringLiteral("not-a-number"));
    QVERIFY(!fov->hasAcceptableInput());
    QVERIFY(table->item(0, 3)->text().contains(QStringLiteral("错误")));
    QVERIFY(!useOnce->isEnabled());
    QVERIFY(!continueProtocol->isEnabled());
    QVERIFY(!next->isEnabled());
    QVERIFY(calculation->text().contains(QStringLiteral("无法计算")));

    fov->setText(QStringLiteral("48×48 mm"));
    matrix->setText(QStringLiteral("128×128"));
    QVERIFY(fov->hasAcceptableInput());
    QVERIFY(matrix->hasAcceptableInput());
    QVERIFY(useOnce->isEnabled());
    QVERIFY(!continueProtocol->isEnabled());
    QVERIFY(calculation->text().contains(QStringLiteral("0.38×0.38")));

    QTest::mouseClick(useOnce, Qt::LeftButton);
    QVERIFY(continueProtocol->isEnabled());
    matrix->setText(QStringLiteral("96×96"));
    QVERIFY(!continueProtocol->isEnabled());
    QVERIFY(calculation->text().contains(QStringLiteral("0.50×0.50")));
}

void MainWindowTest::unsupportedProtocolVersionPersistenceIsDisabledWithReason()
{
    MainWindow window;
    window.setMockWorkflowStep(5);
    auto* saveVersion =
        window.findChild<QPushButton*>(QStringLiteral("ProtocolSaveVersionButton"));
    QVERIFY(saveVersion);
    QVERIFY(!saveVersion->isEnabled());
    QVERIFY(saveVersion->toolTip().contains(QStringLiteral("v0.1")));
    QVERIFY(saveVersion->accessibleDescription().contains(QStringLiteral("未纳入")));
}

void MainWindowTest::locReferenceClearlyRoutesToInteractivePlanning()
{
    MainWindow window;
    window.show();
    window.setMockWorkflowStep(6);

    auto* page =
        window.findChild<QWidget*>(QStringLiteral("WorkflowPage06"));
    QVERIFY(page);
    auto* evidence =
        page->findChild<QLabel*>(QStringLiteral("MockImageEvidenceLabel"));
    auto* openPlanning =
        window.findChild<QPushButton*>(
            QStringLiteral("OpenLocalizationPlanningButton"));
    QVERIFY(evidence);
    QVERIFY(openPlanning);
    QVERIFY2(
        evidence->text().contains(QStringLiteral("此页不可拖动")),
        "The static LOC reference must not look like an interactive planner.");
    QVERIFY2(
        openPlanning->text().contains(QStringLiteral("可交互")),
        "The call to action must clearly route to interactive planning.");
}

void MainWindowTest::localizationActionsExposeFeedbackAndRespectAxialGate()
{
    MainWindow window;
    window.show();
    window.setMockWorkflowStep(7);

    auto* feedback =
        window.findChild<QLabel*>(QStringLiteral("LocalizationActionFeedback"));
    auto* coronal =
        window.findChild<QPushButton*>(QStringLiteral("OrientationCoronalButton"));
    auto* axial =
        window.findChild<QPushButton*>(QStringLiteral("OrientationAxialButton"));
    auto* swap =
        window.findChild<QPushButton*>(QStringLiteral("ReadPhaseSwapButton"));
    auto* reset =
        window.findChild<QPushButton*>(QStringLiteral("ResetPlanningButton"));
    auto* confirm =
        window.findChild<QPushButton*>(QStringLiteral("ConfirmLocalizationButton"));
    auto* next = window.findChild<QPushButton*>(QStringLiteral("WorkflowNextButton"));
    QVERIFY(feedback);
    QVERIFY(coronal);
    QVERIFY(axial);
    QVERIFY(swap);
    QVERIFY(reset);
    QVERIFY(confirm);
    QVERIFY(next);

    QTest::mouseClick(coronal, Qt::LeftButton);
    QVERIFY(feedback->text().contains(QStringLiteral("冠状")));
    QVERIFY(feedback->text().contains(QStringLiteral("横断")));
    QVERIFY(!confirm->isEnabled());
    QVERIFY(!next->isEnabled());

    QTest::mouseClick(axial, Qt::LeftButton);
    QVERIFY(confirm->isEnabled());
    QVERIFY(next->isEnabled());
    QTest::mouseClick(swap, Qt::LeftButton);
    QVERIFY(feedback->text().contains(QStringLiteral("Phase / Read")));
    QTest::mouseClick(reset, Qt::LeftButton);
    QVERIFY(feedback->text().contains(QStringLiteral("横断")));
    QVERIFY(feedback->text().contains(QStringLiteral("推荐")));
}

void MainWindowTest::localizationSliceControlsUpdateVisibleGeometryAndProtocolDraft()
{
    MainWindow window;
    window.resize(1586, 992);
    window.show();
    window.setMockWorkflowStep(7);

    auto* thickness =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral("SliceThicknessSpinBox"));
    auto* gap =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral("SliceGapSpinBox"));
    auto* count =
        window.findChild<QSpinBox*>(
            QStringLiteral("SliceCountSpinBox"));
    auto* slicePosition =
        window.findChild<QSlider*>(
            QStringLiteral("SlicePositionSlider"));
    auto* planner =
        window.findChild<QWidget*>(
            QStringLiteral("LocalizationPlannerView"));
    auto* summary =
        window.findChild<QLabel*>(
            QStringLiteral("LocalizationPlanningSummary"));
    auto* protocolThickness =
        window.findChild<QLineEdit*>(
            QStringLiteral("ProtocolL2Current2"));
    auto* protocolGap =
        window.findChild<QLineEdit*>(
            QStringLiteral("ProtocolL2Current3"));

    QVERIFY(thickness);
    QVERIFY(gap);
    QVERIFY(count);
    QVERIFY(slicePosition);
    QVERIFY(planner);
    QVERIFY(summary);
    QVERIFY(protocolThickness);
    QVERIFY(protocolGap);
    QVERIFY2(
        summary->text().contains(QStringLiteral("层组位置 50%")),
        "Entering planning must render the current interactive geometry.");

    thickness->setValue(4.2);
    gap->setValue(1.0);
    count->setValue(9);
    slicePosition->setValue(70);

    QCOMPARE(planner->property("sliceThicknessMm").toDouble(), 4.2);
    QCOMPARE(planner->property("sliceGapMm").toDouble(), 1.0);
    QCOMPARE(planner->property("sliceCount").toInt(), 9);
    QCOMPARE(planner->property("slicePosition").toDouble(), 0.7);
    QVERIFY(summary->text().contains(QStringLiteral("4.2 mm")));
    QVERIFY(summary->text().contains(QStringLiteral("9 层")));
    QVERIFY(summary->text().contains(QStringLiteral("45.8 mm")));
    QVERIFY(summary->text().contains(QStringLiteral("70%")));
    QCOMPARE(protocolThickness->text(), QStringLiteral("4.2 mm"));
    QCOMPARE(protocolGap->text(), QStringLiteral("1.0 mm"));

    auto* confirm =
        window.findChild<QPushButton*>(
            QStringLiteral("ConfirmLocalizationButton"));
    QVERIFY(confirm);
    QVERIFY(confirm->isEnabled());
    QTest::mouseClick(confirm, Qt::LeftButton);

    auto* confirmationTable =
        window.findChild<QTableWidget*>(
            QStringLiteral("RunConfirmationTable"));
    QVERIFY(confirmationTable);
    QVERIFY(confirmationTable->item(3, 1));
    const QString snapshot = confirmationTable->item(3, 1)->text();
    QVERIFY(snapshot.contains(QStringLiteral("层厚 4.2 mm")));
    QVERIFY(snapshot.contains(QStringLiteral("层间距 1.0 mm")));
    QVERIFY(snapshot.contains(QStringLiteral("9 层")));
}

void MainWindowTest::localizationThumbnailsAndPositioningExposeVisibleFeedback()
{
    MainWindow window;
    window.resize(1586, 992);
    window.show();
    window.setMockWorkflowStep(7);

    auto* planner =
        window.findChild<QWidget*>(
            QStringLiteral("LocalizationPlannerView"));
    auto* coronalThumbnail =
        window.findChild<QPushButton*>(
            QStringLiteral("LocalizationThumbnailCoronal"));
    auto* axialThumbnail =
        window.findChild<QPushButton*>(
            QStringLiteral("LocalizationThumbnailAxial"));
    auto* center =
        window.findChild<QPushButton*>(
            QStringLiteral("CenterPlanningButton"));
    auto* slicePosition =
        window.findChild<QSlider*>(
            QStringLiteral("SlicePositionSlider"));
    auto* feedback =
        window.findChild<QLabel*>(
            QStringLiteral("LocalizationActionFeedback"));
    auto* summary =
        window.findChild<QLabel*>(
            QStringLiteral("LocalizationPlanningSummary"));

    QVERIFY(planner);
    QVERIFY(coronalThumbnail);
    QVERIFY(axialThumbnail);
    QVERIFY(center);
    QVERIFY(slicePosition);
    QVERIFY(feedback);
    QVERIFY(summary);

    const auto peakLuminance = [](const QImage& image) {
        int peak = 0;
        for (int y = 0; y < image.height(); y += 8) {
            for (int x = 0; x < image.width(); x += 8) {
                peak = qMax(peak, qGray(image.pixel(x, y)));
            }
        }
        return peak;
    };
    const int unfocusedPeak =
        peakLuminance(planner->grab().toImage());
    planner->setFocus(Qt::OtherFocusReason);
    QCoreApplication::processEvents();
    QVERIFY(planner->hasFocus());
    const int focusedPeak =
        peakLuminance(planner->grab().toImage());
    QVERIFY2(
        focusedPeak >= unfocusedPeak * 0.8,
        qPrintable(
            QStringLiteral(
                "Keyboard focus must not darken the image "
                "(before=%1 after=%2).")
                .arg(unfocusedPeak)
                .arg(focusedPeak)));

    QCOMPARE(slicePosition->value(), 50);
    QTest::keyClick(
        planner, Qt::Key_Down, Qt::ControlModifier);
    QCOMPARE(
        planner->property("slicePosition").toDouble(), 0.51);
    QCOMPARE(slicePosition->value(), 51);
    QVERIFY(feedback->text().contains(QStringLiteral("层组位置")));

    QTest::mouseClick(coronalThumbnail, Qt::LeftButton);
    QCOMPARE(planner->property("selectedOrientation").toString(),
             QStringLiteral("冠状"));
    QVERIFY(feedback->text().contains(QStringLiteral("冠状")));

    QTest::mouseClick(axialThumbnail, Qt::LeftButton);
    QCOMPARE(planner->property("selectedOrientation").toString(),
             QStringLiteral("横断"));

    const QPoint dragStart(planner->width() / 2, planner->height() / 2);
    const QPoint dragEnd(
        planner->width() * 3 / 5, planner->height() * 2 / 5);
    QTest::mousePress(planner, Qt::LeftButton, Qt::NoModifier, dragStart);
    QTest::mouseMove(planner, dragEnd, 10);
    QTest::mouseRelease(
        planner, Qt::LeftButton, Qt::NoModifier, dragEnd);

    QVERIFY(planner->property("planningCoverageModified").toBool());
    QVERIFY(feedback->text().contains(QStringLiteral("中心")));
    QVERIFY(summary->text().contains(QStringLiteral("中心")));
    QVERIFY(planner->property("coverageCenterX").toDouble() != 0.5
            || planner->property("coverageCenterY").toDouble() != 0.5);

    QTest::mouseClick(center, Qt::LeftButton);
    QCOMPARE(planner->property("coverageCenterX").toDouble(), 0.5);
    QCOMPARE(planner->property("coverageCenterY").toDouble(), 0.5);
    QVERIFY(feedback->text().contains(QStringLiteral("已居中")));
}

void MainWindowTest::localizationProtocolRoundTripAndRunSnapshotStayCoherent()
{
    MainWindow window;
    window.resize(1586, 992);
    window.show();
    window.setMockWorkflowStep(3);
    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("AddComparisonButton")),
        Qt::LeftButton);
    window.setMockWorkflowStep(5);

    auto* protocolThickness =
        window.findChild<QLineEdit*>(
            QStringLiteral("ProtocolL2Current2"));
    auto* protocolGap =
        window.findChild<QLineEdit*>(
            QStringLiteral("ProtocolL2Current3"));
    auto* useOnce =
        window.findChild<QPushButton*>(
            QStringLiteral("ProtocolUseOnceButton"));
    auto* continueProtocol =
        window.findChild<QPushButton*>(
            QStringLiteral("ContinueProtocolButton"));
    QVERIFY(protocolThickness);
    QVERIFY(protocolGap);
    QVERIFY(useOnce);
    QVERIFY(continueProtocol);

    protocolThickness->setText(QStringLiteral("5.0 mm"));
    protocolGap->setText(QStringLiteral("2.0 mm"));
    QTest::mouseClick(useOnce, Qt::LeftButton);
    QVERIFY(continueProtocol->isEnabled());
    QTest::mouseClick(continueProtocol, Qt::LeftButton);
    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("OpenLocalizationPlanningButton")),
        Qt::LeftButton);

    auto* thickness =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral("SliceThicknessSpinBox"));
    auto* gap =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral("SliceGapSpinBox"));
    auto* count =
        window.findChild<QSpinBox*>(
            QStringLiteral("SliceCountSpinBox"));
    auto* position =
        window.findChild<QSlider*>(
            QStringLiteral("SlicePositionSlider"));
    auto* target =
        window.findChild<QComboBox*>(
            QStringLiteral("ImagingTargetCombo"));
    auto* applyTarget =
        window.findChild<QPushButton*>(
            QStringLiteral("ModifyImagingTargetButton"));
    auto* planner =
        window.findChild<QWidget*>(
            QStringLiteral("LocalizationPlannerView"));
    QVERIFY(thickness);
    QVERIFY(gap);
    QVERIFY(count);
    QVERIFY(position);
    QVERIFY(target);
    QVERIFY(applyTarget);
    QVERIFY(planner);
    QCOMPARE(thickness->value(), 5.0);
    QCOMPARE(gap->value(), 2.0);

    count->setValue(9);
    position->setValue(70);
    const int effectiveLayerPosition = position->value();
    QVERIFY(effectiveLayerPosition < 70);
    target->setCurrentIndex(1);
    QTest::mouseClick(applyTarget, Qt::LeftButton);
    planner->setFocus();
    QTest::keyClick(planner, Qt::Key_Right);
    QTest::keyClick(planner, Qt::Key_Down);

    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("ConfirmLocalizationButton")),
        Qt::LeftButton);
    auto* table =
        window.findChild<QTableWidget*>(
            QStringLiteral("RunConfirmationTable"));
    QVERIFY(table);
    QVERIFY(table->item(1, 1));
    QVERIFY(table->item(2, 1));
    QVERIFY(table->item(3, 1));
    const QString taskSnapshot = table->item(1, 1)->text();
    const QString executionSnapshot = table->item(2, 1)->text();
    const QString visibleSnapshot = table->item(3, 1)->text();
    QVERIFY(taskSnapshot.contains(QStringLiteral("FSE B")));
    QVERIFY(taskSnapshot.contains(QStringLiteral("dataSource=MOCK")));
    QVERIFY(executionSnapshot.contains(QStringLiteral("TR 3000")));
    QVERIFY(executionSnapshot.contains(QStringLiteral("TE 12.9")));
    QVERIFY(visibleSnapshot.contains(QStringLiteral("层厚 5.0 mm")));
    QVERIFY(visibleSnapshot.contains(QStringLiteral("层间距 2.0 mm")));
    QVERIFY(visibleSnapshot.contains(QStringLiteral("9 层")));
    QVERIFY(
        visibleSnapshot.contains(
            QStringLiteral("层组位置 %1%")
                .arg(effectiveLayerPosition)));
    QVERIFY(visibleSnapshot.contains(QStringLiteral("中心 51% / 51%")));
    QVERIFY(visibleSnapshot.contains(QStringLiteral("覆盖框")));
    QVERIFY(visibleSnapshot.contains(QStringLiteral("成像目标 结构细节")));

    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("RunConfirmationBackButton")),
        Qt::LeftButton);
    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("ResearchParametersButton")),
        Qt::LeftButton);
    auto* calculation =
        window.findChild<QLabel*>(
            QStringLiteral("ProtocolAutoResultValue"));
    QVERIFY(calculation);
    QVERIFY(calculation->text().contains(QStringLiteral("9 层")));
    QVERIFY(calculation->text().contains(QStringLiteral("61.0 mm")));
}

void MainWindowTest::localizationGeometryRespondsAndPreservesCoverageInvariants()
{
    MainWindow window;
    window.resize(1586, 992);
    window.show();
    window.setMockWorkflowStep(7);

    auto* thickness =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral("SliceThicknessSpinBox"));
    auto* gap =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral("SliceGapSpinBox"));
    auto* count =
        window.findChild<QSpinBox*>(
            QStringLiteral("SliceCountSpinBox"));
    auto* position =
        window.findChild<QSlider*>(
            QStringLiteral("SlicePositionSlider"));
    auto* planner =
        window.findChild<QWidget*>(
            QStringLiteral("LocalizationPlannerView"));
    QVERIFY(thickness);
    QVERIFY(gap);
    QVERIFY(count);
    QVERIFY(position);
    QVERIFY(planner);

    const double initialStackRatio =
        planner->property("sliceStackHeightRatio").toDouble();
    QVERIFY(initialStackRatio > 0.0);
    position->setValue(100);
    const double initialEffectiveMaximum =
        1.0 - initialStackRatio / 2.0;
    QCOMPARE(
        planner->property("sliceStackCenter").toDouble(),
        initialEffectiveMaximum);
    QCOMPARE(
        position->value(),
        qRound(initialEffectiveMaximum * 100.0));
    position->setValue(50);
    thickness->setValue(2.0);
    gap->setValue(0.5);
    count->setValue(5);
    const double compactStackRatio =
        planner->property("sliceStackHeightRatio").toDouble();
    QVERIFY(compactStackRatio < initialStackRatio);
    position->setValue(70);
    QCOMPARE(
        planner->property("sliceStackCenter").toDouble(), 0.7);

    thickness->setValue(0.1);
    gap->setValue(0.0);
    count->setValue(64);
    QCOMPARE(planner->property("renderedSliceCount").toInt(), 64);

    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("ResetPlanningButton")),
        Qt::LeftButton);
    QCoreApplication::processEvents();

    const QPixmap reference(QStringLiteral(":/mock-localization-clean.png"));
    QVERIFY(!reference.isNull());
    const QSize available(
        qMax(1, planner->width() - 12),
        qMax(1, planner->height() - 12));
    const QSize targetSize =
        reference.size().scaled(available, Qt::KeepAspectRatio);
    const QRectF target(
        (planner->width() - targetSize.width()) / 2.0,
        (planner->height() - targetSize.height()) / 2.0,
        targetSize.width(), targetSize.height());
    const auto coverageRect = [&] {
        return QRectF(
            target.left()
                + target.width()
                      * planner->property("coverageX").toDouble(),
            target.top()
                + target.height()
                      * planner->property("coverageY").toDouble(),
            target.width()
                * planner->property("coverageWidth").toDouble(),
            target.height()
                * planner->property("coverageHeight").toDouble());
    };
    const auto centerPoint = [&] {
        return QPoint(
            qRound(
                target.left()
                + target.width()
                      * planner->property("coverageCenterX")
                            .toDouble()),
            qRound(
                target.top()
                + target.height()
                      * planner->property("coverageCenterY")
                            .toDouble()));
    };

    QRectF coverage = coverageRect();
    const QPoint nearBottomRight =
        (coverage.bottomRight() - QPointF(24, 24)).toPoint();
    QTest::mousePress(
        planner, Qt::LeftButton, Qt::NoModifier, centerPoint());
    QTest::mouseMove(planner, nearBottomRight, 20);
    QTest::mouseRelease(
        planner, Qt::LeftButton, Qt::NoModifier, nearBottomRight);

    coverage = coverageRect();
    const QPoint resizeStart = coverage.bottomRight().toPoint();
    const QPoint resizeEnd =
        QPoint(
            qRound(coverage.left() + target.width() * 0.24),
            qRound(coverage.top() + target.height() * 0.24));
    QTest::mousePress(
        planner, Qt::LeftButton, Qt::NoModifier, resizeStart);
    QTest::mouseMove(planner, resizeEnd, 20);
    QTest::mouseRelease(
        planner, Qt::LeftButton, Qt::NoModifier, resizeEnd);

    const double boxX = planner->property("coverageX").toDouble();
    const double boxY = planner->property("coverageY").toDouble();
    const double boxWidth =
        planner->property("coverageWidth").toDouble();
    const double boxHeight =
        planner->property("coverageHeight").toDouble();
    const double centerX =
        planner->property("coverageCenterX").toDouble();
    const double centerY =
        planner->property("coverageCenterY").toDouble();
    QVERIFY(centerX >= boxX && centerX <= boxX + boxWidth);
    QVERIFY(centerY >= boxY && centerY <= boxY + boxHeight);
}

void MainWindowTest::localizationThumbnailsRemainReadableAndTrackCurrentOrientation()
{
    MainWindow window;
    window.resize(1586, 992);
    window.show();
    window.setMockWorkflowStep(7);
    QCoreApplication::processEvents();

    auto* axial =
        window.findChild<QPushButton*>(
            QStringLiteral("LocalizationThumbnailAxial"));
    auto* coronal =
        window.findChild<QPushButton*>(
            QStringLiteral("LocalizationThumbnailCoronal"));
    auto* sagittal =
        window.findChild<QPushButton*>(
            QStringLiteral("LocalizationThumbnailSagittal"));
    auto* axialLabel =
        window.findChild<QLabel*>(
            QStringLiteral("LocalizationThumbnailAxialLabel"));
    auto* coronalLabel =
        window.findChild<QLabel*>(
            QStringLiteral("LocalizationThumbnailCoronalLabel"));
    auto* sagittalLabel =
        window.findChild<QLabel*>(
            QStringLiteral("LocalizationThumbnailSagittalLabel"));
    auto* axialImage =
        window.findChild<QWidget*>(
            QStringLiteral("LocalizationThumbnailAxialImage"));
    QVERIFY(axial);
    QVERIFY(coronal);
    QVERIFY(sagittal);
    QVERIFY(axialLabel);
    QVERIFY(coronalLabel);
    QVERIFY(sagittalLabel);
    QVERIFY(axialImage);
    QVERIFY(axial->height() >= 100);
    QVERIFY(coronal->height() >= 100);
    QVERIFY(sagittal->height() >= 100);
    QVERIFY(axialLabel->isVisibleTo(&window));
    QVERIFY(coronalLabel->isVisibleTo(&window));
    QVERIFY(sagittalLabel->isVisibleTo(&window));
    QVERIFY(axialImage->height() >= 60);
    QVERIFY(axialLabel->height() >= 18);
    QVERIFY(axial->rect().contains(axialImage->geometry()));
    QVERIFY(axial->rect().contains(axialLabel->geometry()));
    QVERIFY(axialLabel->text().contains(QStringLiteral("当前")));
    QVERIFY(!coronalLabel->text().contains(QStringLiteral("当前")));
    QVERIFY(!sagittalLabel->text().contains(QStringLiteral("当前")));

    QTest::mouseClick(coronal, Qt::LeftButton);
    QVERIFY(!axialLabel->text().contains(QStringLiteral("当前")));
    QVERIFY(coronalLabel->text().contains(QStringLiteral("当前")));
    QVERIFY(!sagittalLabel->text().contains(QStringLiteral("当前")));
}

void MainWindowTest::researchParametersNavigatesToExpandedL3()
{
    MainWindow window;
    window.show();
    window.setMockWorkflowStep(7);

    auto* research =
        window.findChild<QPushButton*>(QStringLiteral("ResearchParametersButton"));
    auto* l3 = window.findChild<QLabel*>(QStringLiteral("L3DetailsLabel"));
    auto* currentStep = window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    auto* feedback = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    QVERIFY(research);
    QVERIFY(l3);
    QVERIFY(currentStep);
    QVERIFY(feedback);
    QVERIFY(l3->isHidden());

    QTest::mouseClick(research, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("05"));
    QVERIFY(!l3->isHidden());
    QVERIFY(feedback->text().contains(QStringLiteral("L3")));
    QVERIFY(feedback->text().contains(QStringLiteral("未写入 SDK")));
}

void MainWindowTest::qaJumpAndUnexecutedPagesShowHonestEmptyStates()
{
    MainWindow window;
    window.show();
    auto* status = window.findChild<QLabel*>(QStringLiteral("WorkflowStatusStrip"));
    QVERIFY(status);

    window.setMockWorkflowStep(6);
    auto* locReference = window.findChild<QWidget*>(QStringLiteral("MockLocImage"));
    QVERIFY(locReference);
    QCOMPARE(locReference->property("evidenceKind").toString(),
             QStringLiteral("MOCK_PLANNING_REFERENCE"));

    window.setMockWorkflowStep(9);
    auto* page9 = window.findChild<QWidget*>(QStringLiteral("WorkflowPage09"));
    auto* acquisitionImage =
        window.findChild<QWidget*>(QStringLiteral("MockAcquisitionImage"));
    auto* pause = window.findChild<QPushButton*>(QStringLiteral("MockPauseButton"));
    auto* stop = window.findChild<QPushButton*>(QStringLiteral("LeftMockStopButton"));
    QVERIFY(page9);
    QVERIFY(acquisitionImage);
    QVERIFY(pause);
    QVERIFY(stop);
    QString page9Text;
    for (const QLabel* label : page9->findChildren<QLabel*>())
        page9Text += label->text() + QLatin1Char('\n');
    QVERIFY(page9Text.contains(QStringLiteral("尚未开始")));
    QVERIFY(!page9Text.contains(QStringLiteral("64%")));
    QVERIFY(!acquisitionImage->isVisibleTo(&window));
    QVERIFY(!pause->isEnabled());
    QVERIFY(!stop->isEnabled());

    window.setMockWorkflowStep(10);
    auto* processing = window.findChild<QTableWidget*>(QStringLiteral("MockProcessingSteps"));
    auto* complete =
        window.findChild<QPushButton*>(QStringLiteral("CompleteMockProcessingButton"));
    QVERIFY(processing);
    QVERIFY(complete);
    QString processingText;
    for (int row = 0; row < processing->rowCount(); ++row)
        processingText += processing->item(row, 1)->text() + QLatin1Char('\n');
    QVERIFY(!processingText.contains(QStringLiteral("已保存")));
    QVERIFY(!processingText.contains(QStringLiteral("72%")));
    QVERIFY(!complete->isEnabled());
    QVERIFY(!status->text().contains(QStringLiteral("已完成：FSE A Mock采集")));

    window.setMockWorkflowStep(11);
    auto* resultImage = window.findChild<QWidget*>(QStringLiteral("MockResultImage"));
    auto* confirm = window.findChild<QPushButton*>(QStringLiteral("ConfirmResultButton"));
    auto* right11 = window.findChild<QWidget*>(QStringLiteral("RightPage11"));
    QVERIFY(resultImage);
    QVERIFY(confirm);
    QVERIFY(right11);
    QVERIFY(!resultImage->isVisibleTo(&window));
    QVERIFY(!confirm->isEnabled());
    QString rightText;
    for (const QLabel* label : right11->findChildren<QLabel*>())
        rightText += label->text() + QLatin1Char('\n');
    QVERIFY(!rightText.contains(QStringLiteral("33.2")));
    QVERIFY(!rightText.contains(QStringLiteral("64.1")));
    QVERIFY(!status->text().contains(QStringLiteral("已完成：RAW保存与重建")));

    window.setMockWorkflowStep(12);
    QVERIFY(!status->text().contains(QStringLiteral("已完成：标准结果与QC")));
}

void MainWindowTest::resultAndHistoryActionsAreBlockedBeforeARealMockPackage()
{
    MainWindow window;
    window.show();
    window.setMockWorkflowStep(12);

    auto* resultImage = window.findChild<QWidget*>(QStringLiteral("ResultPackageImage"));
    auto* metadata = window.findChild<QLabel*>(QStringLiteral("ResultPackageMetadata"));
    auto* save = window.findChild<QPushButton*>(QStringLiteral("SaveResultPackageButton"));
    auto* open = window.findChild<QPushButton*>(QStringLiteral("OpenResultLocationButton"));
    auto* copy = window.findChild<QPushButton*>(QStringLiteral("CopyResultPathButton"));
    auto* external = window.findChild<QPushButton*>(QStringLiteral("ExternalAnalysisButton"));
    auto* history = window.findChild<QPushButton*>(QStringLiteral("OpenHistoryButton"));
    auto* next = window.findChild<QPushButton*>(QStringLiteral("WorkflowNextButton"));
    QVERIFY(resultImage);
    QVERIFY(metadata);
    QVERIFY(save);
    QVERIFY(open);
    QVERIFY(copy);
    QVERIFY(external);
    QVERIFY(history);
    QVERIFY(next);
    QVERIFY(!resultImage->isVisibleTo(&window));
    QVERIFY(!metadata->text().contains(QStringLiteral("RUN-MOCK-001")));
    QVERIFY(!save->isEnabled());
    QVERIFY(!open->isEnabled());
    QVERIFY(!copy->isEnabled());
    QVERIFY(!external->isEnabled());
    QVERIFY(external->toolTip().contains(QStringLiteral("未配置")));
    QVERIFY(!history->isEnabled());
    QVERIFY(!next->isEnabled());

    window.setMockWorkflowStep(13);
    auto* table = window.findChild<QTableWidget*>(QStringLiteral("HistoryReadOnlyTable"));
    auto* sampleFilter = window.findChild<QComboBox*>(QStringLiteral("HistorySampleFilter"));
    auto* keyword = window.findChild<QLineEdit*>(QStringLiteral("HistoryFilter"));
    auto* historyOpen = window.findChild<QPushButton*>(QStringLiteral("HistoryOpenButton"));
    auto* historyCompare =
        window.findChild<QPushButton*>(QStringLiteral("HistoryCompareButton"));
    auto* historySource =
        window.findChild<QPushButton*>(QStringLiteral("HistorySourceButton"));
    auto* summary = window.findChild<QLabel*>(QStringLiteral("HistorySelectionSummary"));
    auto* back = window.findChild<QPushButton*>(QStringLiteral("BackToResultsButton"));
    QVERIFY(table);
    QVERIFY(sampleFilter);
    QVERIFY(keyword);
    QVERIFY(historyOpen);
    QVERIFY(historyCompare);
    QVERIFY(historySource);
    QVERIFY(summary);
    QVERIFY(back);
    QCOMPARE(table->rowCount(), 0);
    QVERIFY(!sampleFilter->isEnabled());
    QVERIFY(!keyword->isEnabled());
    QVERIFY(!historyOpen->isEnabled());
    QVERIFY(!historyCompare->isEnabled());
    QVERIFY(historyCompare->toolTip().contains(QStringLiteral("不支持对比")));
    QVERIFY(!historySource->isEnabled());
    QVERIFY(summary->text().contains(QStringLiteral("尚无")));
    QVERIFY(back->isEnabled());
}

void MainWindowTest::mockVerticalSliceCreatesBoundPackageAndActualHistory()
{
    QTemporaryDir resultRoot;
    QVERIFY(resultRoot.isValid());
    QString resultRootPath =
        qEnvironmentVariable("MOCK_CLOSURE_EVIDENCE_ROOT").trimmed();
    if (resultRootPath.isEmpty()) {
        resultRootPath = resultRoot.path();
    } else {
        QVERIFY(QDir().mkpath(resultRootPath));
        QCOMPARE(
            QDir(resultRootPath).entryList(
                QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot).size(),
            0);
    }
    ScopedEnvironment rootOverride(
        QByteArrayLiteral("SCENARIO_NMR_MOCK_RESULT_ROOT"),
        QFile::encodeName(resultRootPath));

    QFile styleFile(QStringLiteral(":/app.qss"));
    QVERIFY(styleFile.open(QIODevice::ReadOnly | QIODevice::Text));
    qApp->setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    MainWindow window;
    window.resize(1586, 992);
    window.show();
    QTest::qWait(100);
    auto* currentStep =
        window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    QVERIFY(currentStep);

    const QString screenshotDirectory =
        qEnvironmentVariable("MOCK_CLOSURE_SCREENSHOT_DIR").trimmed();
    if (!screenshotDirectory.isEmpty())
        QVERIFY(QDir().mkpath(screenshotDirectory));
    const auto capture = [&window, &screenshotDirectory](
                             const QString& name) {
        if (screenshotDirectory.isEmpty())
            return true;
        QCoreApplication::processEvents();
        return window.grab().save(
            QDir(screenshotDirectory).filePath(name + QStringLiteral(".png")),
            "PNG");
    };

    QCOMPARE(currentStep->text(), QStringLiteral("01"));
    QVERIFY(capture(QStringLiteral("01-entry")));
    QTest::mouseClick(
        window.findChild<QPushButton*>(QStringLiteral("BeginResearchButton")),
        Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("02"));
    QVERIFY(capture(QStringLiteral("02-task-selection")));

    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("ShowRecommendedTemplateButton")),
        Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("03"));
    QVERIFY(capture(QStringLiteral("03-template-confirmation")));

    QTest::mouseClick(
        window.findChild<QPushButton*>(QStringLiteral("AcceptTemplateButton")),
        Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("04"));
    QVERIFY(capture(QStringLiteral("04-preparation")));

    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("SavePreparationButton")),
        Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("05"));
    QVERIFY(capture(QStringLiteral("05-protocol")));

    QTest::mouseClick(
        window.findChild<QPushButton*>(QStringLiteral("ProtocolUseOnceButton")),
        Qt::LeftButton);
    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("ContinueProtocolButton")),
        Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("06"));
    QVERIFY(capture(QStringLiteral("06-loc")));

    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("OpenLocalizationPlanningButton")),
        Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("07"));
    QVERIFY(capture(QStringLiteral("07-localization")));

    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("ConfirmLocalizationButton")),
        Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("08"));

    for (int index = 1; index <= 3; ++index) {
        QTest::mouseClick(
            window.findChild<QCheckBox*>(
                QStringLiteral("RunConfirmationCheck%1").arg(index)),
            Qt::LeftButton);
    }
    auto* acquire =
        window.findChild<QPushButton*>(QStringLiteral("MockAcquireButton"));
    QVERIFY(acquire);
    QVERIFY(acquire->isEnabled());
    QVERIFY(capture(QStringLiteral("08-run-confirmation-ready")));
    QTest::mouseClick(acquire, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("09"));
    QVERIFY(capture(QStringLiteral("09-mock-acquiring")));
    QTRY_COMPARE_WITH_TIMEOUT(currentStep->text(), QStringLiteral("10"), 4000);
    QVERIFY(capture(QStringLiteral("10-mock-processing")));

    auto* complete =
        window.findChild<QPushButton*>(QStringLiteral("CompleteMockProcessingButton"));
    QVERIFY(complete);
    QVERIFY(complete->isEnabled());
    QTest::mouseClick(complete, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("11"));
    auto* resultImage =
        window.findChild<QWidget*>(QStringLiteral("MockResultImage"));
    auto* snr = window.findChild<QLabel*>(QStringLiteral("MockQcSnrValue"));
    auto* uniformity =
        window.findChild<QLabel*>(QStringLiteral("MockQcUniformityValue"));
    auto* confirm =
        window.findChild<QPushButton*>(QStringLiteral("ConfirmResultButton"));
    QVERIFY(resultImage);
    QVERIFY(snr);
    QVERIFY(uniformity);
    QVERIFY(confirm);
    QVERIFY(resultImage->isVisibleTo(&window));
    QVERIFY(snr->text().contains(QStringLiteral("MOCK")));
    QVERIFY(uniformity->text().contains(QStringLiteral("MOCK")));
    QVERIFY(confirm->isEnabled());
    QVERIFY(capture(QStringLiteral("11-mock-result-qc")));
    QTest::mouseClick(confirm, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("12"));

    auto* save =
        window.findChild<QPushButton*>(QStringLiteral("SaveResultPackageButton"));
    auto* copy =
        window.findChild<QPushButton*>(QStringLiteral("CopyResultPathButton"));
    auto* history =
        window.findChild<QPushButton*>(QStringLiteral("OpenHistoryButton"));
    auto* saveState =
        window.findChild<QLabel*>(QStringLiteral("ResultPackageSaveState"));
    QVERIFY(save);
    QVERIFY(copy);
    QVERIFY(history);
    QVERIFY(saveState);
    QVERIFY(save->isEnabled());
    QTest::mouseClick(save, Qt::LeftButton);
    QVERIFY(saveState->text().contains(QStringLiteral("MOCK")));
    QVERIFY(copy->isEnabled());
    QVERIFY(history->isEnabled());
    QVERIFY(capture(QStringLiteral("12-result-package")));

    const QFileInfoList packages =
        QDir(resultRootPath).entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QCOMPARE(packages.size(), 1);
    const QString packagePath = packages.first().absoluteFilePath();
    const QString manifestPath =
        QDir(packagePath).filePath(QStringLiteral("manifest.json"));
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    const QJsonObject manifest =
        QJsonDocument::fromJson(manifestFile.readAll()).object();
    QCOMPARE(manifest.value(QStringLiteral("dataSource")).toString(),
             QStringLiteral("MOCK"));
    QVERIFY(!manifest.value(QStringLiteral("runId")).toString().isEmpty());
    QVERIFY(!manifest.value(QStringLiteral("snapshotId")).toString().isEmpty());
    QCOMPARE(
        QDir(packagePath).entryList(
            QDir::Files | QDir::NoDotAndDotDot, QDir::Name).size(),
        7);

    QTest::mouseClick(copy, Qt::LeftButton);
    QCOMPARE(QApplication::clipboard()->text(),
             QDir::toNativeSeparators(packagePath));
    QTest::mouseClick(history, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("13"));
    auto* table =
        window.findChild<QTableWidget*>(QStringLiteral("HistoryReadOnlyTable"));
    auto* preview =
        window.findChild<QWidget*>(QStringLiteral("HistoryPreviewImage"));
    QVERIFY(table);
    QVERIFY(preview);
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->text(),
             manifest.value(QStringLiteral("runId")).toString());
    QVERIFY(table->isEnabled());
    QVERIFY(preview->isVisibleTo(&window));
    QVERIFY(capture(QStringLiteral("13-history")));
    auto* backToResults =
        window.findChild<QPushButton*>(QStringLiteral("BackToResultsButton"));
    QVERIFY(backToResults);
    QVERIFY(backToResults->isEnabled());
    QTest::mouseClick(backToResults, Qt::LeftButton);
    QCOMPARE(currentStep->text(), QStringLiteral("12"));
    QVERIFY(capture(QStringLiteral("12-returned-from-history")));
    QCOMPARE(window.deviceSessionState(), MriSdkSessionState::Unloaded);
}

void MainWindowTest::mockSnapshotBindsVisibleProtocolAndPlanning()
{
    QTemporaryDir resultRoot;
    QVERIFY(resultRoot.isValid());
    ScopedEnvironment rootOverride(
        QByteArrayLiteral("SCENARIO_NMR_MOCK_RESULT_ROOT"),
        QFile::encodeName(resultRoot.path()));

    MainWindow window;
    window.show();
    window.setMockWorkflowStep(3);
    auto* addComparison =
        window.findChild<QPushButton*>(QStringLiteral("AddComparisonButton"));
    QVERIFY(addComparison);
    QVERIFY(addComparison->isEnabled());
    addComparison->click();

    window.setMockWorkflowStep(4);
    window.findChild<QPushButton*>(
        QStringLiteral("SavePreparationButton"))->click();
    auto* sliceGap =
        window.findChild<QLineEdit*>(QStringLiteral("ProtocolL2Current3"));
    auto* nex =
        window.findChild<QLineEdit*>(QStringLiteral("ProtocolL2Current4"));
    QVERIFY(sliceGap);
    QVERIFY(nex);
    sliceGap->setText(QStringLiteral("2 mm"));
    nex->setText(QStringLiteral("3"));
    window.findChild<QPushButton*>(
        QStringLiteral("ProtocolUseOnceButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("ContinueProtocolButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("OpenLocalizationPlanningButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("ReadPhaseSwapButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("AutoPlanningButton"))->click();
    auto* imagingTarget =
        window.findChild<QComboBox*>(
            QStringLiteral("ImagingTargetCombo"));
    QVERIFY(imagingTarget);
    imagingTarget->setCurrentIndex(1);
    window.findChild<QPushButton*>(
        QStringLiteral("ModifyImagingTargetButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("ConfirmLocalizationButton"))->click();

    for (int index = 1; index <= 3; ++index) {
        window.findChild<QCheckBox*>(
            QStringLiteral("RunConfirmationCheck%1").arg(index))->click();
    }
    auto* acquire =
        window.findChild<QPushButton*>(QStringLiteral("MockAcquireButton"));
    QVERIFY(acquire);
    QVERIFY(acquire->isEnabled());
    acquire->click();
    auto* currentStep =
        window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"));
    QVERIFY(currentStep);
    QTRY_COMPARE_WITH_TIMEOUT(currentStep->text(), QStringLiteral("10"), 4000);
    window.findChild<QPushButton*>(
        QStringLiteral("CompleteMockProcessingButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("ConfirmResultButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("SaveResultPackageButton"))->click();

    const QFileInfoList packages =
        QDir(resultRoot.path()).entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QCOMPARE(packages.size(), 1);
    QFile snapshotFile(
        QDir(packages.first().absoluteFilePath())
            .filePath(QStringLiteral("parameter-snapshot.json")));
    QVERIFY(snapshotFile.open(QIODevice::ReadOnly));
    const QJsonObject snapshot =
        QJsonDocument::fromJson(snapshotFile.readAll()).object();
    const QJsonArray protocolChain =
        snapshot.value(QStringLiteral("protocolChain")).toArray();
    QVERIFY(protocolChain.contains(QStringLiteral("FSE B")));
    QCOMPARE(snapshot.value(QStringLiteral("sliceGapMm")).toDouble(), 2.0);
    QCOMPARE(snapshot.value(QStringLiteral("nex")).toInt(), 3);
    QVERIFY(snapshot.value(QStringLiteral("readPhaseSwapped")).toBool());
    QVERIFY(snapshot.value(QStringLiteral("planningCoverageModified")).toBool());
    QVERIFY(snapshot.value(QStringLiteral("coverageWidth")).toDouble() > 0.0);
    QVERIFY(snapshot.value(QStringLiteral("coverageHeight")).toDouble() > 0.0);
    QCOMPARE(snapshot.value(QStringLiteral("imagingTarget")).toString(),
             QStringLiteral("结构细节"));
    QCOMPARE(snapshot.value(QStringLiteral("orientation")).toString(),
             QStringLiteral("横断"));
}

void MainWindowTest::cancelledMockRunKeepsEvidenceAndCreatesNoSuccessfulArtifacts()
{
    QTemporaryDir resultRoot;
    QVERIFY(resultRoot.isValid());
    ScopedEnvironment rootOverride(
        QByteArrayLiteral("SCENARIO_NMR_MOCK_RESULT_ROOT"),
        QFile::encodeName(resultRoot.path()));

    MainWindow window;
    window.show();
    window.setMockWorkflowStep(4);
    window.findChild<QPushButton*>(
        QStringLiteral("SavePreparationButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("ProtocolUseOnceButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("ContinueProtocolButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("OpenLocalizationPlanningButton"))->click();
    window.findChild<QPushButton*>(
        QStringLiteral("ConfirmLocalizationButton"))->click();
    for (int index = 1; index <= 3; ++index) {
        window.findChild<QCheckBox*>(
            QStringLiteral("RunConfirmationCheck%1").arg(index))->click();
    }
    window.findChild<QPushButton*>(
        QStringLiteral("MockAcquireButton"))->click();
    auto* stop =
        window.findChild<QPushButton*>(QStringLiteral("LeftMockStopButton"));
    auto* feedback =
        window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    auto* resultImage =
        window.findChild<QWidget*>(QStringLiteral("MockResultImage"));
    QVERIFY(stop);
    QVERIFY(feedback);
    QVERIFY(resultImage);
    QVERIFY(stop->isEnabled());
    stop->click();

    QVERIFY(feedback->text().contains(QStringLiteral("已取消")));
    QVERIFY(feedback->text().contains(QStringLiteral("RUN-MOCK-")));
    QVERIFY(!resultImage->isVisible());
    QCOMPARE(
        QDir(resultRoot.path()).entryList(
            QDir::Dirs | QDir::NoDotAndDotDot).size(),
        0);
    QCOMPARE(window.deviceSessionState(), MriSdkSessionState::Unloaded);
}

void MainWindowTest::captureLocalizationPlanningEvidenceWhenRequested()
{
    const QString outputDirectory =
        qEnvironmentVariable("LOCALIZATION_QA_CAPTURE_DIR").trimmed();
    if (outputDirectory.isEmpty()) {
        QSKIP("LOCALIZATION_QA_CAPTURE_DIR is not set");
    }
    QVERIFY(QDir().mkpath(outputDirectory));
    QFile styleFile(QStringLiteral(":/app.qss"));
    QVERIFY(styleFile.open(QIODevice::ReadOnly | QIODevice::Text));
    qApp->setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    MainWindow window;
    window.resize(1586, 992);
    window.show();
    QTest::qWait(80);
    const auto capture =
        [&window, &outputDirectory](const QString& name) {
        QCoreApplication::processEvents();
        QTest::qWait(60);
        const QString target =
            QDir(outputDirectory).filePath(name + QStringLiteral(".png"));
        return window.grab().save(target, "PNG");
    };

    window.setMockWorkflowStep(6);
    QVERIFY(capture(QStringLiteral("01-loc-static-reference")));
    auto* openPlanning =
        window.findChild<QPushButton*>(
            QStringLiteral("OpenLocalizationPlanningButton"));
    QVERIFY(openPlanning);
    QTest::mouseClick(openPlanning, Qt::LeftButton);
    QVERIFY(capture(QStringLiteral("02-planning-default")));

    auto* thickness =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral("SliceThicknessSpinBox"));
    auto* gap =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral("SliceGapSpinBox"));
    auto* count =
        window.findChild<QSpinBox*>(
            QStringLiteral("SliceCountSpinBox"));
    auto* position =
        window.findChild<QSlider*>(
            QStringLiteral("SlicePositionSlider"));
    auto* planner =
        window.findChild<QWidget*>(
            QStringLiteral("LocalizationPlannerView"));
    auto* coronal =
        window.findChild<QPushButton*>(
            QStringLiteral("LocalizationThumbnailCoronal"));
    auto* axial =
        window.findChild<QPushButton*>(
            QStringLiteral("LocalizationThumbnailAxial"));
    auto* center =
        window.findChild<QPushButton*>(
            QStringLiteral("CenterPlanningButton"));
    QVERIFY(thickness);
    QVERIFY(gap);
    QVERIFY(count);
    QVERIFY(position);
    QVERIFY(planner);
    QVERIFY(coronal);
    QVERIFY(axial);
    QVERIFY(center);

    thickness->setValue(4.2);
    gap->setValue(1.0);
    count->setValue(9);
    position->setValue(70);
    QVERIFY(capture(QStringLiteral("03-slice-parameters-adjusted")));

    QTest::mouseClick(coronal, Qt::LeftButton);
    QVERIFY(capture(QStringLiteral("04-coronal-thumbnail-selected")));
    QTest::mouseClick(axial, Qt::LeftButton);
    const QPoint dragStart(
        planner->width() / 2, planner->height() / 2);
    const QPoint dragEnd(
        planner->width() * 3 / 5, planner->height() * 2 / 5);
    QTest::mousePress(
        planner, Qt::LeftButton, Qt::NoModifier, dragStart);
    QTest::mouseMove(planner, dragEnd, 10);
    QTest::mouseRelease(
        planner, Qt::LeftButton, Qt::NoModifier, dragEnd);
    QVERIFY(capture(QStringLiteral("05-center-dragged")));

    QTest::mouseClick(center, Qt::LeftButton);
    QVERIFY(capture(QStringLiteral("06-center-reset")));
    QTest::mouseClick(
        window.findChild<QPushButton*>(
            QStringLiteral("ConfirmLocalizationButton")),
        Qt::LeftButton);
    QVERIFY(capture(
        QStringLiteral("07-run-confirmation-snapshot")));
    QCOMPARE(window.deviceSessionState(), MriSdkSessionState::Unloaded);
}

void MainWindowTest::captureInteractionQaScreensWhenRequested()
{
    const QString outputDirectory =
        qEnvironmentVariable("INTERACTION_QA_CAPTURE_DIR").trimmed();
    if (outputDirectory.isEmpty()) {
        QSKIP("INTERACTION_QA_CAPTURE_DIR is not set");
    }
    QVERIFY(QDir().mkpath(outputDirectory));
    QFile styleFile(QStringLiteral(":/app.qss"));
    QVERIFY(styleFile.open(QIODevice::ReadOnly | QIODevice::Text));
    qApp->setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    MainWindow window;
    window.resize(1586, 992);
    window.show();
    for (int step = 1; step <= 13; ++step) {
        window.setMockWorkflowStep(step);
        QTest::qWait(50);
        const QString target =
            QDir(outputDirectory).filePath(QStringLiteral("interaction-%1.png")
                                                .arg(step, 2, 10, QLatin1Char('0')));
        QVERIFY2(window.grab().save(target, "PNG"), qPrintable(target));
    }
}

void MainWindowTest::galleryLayoutUsesStableThreeColumnsAndContextPanels()
{
    MainWindow window;

    QVERIFY(window.findChild<QWidget*>(QStringLiteral("HeaderBar")) == nullptr);
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("FooterBar")) == nullptr);

    auto* centerStack = window.findChild<QStackedWidget*>(QStringLiteral("WorkflowPageStack"));
    auto* rightStack = window.findChild<QStackedWidget*>(QStringLiteral("WorkflowRightStack"));
    QVERIFY(centerStack);
    QVERIFY(rightStack);
    QCOMPARE(centerStack->count(), 13);
    QCOMPARE(rightStack->count(), 13);

    for (int step = 1; step <= 13; ++step) {
        window.setMockWorkflowStep(step);
        QCOMPARE(centerStack->currentIndex(), step - 1);
        QCOMPARE(rightStack->currentIndex(), step - 1);
        QVERIFY(window.findChild<QWidget*>(
            QStringLiteral("WorkflowPage%1").arg(step, 2, 10, QLatin1Char('0'))));
        QVERIFY(window.findChild<QWidget*>(
            QStringLiteral("RightPage%1").arg(step, 2, 10, QLatin1Char('0'))));
    }

    auto* level2 = window.findChild<QTableWidget*>(QStringLiteral("ProtocolLevel2Table"));
    QVERIFY(level2);
    QCOMPARE(level2->rowCount(), 5);
    QCOMPARE(level2->columnCount(), 4);
    QCOMPARE(level2->selectionMode(), QAbstractItemView::NoSelection);
    QCOMPARE(level2->horizontalHeaderItem(0)->text(), QStringLiteral("参数"));
    QCOMPARE(level2->horizontalHeaderItem(2)->text(), QStringLiteral("当前值（可编辑）"));
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("ShowL3Button")));
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("LocalizationThumbnailAxial")));
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("LocalizationThumbnailCoronal")));
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("LocalizationThumbnailSagittal")));
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("MockLocThumbnailAxial")));
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("ResultLocThumbnail")));
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("PrimaryTemplateRecommendation")));
    QVERIFY(window.findChild<QLabel*>(QStringLiteral("RunConfirmationChecks")));

    auto* processingSteps =
        window.findChild<QTableWidget*>(QStringLiteral("MockProcessingSteps"));
    QVERIFY(processingSteps);
    QCOMPARE(processingSteps->rowCount(), 5);
    QCOMPARE(processingSteps->columnCount(), 2);
    QVERIFY(processingSteps->item(0, 0)->text().contains(QStringLiteral("谱仪原生采集输出")));
    QVERIFY(processingSteps->item(2, 1)->text().contains(QStringLiteral("尚未执行")));
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("SampleInfoCard")));
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("PreparationPrecheckCard")));
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("PreparationBackButton")));
    QVERIFY(window.findChild<QTableWidget*>(QStringLiteral("RunConfirmationTable")));
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("RunConfirmationBackButton")));
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("ReturnToLocalizationButton")));

    const auto packageItems =
        window.findChildren<QWidget*>(QStringLiteral("ResultPackageItem"));
    QCOMPARE(packageItems.size(), 6);
    const QStringList expectedPackageItems = {
        QStringLiteral("原始数据"), QStringLiteral("标准结果"),
        QStringLiteral("QC记录"), QStringLiteral("协议与参数快照"),
        QStringLiteral("来源记录"), QStringLiteral("任务说明")
    };
    QStringList actualPackageItems;
    for (QWidget* item : packageItems)
        actualPackageItems.append(item->property("resultItemName").toString());
    QCOMPARE(actualPackageItems, expectedPackageItems);
    QVERIFY(window.findChild<QLabel*>(QStringLiteral("ResultPackageMetadata")));

    auto* history = window.findChild<QTableWidget*>(QStringLiteral("HistoryReadOnlyTable"));
    QVERIFY(history);
    QCOMPARE(history->horizontalHeaderItem(0)->text(), QStringLiteral("运行ID"));
    QCOMPARE(history->horizontalHeaderItem(2)->text(), QStringLiteral("样品ID"));
    QCOMPARE(history->rowCount(), 0);
    QVERIFY(!history->isEnabled());
    QVERIFY(history->toolTip().contains(QStringLiteral("尚无已封存")));
    QVERIFY(window.findChild<QComboBox*>(QStringLiteral("HistorySampleFilter")));
    QVERIFY(window.findChild<QComboBox*>(QStringLiteral("HistoryTemplateFilter")));
    QVERIFY(window.findChild<QComboBox*>(QStringLiteral("HistoryDateFilter")));
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("HistoryCompareButton")));
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("HistorySourceButton")));
    QVERIFY(window.findChild<QLabel*>(QStringLiteral("HistoryReadOnlyNote")));

    QVERIFY(window.findChild<QLabel*>(QStringLiteral("MockImageEvidenceLabel")));
    QVERIFY(window.findChild<QLabel*>(QStringLiteral("RawContractWarningLabel")));
    QVERIFY(!window.findChild<QLabel*>(QStringLiteral("RawContractWarningLabel"))
                 ->text()
                 .contains(QStringLiteral("已验证 k-space")));
    auto* leftMockStart = window.findChild<QPushButton*>(QStringLiteral("LeftMockStartButton"));
    QVERIFY(leftMockStart);
    window.setMockWorkflowStep(1);
    QVERIFY(!leftMockStart->isEnabled());
    window.setMockWorkflowStep(6);
    QVERIFY(!leftMockStart->isEnabled());
    QVERIFY(advanceToMockRunConfirmation(window));
    QVERIFY(!leftMockStart->isEnabled());
    for (int index = 1; index <= 3; ++index) {
        window.findChild<QCheckBox*>(
            QStringLiteral("RunConfirmationCheck%1").arg(index))->click();
    }
    QVERIFY(leftMockStart->isEnabled());
    leftMockStart->click();
    QCOMPARE(centerStack->currentIndex(), 8);

    window.setMockWorkflowStep(5);
    auto* autoResult = window.findChild<QLabel*>(QStringLiteral("ProtocolAutoResultValue"));
    auto* showL3 = window.findChild<QPushButton*>(QStringLiteral("ShowL3Button"));
    auto* l3Details = window.findChild<QLabel*>(QStringLiteral("L3DetailsLabel"));
    QVERIFY(autoResult);
    QVERIFY(showL3);
    QVERIFY(l3Details);
    auto* fovEditor = window.findChild<QLineEdit*>(QStringLiteral("ProtocolL2Current0"));
    QVERIFY(fovEditor);
    fovEditor->setText(QStringLiteral("48×48 mm"));
    QVERIFY(autoResult->text().contains(QStringLiteral("0.38")));
    QVERIFY(l3Details->isHidden());
    showL3->click();
    QVERIFY(!l3Details->isHidden());

    window.setMockWorkflowStep(7);
    auto* planner = window.findChild<QWidget*>(QStringLiteral("LocalizationPlannerView"));
    auto* coronal = window.findChild<QPushButton*>(QStringLiteral("OrientationCoronalButton"));
    auto* autoPlanning = window.findChild<QPushButton*>(QStringLiteral("AutoPlanningButton"));
    auto* resetPlanning = window.findChild<QPushButton*>(QStringLiteral("ResetPlanningButton"));
    QVERIFY(planner);
    QVERIFY(coronal);
    QVERIFY(autoPlanning);
    QVERIFY(resetPlanning);
    coronal->click();
    QCOMPARE(planner->property("selectedOrientation").toString(), QStringLiteral("冠状"));
    autoPlanning->click();
    QVERIFY(planner->property("planningCoverageModified").toBool());
    resetPlanning->click();
    QVERIFY(!planner->property("planningCoverageModified").toBool());

    window.setMockWorkflowStep(12);
    auto* savePackage = window.findChild<QPushButton*>(QStringLiteral("SaveResultPackageButton"));
    auto* saveState = window.findChild<QLabel*>(QStringLiteral("ResultPackageSaveState"));
    QVERIFY(savePackage);
    QVERIFY(saveState);
    QVERIFY(!savePackage->isEnabled());
    QVERIFY(saveState->text().contains(QStringLiteral("尚无已封存")));

    window.setMockWorkflowStep(13);
    auto* sampleFilter = window.findChild<QComboBox*>(QStringLiteral("HistorySampleFilter"));
    auto* selectionSummary = window.findChild<QLabel*>(QStringLiteral("HistorySelectionSummary"));
    auto* sourceButton = window.findChild<QPushButton*>(QStringLiteral("HistorySourceButton"));
    auto* actionState = window.findChild<QLabel*>(QStringLiteral("HistoryActionState"));
    QVERIFY(sampleFilter);
    QVERIFY(selectionSummary);
    QVERIFY(sourceButton);
    QVERIFY(actionState);
    QVERIFY(!sampleFilter->isEnabled());
    QCOMPARE(history->rowCount(), 0);
    QVERIFY(selectionSummary->text().contains(QStringLiteral("尚无已封存")));
    QVERIFY(!sourceButton->isEnabled());
    QVERIFY(actionState->text().contains(QStringLiteral("尚无已封存")));

    window.setMockWorkflowStep(9);
    auto* mockStop = window.findChild<QPushButton*>(QStringLiteral("LeftMockStopButton"));
    QVERIFY(mockStop);
    QVERIFY(!mockStop->isEnabled());
    QCOMPARE(window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"))->text(),
             QStringLiteral("09"));
}

void MainWindowTest::preparationGuidanceRemainsACompactSingleLineNotice()
{
    MainWindow window;
    window.setMockWorkflowStep(4);

    QLabel* guidance = nullptr;
    const auto labels = window.findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (label->text().contains(QStringLiteral("70 mm"))) {
            guidance = label;
            break;
        }
    }

    QVERIFY(guidance);
    QVERIFY2(guidance->maximumHeight() <= 48,
             "The preparation guidance must not expand into a tall empty card.");
}

void MainWindowTest::mockAcquisitionShowsRunningInsteadOfStartState()
{
    MainWindow window;
    auto* mockStart = window.findChild<QPushButton*>(QStringLiteral("LeftMockStartButton"));
    QVERIFY(mockStart);

    window.setMockWorkflowStep(9);

    QVERIFY(!mockStart->text().contains(QStringLiteral("运行中")));
    QVERIFY(mockStart->text().contains(QStringLiteral("待确认")));
    QVERIFY2(!mockStart->isEnabled(),
             "An in-progress Mock acquisition must not expose a second start action.");
}

void MainWindowTest::runConfirmationSnapshotKeepsChecksGrouped()
{
    MainWindow window;
    window.setMockWorkflowStep(8);

    QLabel* snapshot = nullptr;
    const auto labels = window.findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (label->text().contains(QStringLiteral("SNAPSHOT-PENDING"))) {
            snapshot = label;
            break;
        }
    }

    QVERIFY(snapshot);
    QVERIFY2(snapshot->maximumHeight() <= 48,
             "The parameter snapshot must remain a compact strip above its checks.");

    auto* confirmationTitle =
        window.findChild<QLabel*>(QStringLiteral("RunConfirmationChecks"));
    QVERIFY(confirmationTitle);
    QVERIFY2(confirmationTitle->maximumHeight() <= 36,
             "The confirmation heading must not consume the page's remaining height.");

    auto* page = window.findChild<QWidget*>(QStringLiteral("WorkflowPage08"));
    QVERIFY(page);
    auto* pageTitle = page->findChild<QLabel*>(
        QStringLiteral("WorkflowBodyLabel"), Qt::FindDirectChildrenOnly);
    QVERIFY(pageTitle);
    QVERIFY(pageTitle->minimumHeight() >= 80);
    QVERIFY(pageTitle->maximumHeight() <= 100);

    auto* firstCheck =
        window.findChild<QCheckBox*>(QStringLiteral("RunConfirmationCheck1"));
    QVERIFY(firstCheck);
    QVERIFY2(firstCheck->parentWidget()->inherits("QFrame"),
             "The three confirmation checks must remain grouped in a dedicated card.");
    QVERIFY(firstCheck->parentWidget()->minimumHeight() >= 60);
}

void MainWindowTest::operationalRightStatusUsesReadableRows()
{
    MainWindow window;

    for (int step = 6; step <= 9; ++step) {
        auto* rightPage = window.findChild<QWidget*>(
            QStringLiteral("RightPage%1").arg(step, 2, 10, QLatin1Char('0')));
        QVERIFY(rightPage);
        const auto cards = rightPage->findChildren<QWidget*>(
            QStringLiteral("WorkflowCard"), Qt::FindDirectChildrenOnly);
        QVERIFY(!rightPage->findChildren<QLabel*>().isEmpty());
        for (QWidget* card : cards) {
            QVERIFY2(card->minimumHeight() >= 80,
                      "Operational status rows must retain the reference hierarchy.");
        }
    }
}

QTEST_MAIN(MainWindowTest)
#include "test_main_window.moc"
