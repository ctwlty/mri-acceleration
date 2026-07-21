#include "app/DeviceBridge.h"
#include "app/MriSdkTypes.h"

#include <QFile>
#include <QLibrary>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {
class FakeSdkControl {
public:
    explicit FakeSdkControl(const QString& path)
        : library(path)
    {
        QVERIFY2(library.load(), qPrintable(library.errorString()));
        reset = reinterpret_cast<void (*)()>(library.resolve("FakeReset"));
        setScanStatus = reinterpret_cast<void (*)(int)>(library.resolve("FakeSetScanStatus"));
        setRawMode = reinterpret_cast<void (*)(int)>(library.resolve("FakeSetRawMode"));
        writeRaw = reinterpret_cast<void (*)()>(library.resolve("FakeWriteRaw"));
        calls = reinterpret_cast<const char* (*)()>(library.resolve("FakeCalls"));
        QVERIFY(reset);
        QVERIFY(setScanStatus);
        QVERIFY(setRawMode);
        QVERIFY(writeRaw);
        QVERIFY(calls);
    }

    QLibrary library;
    void (*reset)() = nullptr;
    void (*setScanStatus)(int) = nullptr;
    void (*setRawMode)(int) = nullptr;
    void (*writeRaw)() = nullptr;
    const char* (*calls)() = nullptr;
};

void writeFile(const QString& path)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("test") > 0);
}

MriSdkConfig createConfig(QTemporaryDir& temp)
{
    MriSdkConfig config;
    config.initPath = temp.filePath(QStringLiteral("init.ini"));
    config.parameterPath = temp.filePath(QStringLiteral("PTScan.par"));
    config.outputPath = temp.path();
    config.pollIntervalMs = 60000;
    config.scanTimeoutMs = 60000;
    config.verifiedRuntimeAndParameterIdentity = true;
    writeFile(config.initPath);
    writeFile(config.parameterPath);
    return config;
}

void authorizeVerifiedBaseline(DeviceBridge& bridge)
{
    bridge.selectExecutionGate(ExecutionGate::VerifiedBaseline);
    QVERIFY(bridge.precheck().passed);
}
}

class DeviceBridgeTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void runIsRejectedBeforeReady();
    void successfulScanTransitionsReadyScanningReady();
    void initialIdleStatusWaitsForActiveScan();
    void faultStatusAbortsAndTransitionsFault();
    void completedScanRequiresNewNonEmptyRawFile();
    void overwrittenRawFileIsAccepted();
    void completedStatusWaitsForRawFile();
    void abortWaitsForStoppedStatus();
    void abortTimeoutWhileActiveTransitionsFault();
    void shutdownDoesNotRepeatAbort();
    void completionAtTimeoutBoundaryIsAccepted();
    void rawSettlingContinuesPastScanTimeout();
    void timeoutAbortsAndTransitionsFault();
    void verifiedBaselineRequiresFreshPrecheck();
    void precheckInvalidatesOnModeSceneAndRunChanges();
    void dryRunDoesNotWriteToSdk();
};

void DeviceBridgeTest::initTestCase()
{
    qRegisterMetaType<MriSdkSessionState>();
}

void DeviceBridgeTest::runIsRejectedBeforeReady()
{
    DeviceBridge bridge;

    const MriSdkResult result = bridge.startScan(bridge.executionContext());

    QVERIFY(!result.ok);
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Unloaded);
}

void DeviceBridgeTest::successfulScanTransitionsReadyScanningReady()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp);
    DeviceBridge bridge;
    QSignalSpy states(&bridge, &DeviceBridge::sessionStateChanged);
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);

    QVERIFY(bridge.startScan(bridge.executionContext()).ok);
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Scanning);
    fake.setScanStatus(3);
    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Ready);
    QVERIFY(QFileInfo(bridge.lastRawFile()).size() > 0);
    QVERIFY(states.count() >= 3);
}

void DeviceBridgeTest::initialIdleStatusWaitsForActiveScan()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);

    fake.setScanStatus(0);
    bridge.refreshStatus();
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Scanning);

    fake.setScanStatus(1);
    bridge.refreshStatus();
    fake.setScanStatus(0);
    bridge.refreshStatus();
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Ready);
    QVERIFY(QFileInfo(bridge.lastRawFile()).size() > 0);
}

void DeviceBridgeTest::faultStatusAbortsAndTransitionsFault()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);

    fake.setScanStatus(5);
    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Fault);
    QVERIFY(QString::fromUtf8(fake.calls()).contains(QStringLiteral("Abort")));
}

void DeviceBridgeTest::completedScanRequiresNewNonEmptyRawFile()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    fake.setRawMode(0);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriSdkConfig config = createConfig(temp);
    config.rawSettleTimeoutMs = 1;
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);

    fake.setScanStatus(3);
    bridge.refreshStatus();
    QTest::qWait(5);
    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Fault);
    QCOMPARE(bridge.lastErrorResult().function, QStringLiteral("raw-verification"));
}

