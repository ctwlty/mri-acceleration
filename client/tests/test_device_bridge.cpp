#include "app/DeviceBridge.h"
#include "app/MriRuntimeResolver.h"
#include "app/MriSdkTypes.h"

#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QLibrary>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <type_traits>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static_assert(!std::is_aggregate_v<BaselineIdentityProof>);
static_assert(!std::is_constructible_v<BaselineIdentityProof, quint64>);
static_assert(!std::is_aggregate_v<PrecheckTicket>);

namespace {
class FakeSdkControl {
public:
    explicit FakeSdkControl(const QString& path)
        : library(path)
    {
        library.load();
        m_reset = reinterpret_cast<void (*)()>(library.resolve("FakeReset"));
        m_setScanStatus = reinterpret_cast<void (*)(int)>(library.resolve("FakeSetScanStatus"));
        m_setConnectionStatus = reinterpret_cast<void (*)(int)>(library.resolve("FakeSetConnectionStatus"));
        m_setRawMode = reinterpret_cast<void (*)(int)>(library.resolve("FakeSetRawMode"));
        m_writeRaw = reinterpret_cast<void (*)()>(library.resolve("FakeWriteRaw"));
        m_calls = reinterpret_cast<const char* (*)()>(library.resolve("FakeCalls"));
    }

    bool isValid() const
    {
        return library.isLoaded() && m_reset && m_setScanStatus && m_setConnectionStatus
            && m_setRawMode && m_writeRaw && m_calls;
    }

    QString errorString() const
    {
        return library.isLoaded()
            ? QStringLiteral("fake MRI SDK is missing one or more control exports")
            : library.errorString();
    }

    void reset() { QVERIFY2(isValid(), qPrintable(errorString())); m_reset(); }
    void setScanStatus(int status) { QVERIFY2(isValid(), qPrintable(errorString())); m_setScanStatus(status); }
    void setConnectionStatus(int status) { QVERIFY2(isValid(), qPrintable(errorString())); m_setConnectionStatus(status); }
    void setRawMode(int mode) { QVERIFY2(isValid(), qPrintable(errorString())); m_setRawMode(mode); }
    void writeRaw() { QVERIFY2(isValid(), qPrintable(errorString())); m_writeRaw(); }
    const char* calls() const { return isValid() ? m_calls() : ""; }

private:
    QLibrary library;
    void (*m_reset)() = nullptr;
    void (*m_setScanStatus)(int) = nullptr;
    void (*m_setConnectionStatus)(int) = nullptr;
    void (*m_setRawMode)(int) = nullptr;
    void (*m_writeRaw)() = nullptr;
    const char* (*m_calls)() = nullptr;
};

void writeFile(const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("test") > 0);
}

QString sha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex().toUpper());
}

void markHidden(const QString& path)
{
#ifdef Q_OS_WIN
    QVERIFY(SetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_ATTRIBUTE_HIDDEN));
#else
    Q_UNUSED(path);
#endif
}

MriSdkConfig createConfig(QTemporaryDir& temp, const QString& sdkPath)
{
    MriSdkConfig config;
    config.initPath = temp.filePath(QStringLiteral("hw_cfg/init.ini"));
    config.parameterPath = temp.filePath(QStringLiteral("profiles/PTScan.par"));
    config.outputPath = temp.filePath(QStringLiteral("output"));
    config.pollIntervalMs = 60000;
    config.scanTimeoutMs = 60000;
    writeFile(config.initPath);
    writeFile(config.parameterPath);
    QDir().mkpath(config.outputPath);
    MriRuntimeExpectations expectations;
    expectations.dllSha256 = sha256(sdkPath);
    expectations.initSha256 = sha256(config.initPath);
    expectations.parameterSha256 = sha256(config.parameterPath);
    expectations.hwCfgFileCount = 1;
    expectations.hwCfgTotalBytes = QFileInfo(config.initPath).size();
    const QString record = QStringLiteral("init.ini|%1|%2")
                               .arg(expectations.hwCfgTotalBytes)
                               .arg(expectations.initSha256);
    expectations.hwCfgManifestSha256 = QString::fromLatin1(
        QCryptographicHash::hash(record.toUtf8(), QCryptographicHash::Sha256).toHex().toUpper());
    MriRuntimeOverrides overrides;
    overrides.sdkPath = sdkPath;
    overrides.initPath = config.initPath;
    overrides.parameterPath = config.parameterPath;
    overrides.outputPath = config.outputPath;
    config.identityProof = MriRuntimeResolver::resolveForTesting(temp.path(), overrides, expectations).identityProof;
    return config;
}

