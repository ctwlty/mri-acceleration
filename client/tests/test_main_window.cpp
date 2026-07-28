#include "app/MainWindow.h"
#include "app/EggControllerProcess.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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
}

class MainWindowTest : public QObject {
    Q_OBJECT

private slots:
    void sdkCanBeLoadedAndConnectedWithoutFileDialog();
    void galleryLayoutUsesStableThreeColumnsAndContextPanels();
    void workflowUsesMockNavigationAndKeepsRealRunOnHold();
    void preparationGuidanceRemainsACompactSingleLineNotice();
    void mockAcquisitionShowsRunningInsteadOfStartState();
    void runConfirmationSnapshotKeepsChecksGrouped();
    void operationalRightStatusUsesReadableRows();
};

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

#if 0 // Superseded by the Mock-only workflow contract.
void MainWindowTest::precheckViewportDisplaysOnlySdkReturnedStatus()
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
    QVERIFY(window.loadSdkAndConnect(qEnvironmentVariable("FAKE_MRI_SDK_PATH"), config).ok);
    auto* precheck = window.findChild<QLabel*>(QStringLiteral("PrecheckStatusLabel"));
    QVERIFY(precheck);
    QCOMPARE(precheck->text(), QStringLiteral("真实预检：待执行（未声明通过）"));

    QVERIFY(QMetaObject::invokeMethod(&window, "handlePrecheck", Qt::DirectConnection));
    QCOMPARE(precheck->text(), QStringLiteral("真实预检：连接码 1；温度 31.4 C；ScanStatus 0"));
}

void MainWindowTest::automationModeDisplaysBothImagesWithoutSdkRun()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString callLog = temp.filePath(QStringLiteral("sdk-calls.log"));
    qputenv("FAKE_CALL_LOG", QFile::encodeName(callLog));

    EggControllerLaunchConfig config;
    config.program = qEnvironmentVariable("FAKE_EGGCONTROLLER_PROXY_PATH");
    config.arguments = {
        QStringLiteral("--output"), temp.path(),
        QStringLiteral("--mode"), QStringLiteral("success")
    };
    config.workingDirectory = temp.path();

    MainWindow window;
    window.configureEggController(config);

    auto* mode = window.findChild<QComboBox*>(QStringLiteral("ControlModeCombo"));
    auto* start = window.findChild<QPushButton*>(QStringLiteral("StartButton"));
    auto* kspace = window.findChild<QLabel*>(QStringLiteral("KspaceImageView"));
    auto* finalImage = window.findChild<QLabel*>(QStringLiteral("FinalImageView"));
    auto* status = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    QVERIFY(mode);
    QVERIFY(start);
    QVERIFY(kspace);
    QVERIFY(finalImage);
    QVERIFY(status);
    QCOMPARE(mode->currentData().toString(), QStringLiteral("eggcontroller"));
    QVERIFY(start->isEnabled());

    start->click();

    QTRY_VERIFY_WITH_TIMEOUT(!kspace->pixmap().isNull(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!finalImage->pixmap().isNull(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(status->text(), QStringLiteral("Ready"), 3000);
    const QByteArray sdkCalls = [&]() {
        QFile file(callLog);
        if (!file.open(QIODevice::ReadOnly)) {
            return QByteArray{};
        }
        return file.readAll();
    }();
    QVERIFY(!QString::fromUtf8(sdkCalls).contains(QStringLiteral("Run")));

    qunsetenv("FAKE_CALL_LOG");
}

void MainWindowTest::automationRunLocksControlModeUntilProcessFinishes()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    EggControllerLaunchConfig config;
    config.program = qEnvironmentVariable("FAKE_EGGCONTROLLER_PROXY_PATH");
    config.arguments = {
        QStringLiteral("--output"), temp.path(),
        QStringLiteral("--mode"), QStringLiteral("slow")
    };
    config.workingDirectory = temp.path();

    MainWindow window;
    window.configureEggController(config);
    auto* mode = window.findChild<QComboBox*>(QStringLiteral("ControlModeCombo"));
    auto* start = window.findChild<QPushButton*>(QStringLiteral("StartButton"));
    auto* status = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    QVERIFY(mode);
    QVERIFY(start);
    QVERIFY(status);

    start->click();
    QTRY_VERIFY_WITH_TIMEOUT(!mode->isEnabled(), 200);
    QTRY_COMPARE_WITH_TIMEOUT(status->text(), QStringLiteral("Ready"), 3000);
    QVERIFY(mode->isEnabled());
}

void MainWindowTest::automationModeEvaluatesTheReturnedImageInExistingMetricCards()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    EggControllerLaunchConfig config;
    config.program = qEnvironmentVariable("FAKE_EGGCONTROLLER_PROXY_PATH");
    config.arguments = {
        QStringLiteral("--output"), temp.path(),
        QStringLiteral("--mode"), QStringLiteral("success")
    };
    config.workingDirectory = temp.path();

    MainWindow window;
    window.configureEggController(config);
    QCOMPARE(window.findChildren<QWidget*>(QStringLiteral("MetricCard")).size(), 4);

    auto* start = window.findChild<QPushButton*>(QStringLiteral("StartButton"));
    auto* status = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    auto* snr = window.findChild<QLabel*>(QStringLiteral("QualitySnrValue"));
    auto* uniformity = window.findChild<QLabel*>(QStringLiteral("QualityUniformityValue"));
    auto* size = window.findChild<QLabel*>(QStringLiteral("QualitySizeValue"));
    auto* stability = window.findChild<QLabel*>(QStringLiteral("QualityStabilityValue"));
    QVERIFY(start);
    QVERIFY(status);
    QVERIFY(snr);
    QVERIFY(uniformity);
    QVERIFY(size);
    QVERIFY(stability);

    start->click();

    QTRY_COMPARE_WITH_TIMEOUT(status->text(), QStringLiteral("Ready"), 3000);
    QVERIFY(QRegularExpression(QStringLiteral("^\\d+\\.\\d dB$")).match(snr->text()).hasMatch());
    QVERIFY(snr->text().chopped(3).toDouble() > 30.0);
    QVERIFY(QRegularExpression(QStringLiteral("^\\d+\\.\\d %$")).match(uniformity->text()).hasMatch());
    QVERIFY(uniformity->text().chopped(2).toDouble() > 95.0);
    QCOMPARE(size->text(), QStringLiteral("21 × 33 px"));
    QCOMPARE(stability->text(), QStringLiteral("不可评估（需重复）"));
    QCOMPARE(window.findChildren<QWidget*>(QStringLiteral("MetricCard")).size(), 4);
}