void DeviceBridgeTest::completedStatusWaitsForRawFile()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    fake.setRawMode(0);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);

    fake.setScanStatus(3);
    bridge.refreshStatus();
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Scanning);

    fake.writeRaw();
    bridge.refreshStatus();
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Ready);
    QVERIFY(QFileInfo(bridge.lastRawFile()).size() > 0);
}

void DeviceBridgeTest::abortWaitsForStoppedStatus()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);

    fake.setScanStatus(1);
    bridge.abortScan();
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Stopping);
    bridge.refreshStatus();
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Stopping);

    fake.setScanStatus(0);
    bridge.refreshStatus();
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Ready);
    QCOMPARE(QString::fromUtf8(fake.calls()).count(QStringLiteral("Abort")), 1);
}

void DeviceBridgeTest::abortTimeoutWhileActiveTransitionsFault()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriSdkConfig config = createConfig(temp);
    config.stopTimeoutMs = 1;
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);
    fake.setScanStatus(1);
    bridge.abortScan();
    QTest::qWait(5);

    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Fault);
    QCOMPARE(bridge.lastErrorResult().function, QStringLiteral("timeout"));
}

void DeviceBridgeTest::shutdownDoesNotRepeatAbort()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp);
    {
        DeviceBridge bridge;
        QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);
        fake.setScanStatus(1);
        bridge.abortScan();
        QCOMPARE(bridge.sessionState(), MriSdkSessionState::Stopping);
    }

    const QString calls = QString::fromUtf8(fake.calls());
    QCOMPARE(calls.count(QStringLiteral("Abort")), 1);
    QCOMPARE(calls.count(QStringLiteral("CloseSys")), 1);
}

void DeviceBridgeTest::completionAtTimeoutBoundaryIsAccepted()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriSdkConfig config = createConfig(temp);
    config.scanTimeoutMs = 1;
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);
    fake.setScanStatus(1);
    bridge.refreshStatus();
    QTest::qWait(5);

    fake.setScanStatus(0);
    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Ready);
    QVERIFY(QFileInfo(bridge.lastRawFile()).size() > 0);
}

void DeviceBridgeTest::rawSettlingContinuesPastScanTimeout()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    fake.setRawMode(0);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriSdkConfig config = createConfig(temp);
    config.scanTimeoutMs = 1;
    config.rawSettleTimeoutMs = 1000;
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);
    fake.setScanStatus(1);
    bridge.refreshStatus();
    QTest::qWait(5);

    fake.setScanStatus(3);
    bridge.refreshStatus();
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Scanning);
    fake.writeRaw();
    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Ready);
    QVERIFY(QFileInfo(bridge.lastRawFile()).size() > 0);
}

void DeviceBridgeTest::overwrittenRawFileIsAccepted()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp);
    const QString fixedRaw = temp.filePath(QStringLiteral("PTMRIData_fake.raw"));
    writeFile(fixedRaw);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);

    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);
    fake.setScanStatus(3);
    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Ready);
    QCOMPARE(QFileInfo(bridge.lastRawFile()).absoluteFilePath(), QFileInfo(fixedRaw).absoluteFilePath());
    QVERIFY(QFileInfo(fixedRaw).size() > 4);
}

void DeviceBridgeTest::timeoutAbortsAndTransitionsFault()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriSdkConfig config = createConfig(temp);
    config.scanTimeoutMs = 1;
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);
    QTest::qWait(5);

    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Fault);
    QVERIFY(QString::fromUtf8(fake.calls()).contains(QStringLiteral("Abort")));
    QCOMPARE(bridge.lastErrorResult().function, QStringLiteral("timeout"));
}

void DeviceBridgeTest::verifiedBaselineRequiresFreshPrecheck()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    bridge.selectExecutionGate(ExecutionGate::VerifiedBaseline);

    QVERIFY(!bridge.startScan(bridge.executionContext()).ok);
    QVERIFY(bridge.precheck().passed);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);
}

void DeviceBridgeTest::precheckInvalidatesOnModeSceneAndRunChanges()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.precheckResult().passed);
    MriSdkConfig changedConfig = config;
    changedConfig.outputPrefix = "changed";
    QVERIFY(!bridge.connectDevice(changedConfig).ok);
    QVERIFY(!bridge.precheckResult().passed);
    authorizeVerifiedBaseline(bridge);
    bridge.invalidatePrecheck(QStringLiteral("scene changed"));
    QVERIFY(!bridge.precheckResult().passed);
    authorizeVerifiedBaseline(bridge);
    bridge.selectExecutionGate(ExecutionGate::Hold);
    QVERIFY(!bridge.precheckResult().passed);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.executionContext()).ok);
    QVERIFY(!bridge.precheckResult().passed);
}

void DeviceBridgeTest::dryRunDoesNotWriteToSdk()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    DeviceBridge bridge;
    SceneTemplate scene;
    scene.name = QStringLiteral("HOLD scene");
    scene.executionGate = ExecutionGate::Hold;
    bridge.dryRunScene(scene);

    QVERIFY(QString::fromUtf8(fake.calls()).isEmpty());
}

QTEST_MAIN(DeviceBridgeTest)
#include "test_device_bridge.moc"