void authorizeVerifiedBaseline(DeviceBridge& bridge)
{
    bridge.selectVerifiedBaseline();
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
    void runRechecksBusyStatusBeforeSdkWrites();
    void staleTicketsNeverIssueAnotherRun();
    void ticketCannotCrossBridgeInstances();
    void terminalTransitionsInvalidateTicket();
    void proofCannotDetachFromResolvedPaths();
    void rejectsDllReplacementBeforeVerifiedLoad();
    void rejectsInitReplacementBeforePrecheck();
    void rejectsParameterReplacementBeforeRun();
    void rejectsHiddenHwCfgAdditionBeforeRun();
    void scientificSelectionCannotReuseBaselineTicket();
};

void DeviceBridgeTest::initTestCase()
{
    qRegisterMetaType<MriSdkSessionState>();
}

void DeviceBridgeTest::runIsRejectedBeforeReady()
{
    DeviceBridge bridge;

    const MriSdkResult result = bridge.startScan({});

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
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QSignalSpy states(&bridge, &DeviceBridge::sessionStateChanged);
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);

    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Scanning);
    fake.setScanStatus(3);
    bridge.refreshStatus();

    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Ready);
    QVERIFY(QFileInfo(bridge.lastRawFile()).size() > 0);
    QVERIFY(states.count() >= 3);
    QVERIFY(!QString::fromUtf8(fake.calls()).contains(QStringLiteral("Abort")));
}

void DeviceBridgeTest::initialIdleStatusWaitsForActiveScan()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);

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
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);

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
    MriSdkConfig config = createConfig(temp, sdkPath);
    config.rawSettleTimeoutMs = 1;
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);

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
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);

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
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);

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
    MriSdkConfig config = createConfig(temp, sdkPath);
    config.stopTimeoutMs = 1;
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);
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
    const MriSdkConfig config = createConfig(temp, sdkPath);
    {
        DeviceBridge bridge;
        QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);
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
    MriSdkConfig config = createConfig(temp, sdkPath);
    config.scanTimeoutMs = 1;
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);
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
    MriSdkConfig config = createConfig(temp, sdkPath);
    config.scanTimeoutMs = 1;
    config.rawSettleTimeoutMs = 1000;
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);
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
    const MriSdkConfig config = createConfig(temp, sdkPath);
    const QString fixedRaw = QDir(config.outputPath).filePath(QStringLiteral("PTMRIData_fake.raw"));
    writeFile(fixedRaw);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);

    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);
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
    MriSdkConfig config = createConfig(temp, sdkPath);
    config.scanTimeoutMs = 1;
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);
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
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    bridge.selectVerifiedBaseline();

    QVERIFY(!bridge.startScan({}).ok);
    QVERIFY(bridge.precheck().passed);
    const PrecheckTicket ticket = bridge.precheckTicket();
    QVERIFY(bridge.startScan(ticket).ok);
    QVERIFY(!bridge.startScan(ticket).ok);
}

void DeviceBridgeTest::precheckInvalidatesOnModeSceneAndRunChanges()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.precheckResult().passed);
    MriSdkConfig changedConfig = config;
    changedConfig.outputPrefix = "changed";
    const QString beforeReconfigure = QString::fromUtf8(fake.calls());
    QVERIFY(bridge.connectDevice(changedConfig).ok);
    QCOMPARE(QString::fromUtf8(fake.calls()).count(QStringLiteral("CloseSys")),
        beforeReconfigure.count(QStringLiteral("CloseSys")) + 1);
    QCOMPARE(QString::fromUtf8(fake.calls()).count(QStringLiteral("Abort")),
        beforeReconfigure.count(QStringLiteral("Abort")));
    QVERIFY(!bridge.precheckResult().passed);
    authorizeVerifiedBaseline(bridge);
    SceneTemplate scene;
    scene.name = QStringLiteral("scene-one");
    scene.executionGate = ExecutionGate::Hold;
    bridge.selectScientificScene(scene);
    QVERIFY(!bridge.precheckResult().passed);
    authorizeVerifiedBaseline(bridge);
    scene.name = QStringLiteral("scene-two");
    bridge.selectScientificScene(scene);
    QVERIFY(!bridge.precheckResult().passed);
    authorizeVerifiedBaseline(bridge);
    QVERIFY(bridge.startScan(bridge.precheckTicket()).ok);
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

