#include "app/MriSdkLoader.h"
#include "app/MriSdkTypes.h"

#include <QDir>
#include <QFile>
#include <QLibrary>
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
        setFailure = reinterpret_cast<void (*)(const char*, int)>(library.resolve("FakeSetFailure"));
        calls = reinterpret_cast<const char* (*)()>(library.resolve("FakeCalls"));
        initPath = reinterpret_cast<const char* (*)()>(library.resolve("FakeInitPath"));
        outputPath = reinterpret_cast<const char* (*)()>(library.resolve("FakeOutputPath"));
        parameterPath = reinterpret_cast<const char* (*)()>(library.resolve("FakeParameterPath"));
        QVERIFY(reset);
        QVERIFY(setFailure);
        QVERIFY(calls);
        QVERIFY(initPath);
        QVERIFY(outputPath);
        QVERIFY(parameterPath);
    }

    QLibrary library;
    void (*reset)() = nullptr;
    void (*setFailure)(const char*, int) = nullptr;
    const char* (*calls)() = nullptr;
    const char* (*initPath)() = nullptr;
    const char* (*outputPath)() = nullptr;
    const char* (*parameterPath)() = nullptr;
};

void writeFile(const QString& path)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("test") > 0);
}
}

class MriSdkLoaderTest : public QObject {
    Q_OBJECT

private slots:
    void missingDllDoesNotFallBackToDemo();
    void missingExportDoesNotEnterLoadedState();
    void completeDllEntersLoadedState();
    void initializeUsesVerifiedCalibrationSequence();
    void initializeStopsAndClosesOnFailure();
    void initializeClosesOnParameterFailure();
    void calibrationStatusValueDoesNotFailInitialization();
    void prepareScanReloadsParameterAndChannel();
};

void MriSdkLoaderTest::missingDllDoesNotFallBackToDemo()
{
    MriSdkLoader loader;

    const MriSdkResult result = loader.load(QDir::temp().filePath(QStringLiteral("missing-mridll.dll")));

    QVERIFY(!result.ok);
    QVERIFY(!loader.isLoaded());
    QVERIFY(loader.sessionState() != MriSdkSessionState::Loaded);
    QCOMPARE(result.function, QStringLiteral("LoadLibrary"));
}

void MriSdkLoaderTest::missingExportDoesNotEnterLoadedState()
{
    const QString incompleteSdkPath = qEnvironmentVariable("FAKE_INCOMPLETE_MRI_SDK_PATH");
    QVERIFY2(!incompleteSdkPath.isEmpty(), "FAKE_INCOMPLETE_MRI_SDK_PATH must point to the test DLL");
    MriSdkLoader loader;

    const MriSdkResult result = loader.load(incompleteSdkPath);

    QVERIFY(!result.ok);
    QVERIFY(!loader.isLoaded());
    QCOMPARE(loader.sessionState(), MriSdkSessionState::Fault);
    QCOMPARE(result.stage, QStringLiteral("bind"));
    QCOMPARE(result.function, QStringLiteral("GetProcAddress"));
}

void MriSdkLoaderTest::completeDllEntersLoadedState()
{
    const QString fakeSdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    QVERIFY2(!fakeSdkPath.isEmpty(), "FAKE_MRI_SDK_PATH must point to the test DLL");
    MriSdkLoader loader;

    const MriSdkResult result = loader.load(fakeSdkPath);

    QVERIFY2(result.ok, qPrintable(result.message));
    QVERIFY(loader.isLoaded());
    QCOMPARE(loader.sessionState(), MriSdkSessionState::Loaded);
}

