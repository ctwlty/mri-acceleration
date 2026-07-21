#include "app/MainWindow.h"

#include <QFile>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

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
    void selectingScientificSceneForcesRunHoldMode();
    void reconnectResetsBaselineVisualsToHold();
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

void MainWindowTest::selectingScientificSceneForcesRunHoldMode()
{
    MainWindow window;
    auto* gate = window.findChild<QComboBox*>(QStringLiteral("ExecutionGateCombo"));
    auto* scenes = window.findChild<QListWidget*>(QStringLiteral("TemplateList"));
    QVERIFY(gate);
    QVERIFY(scenes);
    QVERIFY(scenes->count() > 0);
    gate->setCurrentIndex(gate->findData(static_cast<int>(ExecutionGate::VerifiedBaseline)));
    QCOMPARE(static_cast<ExecutionGate>(gate->currentData().toInt()), ExecutionGate::VerifiedBaseline);

    scenes->setCurrentRow(-1);
    scenes->setCurrentRow(0);

    QCOMPARE(static_cast<ExecutionGate>(gate->currentData().toInt()), ExecutionGate::Hold);
}

void MainWindowTest::reconnectResetsBaselineVisualsToHold()
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
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    QVERIFY2(window.loadSdkAndConnect(sdkPath, config).ok, "initial connection failed");

    auto* gate = window.findChild<QComboBox*>(QStringLiteral("ExecutionGateCombo"));
    QVERIFY(gate);
    gate->setCurrentIndex(gate->findData(static_cast<int>(ExecutionGate::VerifiedBaseline)));
    QCOMPARE(static_cast<ExecutionGate>(gate->currentData().toInt()), ExecutionGate::VerifiedBaseline);

    auto* bridge = window.findChild<DeviceBridge*>();
    QVERIFY(bridge);
    QVERIFY2(bridge->connectDevice(config).ok, "reconnection failed");

    QCOMPARE(static_cast<ExecutionGate>(gate->currentData().toInt()), ExecutionGate::Hold);
    const auto buttons = window.findChildren<QPushButton*>();
    const auto startIt = std::find_if(buttons.cbegin(), buttons.cend(), [](const QPushButton* button) {
        return button->text() == QStringLiteral("开始采集");
    });
    QVERIFY(startIt != buttons.cend());
    QVERIFY(!(*startIt)->isEnabled());
}

QTEST_MAIN(MainWindowTest)
#include "test_main_window.moc"