void DeviceBridgeTest::runRechecksBusyStatusBeforeSdkWrites()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    const PrecheckTicket ticket = bridge.precheckTicket();
    fake.setScanStatus(1);

    QVERIFY(!bridge.startScan(ticket).ok);
    QVERIFY(!QString::fromUtf8(fake.calls()).contains(QStringLiteral("Run")));
    fake.setScanStatus(0);
    QVERIFY(!bridge.startScan(ticket).ok);
    QVERIFY(!QString::fromUtf8(fake.calls()).contains(QStringLiteral("Run")));
}

void DeviceBridgeTest::staleTicketsNeverIssueAnotherRun()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    auto expectNoRun = [&fake](const QString& before) {
        QCOMPARE(QString::fromUtf8(fake.calls()).count(QStringLiteral("Run")), before.count(QStringLiteral("Run")));
    };
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp, sdkPath);

    auto prepare = [&](DeviceBridge& bridge, const MriSdkConfig& selectedConfig) {
        if (!bridge.loadSdk(sdkPath).ok || !bridge.connectDevice(selectedConfig).ok) {
            return false;
        }
        bridge.selectVerifiedBaseline();
        return bridge.precheck().passed;
    };

    {
        fake.reset(); DeviceBridge bridge; QVERIFY(prepare(bridge, config));
        const PrecheckTicket ticket = bridge.precheckTicket();
        SceneTemplate scene; scene.name = QStringLiteral("mode-hold"); scene.executionGate = ExecutionGate::Hold;
        bridge.selectScientificScene(scene); const QString calls = QString::fromUtf8(fake.calls());
        QVERIFY(!bridge.startScan(ticket).ok); expectNoRun(calls);
    }
    {
        fake.reset(); DeviceBridge bridge; QVERIFY(prepare(bridge, config));
        const PrecheckTicket ticket = bridge.precheckTicket();
        SceneTemplate scene; scene.name = QStringLiteral("changed-scene"); scene.executionGate = ExecutionGate::Hold;
        bridge.selectScientificScene(scene); const QString calls = QString::fromUtf8(fake.calls());
        QVERIFY(!bridge.startScan(ticket).ok); expectNoRun(calls);
    }
    {
        fake.reset(); DeviceBridge bridge; QVERIFY(prepare(bridge, config));
        const PrecheckTicket ticket = bridge.precheckTicket();
        fake.setConnectionStatus(0); const QString calls = QString::fromUtf8(fake.calls());
        QVERIFY(!bridge.startScan(ticket).ok); expectNoRun(calls);
    }
    {
        fake.reset(); DeviceBridge bridge; QVERIFY(prepare(bridge, config));
        const PrecheckTicket ticket = bridge.precheckTicket();
        QVERIFY(bridge.loadSdk(sdkPath).ok); const QString calls = QString::fromUtf8(fake.calls());
        QVERIFY(!bridge.startScan(ticket).ok); expectNoRun(calls);
    }
    {
        fake.reset(); DeviceBridge bridge; QVERIFY(prepare(bridge, config));
        const PrecheckTicket ticket = bridge.precheckTicket();
        MriSdkConfig updated = config; updated.outputPrefix = "new-prefix";
        QVERIFY(bridge.connectDevice(updated).ok); const QString calls = QString::fromUtf8(fake.calls());
        QVERIFY(!bridge.startScan(ticket).ok); expectNoRun(calls);
    }
    {
        fake.reset(); DeviceBridge bridge; MriSdkConfig outputConfig = config;
        outputConfig.outputPath = temp.filePath(QStringLiteral("dedicated-output"));
        QVERIFY(QDir().mkpath(outputConfig.outputPath));
        QVERIFY(prepare(bridge, outputConfig)); const PrecheckTicket ticket = bridge.precheckTicket();
        QVERIFY(QDir(outputConfig.outputPath).removeRecursively());
        QFile outputFile(outputConfig.outputPath); QVERIFY(outputFile.open(QIODevice::WriteOnly)); outputFile.close();
        const QString calls = QString::fromUtf8(fake.calls());
        QVERIFY(!bridge.startScan(ticket).ok); expectNoRun(calls);
    }
}

