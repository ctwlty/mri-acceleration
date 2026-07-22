#include "app/MainWindow.h"
#include "app/EggControllerProcess.h"

#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QPushButton>
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
    void automationModeDisplaysBothImagesWithoutSdkRun();
    void automationModeEvaluatesTheReturnedImageInExistingMetricCards();
    void automationRunLocksControlModeUntilProcessFinishes();
    void automationRunPreventsWindowCloseUntilProcessFinishes();
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

QTEST_MAIN(MainWindowTest)
#include "test_main_window.moc"