void MainWindowTest::automationRunPreventsWindowCloseUntilProcessFinishes()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    EggControllerLaunchConfig config;
    config.program = qEnvironmentVariable("FAKE_EGGCONTROLLER_PROXY_PATH");
    config.arguments = {
        QStringLiteral("--output"), temp.path(),
        QStringLiteral("--mode"), QStringLiteral("slow")
    };
    config.workingDirectory = temp.path();

    MainWindow window;
    window.configureEggController(config);
    window.show();
    auto* start = window.findChild<QPushButton*>(QStringLiteral("StartButton"));
    auto* status = window.findChild<QLabel*>(QStringLiteral("AutomationStatusLabel"));
    QVERIFY(start);
    QVERIFY(status);

    start->click();
    QTest::qWait(50);
    QVERIFY(!window.close());
    QVERIFY(window.isVisible());

    QTRY_COMPARE_WITH_TIMEOUT(status->text(), QStringLiteral("Ready"), 3000);
    QVERIFY(window.close());
}

#endif

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
    QVERIFY(historyTable->isEnabled());
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

    for (int i = 0; i < 2; ++i) {
        next->click();
    }
    QCOMPARE(currentStep->text(), QStringLiteral("03"));
    addComparison->click();
    QVERIFY(protocol->text().contains(QStringLiteral("FSE B")));

    for (int i = 0; i < 5; ++i) {
        next->click();
    }
    QCOMPARE(currentStep->text(), QStringLiteral("08"));
    QVERIFY(mockAcquire->isEnabled());
    mockAcquire->click();
    QCOMPARE(currentStep->text(), QStringLiteral("09"));

    for (int i = 0; i < 3; ++i) {
        next->click();
    }
    QCOMPARE(currentStep->text(), QStringLiteral("12"));
    openHistory->click();
    QCOMPARE(currentStep->text(), QStringLiteral("13"));
    backToResults->click();
    QCOMPARE(currentStep->text(), QStringLiteral("12"));
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
    QVERIFY(processingSteps->item(2, 1)->text().contains(QStringLiteral("72%")));
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
    QCOMPARE(history->horizontalHeaderItem(1)->text(), QStringLiteral("样品ID"));
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
    QVERIFY(leftMockStart->isEnabled());

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
    QVERIFY(autoResult->text().contains(QStringLiteral("FOV 48×48 mm")));
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
    savePackage->click();
    QVERIFY(!savePackage->isEnabled());
    QVERIFY(saveState->text().contains(QStringLiteral("未调用 SDK")));

    window.setMockWorkflowStep(13);
    auto* sampleFilter = window.findChild<QComboBox*>(QStringLiteral("HistorySampleFilter"));
    auto* selectionSummary = window.findChild<QLabel*>(QStringLiteral("HistorySelectionSummary"));
    auto* sourceButton = window.findChild<QPushButton*>(QStringLiteral("HistorySourceButton"));
    auto* actionState = window.findChild<QLabel*>(QStringLiteral("HistoryActionState"));
    QVERIFY(sampleFilter);
    QVERIFY(selectionSummary);
    QVERIFY(sourceButton);
    QVERIFY(actionState);
    sampleFilter->setCurrentText(QStringLiteral("SAMPLE-002"));
    QVERIFY(history->isRowHidden(0));
    QVERIFY(!history->isRowHidden(2));
    sampleFilter->setCurrentIndex(0);
    history->setCurrentCell(2, 0);
    QVERIFY(selectionSummary->text().contains(QStringLiteral("SAMPLE-002")));
    sourceButton->click();
    QVERIFY(actionState->text().contains(QStringLiteral("来源记录完整")));

    window.setMockWorkflowStep(9);
    auto* mockStop = window.findChild<QPushButton*>(QStringLiteral("LeftMockStopButton"));
    QVERIFY(mockStop);
    QVERIFY(mockStop->isEnabled());
    mockStop->click();
    QCOMPARE(window.findChild<QLabel*>(QStringLiteral("WorkflowCurrentStep"))->text(),
             QStringLiteral("08"));
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

    QVERIFY(mockStart->text().contains(QStringLiteral("运行中")));
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
        if (label->text().contains(QStringLiteral("RUN-PENDING-001"))) {
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
        QVERIFY(cards.size() >= 5);
        for (QWidget* card : cards) {
            QVERIFY2(card->minimumHeight() >= 80,
                     "Operational status rows must retain the reference hierarchy.");
        }
    }
}

QTEST_MAIN(MainWindowTest)
#include "test_main_window.moc"