void DeviceBridgeTest::ticketCannotCrossBridgeInstances()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge first;
    DeviceBridge second;
    QVERIFY(first.loadSdk(sdkPath).ok);
    QVERIFY(first.connectDevice(config).ok);
    authorizeVerifiedBaseline(first);
    QVERIFY(second.loadSdk(sdkPath).ok);
    QVERIFY(second.connectDevice(config).ok);
    authorizeVerifiedBaseline(second);
    const QString before = QString::fromUtf8(fake.calls());

    QVERIFY(!second.startScan(first.precheckTicket()).ok);
    QCOMPARE(QString::fromUtf8(fake.calls()).count(QStringLiteral("Run")),
        before.count(QStringLiteral("Run")));
}

void DeviceBridgeTest::terminalTransitionsInvalidateTicket()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp, sdkPath);

    auto expectOldTicketRejected = [&fake](DeviceBridge& bridge, const PrecheckTicket& ticket) {
        const int runs = QString::fromUtf8(fake.calls()).count(QStringLiteral("Run"));
        QVERIFY(!bridge.startScan(ticket).ok);
        QCOMPARE(QString::fromUtf8(fake.calls()).count(QStringLiteral("Run")), runs);
    };

    {
        fake.reset();
        DeviceBridge bridge;
        QVERIFY(bridge.loadSdk(sdkPath).ok);
        QVERIFY(bridge.connectDevice(config).ok);
        authorizeVerifiedBaseline(bridge);
        const PrecheckTicket ticket = bridge.precheckTicket();
        QVERIFY(bridge.startScan(ticket).ok);
        fake.setScanStatus(3);
        bridge.refreshStatus();
        QCOMPARE(bridge.sessionState(), MriSdkSessionState::Ready);
        QVERIFY(!QString::fromUtf8(fake.calls()).contains(QStringLiteral("Abort")));
        expectOldTicketRejected(bridge, ticket);
    }

    {
        fake.reset();
        DeviceBridge bridge;
        QVERIFY(bridge.loadSdk(sdkPath).ok);
        QVERIFY(bridge.connectDevice(config).ok);
        authorizeVerifiedBaseline(bridge);
        const PrecheckTicket ticket = bridge.precheckTicket();
        QVERIFY(bridge.startScan(ticket).ok);
        fake.setScanStatus(1);
        bridge.abortScan();
        fake.setScanStatus(0);
        bridge.refreshStatus();
        QCOMPARE(bridge.sessionState(), MriSdkSessionState::Ready);
        QCOMPARE(QString::fromUtf8(fake.calls()).count(QStringLiteral("Abort")), 1);
        expectOldTicketRejected(bridge, ticket);
    }

    {
        fake.reset();
        DeviceBridge bridge;
        QVERIFY(bridge.loadSdk(sdkPath).ok);
        QVERIFY(bridge.connectDevice(config).ok);
        authorizeVerifiedBaseline(bridge);
        const PrecheckTicket ticket = bridge.precheckTicket();
        QVERIFY(bridge.startScan(ticket).ok);
        fake.setScanStatus(5);
        bridge.refreshStatus();
        QCOMPARE(bridge.sessionState(), MriSdkSessionState::Fault);
        QCOMPARE(QString::fromUtf8(fake.calls()).count(QStringLiteral("Abort")), 1);
        expectOldTicketRejected(bridge, ticket);
    }
}

void DeviceBridgeTest::proofCannotDetachFromResolvedPaths()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriSdkConfig detached = createConfig(temp, sdkPath);
    const QString detachedSdk = temp.filePath(QStringLiteral("detached/mridll.dll"));
    QVERIFY(QDir().mkpath(QFileInfo(detachedSdk).absolutePath()));
    QVERIFY(QFile::copy(sdkPath, detachedSdk));
    DeviceBridge sdkBridge;
    QVERIFY(!sdkBridge.loadSdk(detachedSdk, detached.identityProof).ok);
    detached.initPath = temp.filePath(QStringLiteral("detached/init.ini"));
    detached.parameterPath = temp.filePath(QStringLiteral("detached/PTScan.par"));
    writeFile(detached.initPath);
    writeFile(detached.parameterPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath).ok);

    const MriSdkResult connectResult = bridge.connectDevice(detached);
    bridge.selectVerifiedBaseline();
    const PrecheckResult precheckResult = bridge.precheck();
    const MriSdkResult runResult = bridge.startScan(bridge.precheckTicket());

    QVERIFY(!connectResult.ok || !precheckResult.passed);
    QVERIFY(!runResult.ok);
    QVERIFY(!QString::fromUtf8(fake.calls()).contains(QStringLiteral("Run")));
}

