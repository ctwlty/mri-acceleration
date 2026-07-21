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
        calls = reinterpret_cast<const char* (*)()>(library.resolve("FakeCalls"));
        QVERIFY(reset);
        QVERIFY(setScanStatus);
        QVERIFY(setRawMode);
        QVERIFY(calls);
    }

    QLibrary library;
    void (*reset)() = nullptr;
    void (*setScanStatus)(int) = nullptr;
    void (*setRawMode)(int) = nullptr;
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
    writeFile(config.initPath);
    writeFile(config.parameterPath);
    return config;
}
}

class DeviceBridgeTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void runIsRejectedBeforeReady();
    void successfulScanTransitionsReadyScanningReady();
    void faultStatusAbortsAndTransitionsFault();
    void completedScanRequiresNewNonEmptyRawFile();
    void timeoutAbortsAndTransitionsFault();
};

void DeviceBridgeTest::initTestCase()
{
    qRegisterMetaType<MriSdkSessionState>();
}

void DeviceBridgeTest::runIsRejectedBeforeReady()
{
    DeviceBridge bridge;

    const MriSdkResult result = bridge.startScan();

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

    QVERIFY(bridge.startScan().ok);
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Scanning);
    fake.setScanStatus(3);
    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Ready);
    QVERIFY(QFileInfo(bridge.lastRawFile()).size() > 0);
    QVERIFY(states.count() >= 3);
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
    QVERIFY(bridge.startScan().ok);

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
    const MriSdkConfig config = createConfig(temp);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    QVERIFY(bridge.startScan().ok);

    fake.setScanStatus(3);
    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Fault);
    QCOMPARE(bridge.lastErrorResult().function, QStringLiteral("raw-verification"));
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
    QVERIFY(bridge.startScan().ok);
    QTest::qWait(5);

    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Fault);
    QVERIFY(QString::fromUtf8(fake.calls()).contains(QStringLiteral("Abort")));
    QCOMPARE(bridge.lastErrorResult().function, QStringLiteral("timeout"));
}

QTEST_MAIN(DeviceBridgeTest)
#include "test_device_bridge.moc"
