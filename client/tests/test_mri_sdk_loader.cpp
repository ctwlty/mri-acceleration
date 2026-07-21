#include "app/MriSdkLoader.h"
#include "app/MriSdkTypes.h"

#include <QDir>
#include <QtTest>

class MriSdkLoaderTest : public QObject {
    Q_OBJECT

private slots:
    void missingDllDoesNotFallBackToDemo();
    void missingExportDoesNotEnterLoadedState();
    void completeDllEntersLoadedState();
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

QTEST_MAIN(MriSdkLoaderTest)
#include "test_mri_sdk_loader.moc"