void DeviceBridgeTest::rejectsDllReplacementBeforeVerifiedLoad()
{
    const QString sourceSdk = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString copiedSdk = temp.filePath(QStringLiteral("runtime/mridll.dll"));
    QVERIFY(QDir().mkpath(QFileInfo(copiedSdk).absolutePath()));
    QVERIFY(QFile::copy(sourceSdk, copiedSdk));
    const MriSdkConfig config = createConfig(temp, copiedSdk);
    QFile replaced(copiedSdk);
    QVERIFY(replaced.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(replaced.write("replaced") > 0);
    replaced.close();
    DeviceBridge bridge;

    const MriSdkResult result = bridge.loadSdk(copiedSdk, config.identityProof);

    QVERIFY(!result.ok);
    QCOMPARE(bridge.sessionState(), MriSdkSessionState::Unloaded);
}

void DeviceBridgeTest::rejectsInitReplacementBeforePrecheck()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath); fake.reset();
    QTemporaryDir temp; QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath, config.identityProof).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    bridge.selectVerifiedBaseline();
    QFile init(config.initPath); QVERIFY(init.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(init.write("changed init") > 0); init.close();

    QVERIFY(!bridge.precheck().passed);
    QVERIFY(!bridge.startScan(bridge.precheckTicket()).ok);
    QVERIFY(!QString::fromUtf8(fake.calls()).contains(QStringLiteral("Run")));
}

void DeviceBridgeTest::rejectsParameterReplacementBeforeRun()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath); fake.reset();
    QTemporaryDir temp; QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath, config.identityProof).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    const PrecheckTicket ticket = bridge.precheckTicket();
    QFile parameter(config.parameterPath); QVERIFY(parameter.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(parameter.write("changed parameter") > 0); parameter.close();

    QVERIFY(!bridge.startScan(ticket).ok);
    QVERIFY(!QString::fromUtf8(fake.calls()).contains(QStringLiteral("Run")));
}

void DeviceBridgeTest::rejectsHiddenHwCfgAdditionBeforeRun()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath); fake.reset();
    QTemporaryDir temp; QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath, config.identityProof).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    authorizeVerifiedBaseline(bridge);
    const PrecheckTicket ticket = bridge.precheckTicket();
    const QString hidden = QFileInfo(config.initPath).absoluteDir().filePath(QStringLiteral("hidden.cfg"));
    writeFile(hidden); markHidden(hidden);

    QVERIFY(!bridge.startScan(ticket).ok);
    QVERIFY(!QString::fromUtf8(fake.calls()).contains(QStringLiteral("Run")));
}

void DeviceBridgeTest::scientificSelectionCannotReuseBaselineTicket()
{
    const QString sdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(sdkPath); fake.reset();
    QTemporaryDir temp; QVERIFY(temp.isValid());
    const MriSdkConfig config = createConfig(temp, sdkPath);
    DeviceBridge bridge;
    QVERIFY(bridge.loadSdk(sdkPath, config.identityProof).ok);
    QVERIFY(bridge.connectDevice(config).ok);
    bridge.selectVerifiedBaseline();
    QVERIFY(bridge.precheck().passed);
    const PrecheckTicket baselineTicket = bridge.precheckTicket();
    SceneTemplate scientific;
    scientific.name = QStringLiteral("scientific-hold-scene");
    scientific.executionGate = ExecutionGate::Hold;

    bridge.selectScientificScene(scientific);
    QVERIFY(!bridge.startScan(baselineTicket).ok);
    QVERIFY(!bridge.precheck().passed);
    QVERIFY(!bridge.startScan(bridge.precheckTicket()).ok);
    bridge.selectVerifiedBaseline();
    QVERIFY(bridge.precheck().passed);
}

QTEST_MAIN(DeviceBridgeTest)
#include "test_device_bridge.moc"