void MriSdkLoaderTest::initializeUsesVerifiedCalibrationSequence()
{
    const QString fakeSdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(fakeSdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriSdkConfig config;
    config.initPath = temp.filePath(QStringLiteral("init.ini"));
    config.parameterPath = temp.filePath(QStringLiteral("PTScan.par"));
    config.outputPath = temp.path();
    writeFile(config.initPath);
    writeFile(config.parameterPath);
    MriSdkLoader loader;
    QVERIFY(loader.load(fakeSdkPath).ok);

    const MriSdkResult result = loader.initialize(config);

    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(loader.sessionState(), MriSdkSessionState::Ready);
    const QString expected = QStringLiteral(
        "Init|ConfigFile|SetOutputPath|SetChannelValid:1|SetOutputPrefix:PTMRIData|SetSaveMode:1|"
        "SetParameterFile|SetSystemSel:3|SetAllPreempValue|SetAllGraAnalogDelay|"
        "SetSingleGraGmax:0:2240|SetSingleGraGmax:1:2080|SetSingleGraGmax:2:2980|"
        "SetPreempCross:1|SetPreempValue:0:6:200|SetPreempValue:0:7:500|"
        "SetPreempValue:0:8:800|SetPreempValue:0:9:1000");
    QVERIFY(QString::fromUtf8(fake.calls()).startsWith(expected));
#ifdef Q_OS_WIN
    QVERIFY(!QString::fromLocal8Bit(fake.initPath()).contains('/'));
    QVERIFY(!QString::fromLocal8Bit(fake.outputPath()).contains('/'));
    QVERIFY(!QString::fromLocal8Bit(fake.parameterPath()).contains('/'));
#endif

    const MriSdkStatus status = loader.status();
    QCOMPARE(status.connection, 1);
    QCOMPARE(status.temperature, 31.4);
    QCOMPARE(status.scan, 0);
    QCOMPARE(status.currentScan, 0);
    QCOMPARE(status.totalScans, 8);
}

void MriSdkLoaderTest::initializeStopsAndClosesOnFailure()
{
    const QString fakeSdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(fakeSdkPath);
    fake.reset();
    fake.setFailure("ConfigFile", 12);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriSdkConfig config;
    config.initPath = temp.filePath(QStringLiteral("init.ini"));
    config.parameterPath = temp.filePath(QStringLiteral("PTScan.par"));
    config.outputPath = temp.path();
    writeFile(config.initPath);
    writeFile(config.parameterPath);
    MriSdkLoader loader;
    QVERIFY(loader.load(fakeSdkPath).ok);

    const MriSdkResult result = loader.initialize(config);

    QVERIFY(!result.ok);
    QCOMPARE(result.function, QStringLiteral("ConfigFile"));
    QCOMPARE(result.code, 12);
    QCOMPARE(loader.sessionState(), MriSdkSessionState::Fault);
    QCOMPARE(QString::fromUtf8(fake.calls()), QStringLiteral("Init|ConfigFile|CloseSys"));
}

void MriSdkLoaderTest::initializeClosesOnParameterFailure()
{
    const QString fakeSdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(fakeSdkPath);
    fake.reset();
    fake.setFailure("SetParameterFile", 7);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriSdkConfig config;
    config.initPath = temp.filePath(QStringLiteral("init.ini"));
    config.parameterPath = temp.filePath(QStringLiteral("PTScan.par"));
    config.outputPath = temp.path();
    writeFile(config.initPath);
    writeFile(config.parameterPath);
    MriSdkLoader loader;
    QVERIFY(loader.load(fakeSdkPath).ok);

    const MriSdkResult result = loader.initialize(config);

    QVERIFY(!result.ok);
    QCOMPARE(result.function, QStringLiteral("SetParameterFile"));
    QCOMPARE(result.code, 7);
    QVERIFY(QString::fromUtf8(fake.calls()).endsWith(QStringLiteral("SetParameterFile|CloseSys")));
}

void MriSdkLoaderTest::calibrationStatusValueDoesNotFailInitialization()
{
    const QString fakeSdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(fakeSdkPath);
    fake.reset();
    fake.setFailure("SetPreempCross", 1);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriSdkConfig config;
    config.initPath = temp.filePath(QStringLiteral("init.ini"));
    config.parameterPath = temp.filePath(QStringLiteral("PTScan.par"));
    config.outputPath = temp.path();
    writeFile(config.initPath);
    writeFile(config.parameterPath);
    MriSdkLoader loader;
    QVERIFY(loader.load(fakeSdkPath).ok);

    const MriSdkResult result = loader.initialize(config);

    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(loader.sessionState(), MriSdkSessionState::Ready);
    QVERIFY(QString::fromUtf8(fake.calls()).contains(QStringLiteral("SetPreempCross:1")));
}

void MriSdkLoaderTest::prepareScanReloadsParameterAndChannel()
{
    const QString fakeSdkPath = qEnvironmentVariable("FAKE_MRI_SDK_PATH");
    FakeSdkControl fake(fakeSdkPath);
    fake.reset();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriSdkConfig config;
    config.initPath = temp.filePath(QStringLiteral("init.ini"));
    config.parameterPath = temp.filePath(QStringLiteral("PTScan.par"));
    config.outputPath = temp.path();
    writeFile(config.initPath);
    writeFile(config.parameterPath);
    MriSdkLoader loader;
    QVERIFY(loader.load(fakeSdkPath).ok);
    QVERIFY(loader.initialize(config).ok);
    fake.reset();

    const MriSdkResult result = loader.prepareScan();

    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(QString::fromUtf8(fake.calls()), QStringLiteral("SetParameterFile|SetChannelValid:1"));
}

QTEST_MAIN(MriSdkLoaderTest)
#include "test_mri_sdk_loader.moc"
