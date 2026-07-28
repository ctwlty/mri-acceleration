#include "app/MainWindow.h"
#include "app/EggControllerProcess.h"

#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
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
    void workflowUsesMockNavigationAndKeepsRealRunOnHold();
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
    QVERIFY(status->text().contains(QStringLiteral("已完成｜当前｜下一步")));
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

QTEST_MAIN(MainWindowTest)
#include "test_main_window.moc"
