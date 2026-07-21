#include "app/MriRuntimeResolver.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QByteArray sha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex().toUpper();
}

QByteArray directoryManifestSha256(const QString& directoryPath)
{
    QStringList records;
    QDirIterator iterator(directoryPath, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    const QDir directory(directoryPath);
    while (iterator.hasNext()) {
        const QFileInfo fileInfo(iterator.next());
        records.append(QStringLiteral("%1|%2|%3")
                           .arg(QDir::fromNativeSeparators(directory.relativeFilePath(fileInfo.filePath())))
                           .arg(fileInfo.size())
                           .arg(QString::fromLatin1(sha256(fileInfo.filePath()))));
    }
    records.sort();
    return QCryptographicHash::hash((records.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8(),
                                    QCryptographicHash::Sha256)
        .toHex()
        .toUpper();
}

void writeFile(const QString& path, const QByteArray& content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    QCOMPARE(file.write(content), content.size());
}

struct RuntimeFixture {
    QString appDir;
    QString runtimeDir;
    QString sdkPath;
    QString initPath;
    QString parPath;
    QString manifestPath;
    MriRuntimeExpectations expectations;
};

void writeManifest(const RuntimeFixture& fixture, const MriRuntimeExpectations& expectations)
{
    QJsonObject manifest{
        {QStringLiteral("mridll"), QJsonObject{
            {QStringLiteral("relativePath"), QStringLiteral("mridll.dll")},
            {QStringLiteral("sha256"), expectations.dllSha256}}},
        {QStringLiteral("hwCfg"), QJsonObject{
            {QStringLiteral("relativePath"), QStringLiteral("hw_cfg")},
            {QStringLiteral("fileCount"), expectations.hwCfgFileCount},
            {QStringLiteral("totalBytes"), static_cast<double>(expectations.hwCfgTotalBytes)},
            {QStringLiteral("manifestSha256"), expectations.hwCfgManifestSha256},
            {QStringLiteral("initSha256"), expectations.initSha256}}},
        {QStringLiteral("parameterFile"), QJsonObject{
            {QStringLiteral("fileName"), QStringLiteral("PTScan.par")},
            {QStringLiteral("sha256"), expectations.parameterSha256}}}
    };
    writeFile(fixture.manifestPath, QJsonDocument(manifest).toJson(QJsonDocument::Compact));
}

RuntimeFixture createBundledRuntime(const QString& applicationDir)
{
    RuntimeFixture fixture;
    fixture.appDir = applicationDir;
    fixture.runtimeDir = QDir(applicationDir).filePath(QStringLiteral("mri-runtime"));
    fixture.sdkPath = QDir(fixture.runtimeDir).filePath(QStringLiteral("mridll.dll"));
    fixture.initPath = QDir(fixture.runtimeDir).filePath(QStringLiteral("hw_cfg/init.ini"));
    fixture.parPath = QDir(fixture.runtimeDir).filePath(QStringLiteral("profiles/PTScan.par"));
    fixture.manifestPath = QDir(fixture.runtimeDir).filePath(QStringLiteral("mri-runtime-manifest.json"));
    writeFile(fixture.sdkPath, "fake sdk");
    writeFile(fixture.initPath, "fake init");
    writeFile(fixture.parPath, "fake parameters");

    const QString hwCfgPath = QDir(fixture.runtimeDir).filePath(QStringLiteral("hw_cfg"));
    fixture.expectations.dllSha256 = QString::fromLatin1(sha256(fixture.sdkPath));
    fixture.expectations.initSha256 = QString::fromLatin1(sha256(fixture.initPath));
    fixture.expectations.parameterSha256 = QString::fromLatin1(sha256(fixture.parPath));
    fixture.expectations.hwCfgFileCount = 1;
    fixture.expectations.hwCfgTotalBytes = QFileInfo(fixture.initPath).size();
    fixture.expectations.hwCfgManifestSha256 = QString::fromLatin1(directoryManifestSha256(hwCfgPath));
    writeManifest(fixture, fixture.expectations);
    return fixture;
}

void setHidden(const QString& path)
{
#ifdef Q_OS_WIN
    QVERIFY(SetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_ATTRIBUTE_HIDDEN));
#else
    Q_UNUSED(path);
#endif
}
}

class MriRuntimeResolverTest : public QObject {
    Q_OBJECT

private slots:
    void resolvesBundledDefaultsAndCreatesOutputDirectory();
    void overridesOnlySpecifiedField();
    void verifiesResolvedIdentityAfterOverrides();
    void rejectsMissingOrInvalidManifest();
    void overridesBypassOnlyMatchingBundledManifestEntry();
    void rejectsTamperedDllEvenWhenManifestIsRewritten();
    void rejectsTamperedParameterAsset();
    void rejectsMissingAndHiddenHwCfgAssets();
    void resolvesAllExplicitLegacyPathsWithoutBundledRuntime();
};

void MriRuntimeResolverTest::resolvesBundledDefaultsAndCreatesOutputDirectory()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const RuntimeFixture fixture = createBundledRuntime(temp.path());

    const MriRuntimePaths paths = MriRuntimeResolver::resolveForTesting(temp.path(), {}, fixture.expectations);

    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QCOMPARE(paths.sdkPath, fixture.sdkPath);
    QCOMPARE(paths.initPath, fixture.initPath);
    QCOMPARE(paths.parameterPath, fixture.parPath);
    QCOMPARE(paths.outputPath, QDir(temp.path()).filePath(QStringLiteral("mri-output")));
    QVERIFY(QFileInfo(paths.outputPath).isDir());
    QVERIFY(paths.verifiedRuntimeAndParameterIdentity);
}

void MriRuntimeResolverTest::verifiesResolvedIdentityAfterOverrides()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const RuntimeFixture fixture = createBundledRuntime(temp.path());
    MriRuntimeOverrides overrides;
    overrides.sdkPath = temp.filePath(QStringLiteral("copies/mridll.dll"));
    overrides.initPath = temp.filePath(QStringLiteral("copies/hw_cfg/init.ini"));
    overrides.parameterPath = temp.filePath(QStringLiteral("copies/PTScan.par"));
    writeFile(overrides.sdkPath, "fake sdk");
    writeFile(overrides.initPath, "fake init");
    writeFile(overrides.parameterPath, "fake parameters");

    const MriRuntimePaths paths = MriRuntimeResolver::resolveForTesting(temp.path(), overrides, fixture.expectations);

    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QVERIFY(paths.verifiedRuntimeAndParameterIdentity);
}

void MriRuntimeResolverTest::overridesOnlySpecifiedField()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const RuntimeFixture fixture = createBundledRuntime(temp.path());
    const QString customSdk = temp.filePath(QStringLiteral("custom/alternate.dll"));
    const QString customInit = temp.filePath(QStringLiteral("custom/alternate.ini"));
    const QString customPar = temp.filePath(QStringLiteral("custom/alternate.par"));
    const QString customOutput = temp.filePath(QStringLiteral("custom-output"));
    writeFile(customSdk, "alternate sdk");
    writeFile(customInit, "alternate init");
    writeFile(customPar, "alternate parameters");

    MriRuntimeOverrides overrides;
    overrides.sdkPath = customSdk;
    MriRuntimePaths paths = MriRuntimeResolver::resolveForTesting(temp.path(), overrides, fixture.expectations);
    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QCOMPARE(paths.sdkPath, customSdk);
    QCOMPARE(paths.initPath, fixture.initPath);
    QCOMPARE(paths.parameterPath, fixture.parPath);

    overrides = {};
    overrides.initPath = customInit;
    paths = MriRuntimeResolver::resolveForTesting(temp.path(), overrides, fixture.expectations);
    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QCOMPARE(paths.sdkPath, fixture.sdkPath);
    QCOMPARE(paths.initPath, customInit);
    QCOMPARE(paths.parameterPath, fixture.parPath);

    overrides = {};
    overrides.parameterPath = customPar;
    paths = MriRuntimeResolver::resolveForTesting(temp.path(), overrides, fixture.expectations);
    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QCOMPARE(paths.sdkPath, fixture.sdkPath);
    QCOMPARE(paths.initPath, fixture.initPath);
    QCOMPARE(paths.parameterPath, customPar);

    overrides = {};
    overrides.outputPath = customOutput;
    paths = MriRuntimeResolver::resolveForTesting(temp.path(), overrides, fixture.expectations);
    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QCOMPARE(paths.outputPath, customOutput);
    QVERIFY(QFileInfo(customOutput).isDir());
}

void MriRuntimeResolverTest::rejectsMissingOrInvalidManifest()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const RuntimeFixture fixture = createBundledRuntime(temp.path());

    QVERIFY(QFile::remove(fixture.manifestPath));
    MriRuntimePaths paths = MriRuntimeResolver::resolveForTesting(temp.path(), {}, fixture.expectations);
    QVERIFY(!paths.isValid());
    QVERIFY(paths.error.contains(QStringLiteral("manifest")));

    writeFile(fixture.manifestPath, "not JSON");
    paths = MriRuntimeResolver::resolveForTesting(temp.path(), {}, fixture.expectations);
    QVERIFY(!paths.isValid());
    QVERIFY(paths.error.contains(QStringLiteral("manifest")));
}

void MriRuntimeResolverTest::overridesBypassOnlyMatchingBundledManifestEntry()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const RuntimeFixture fixture = createBundledRuntime(temp.path());
    const QString customSdk = temp.filePath(QStringLiteral("overrides/custom.dll"));
    const QString customInit = temp.filePath(QStringLiteral("overrides/custom.ini"));
    const QString customPar = temp.filePath(QStringLiteral("overrides/custom.par"));
    writeFile(customSdk, "custom sdk");
    writeFile(customInit, "custom init");
    writeFile(customPar, "custom parameters");

    MriRuntimeExpectations alteredManifest = fixture.expectations;
    alteredManifest.parameterSha256 = QStringLiteral("BAD_PARAMETER_HASH");
    writeManifest(fixture, alteredManifest);
    MriRuntimeOverrides overrides;
    overrides.parameterPath = customPar;
    MriRuntimePaths paths = MriRuntimeResolver::resolveForTesting(temp.path(), overrides, fixture.expectations);
    QVERIFY2(paths.isValid(), qPrintable(paths.error));

    alteredManifest = fixture.expectations;
    alteredManifest.dllSha256 = QStringLiteral("BAD_DLL_HASH");
    writeManifest(fixture, alteredManifest);
    overrides = {};
    overrides.sdkPath = customSdk;
    paths = MriRuntimeResolver::resolveForTesting(temp.path(), overrides, fixture.expectations);
    QVERIFY2(paths.isValid(), qPrintable(paths.error));

    alteredManifest = fixture.expectations;
    alteredManifest.initSha256 = QStringLiteral("BAD_INIT_HASH");
    writeManifest(fixture, alteredManifest);
    overrides = {};
    overrides.initPath = customInit;
    paths = MriRuntimeResolver::resolveForTesting(temp.path(), overrides, fixture.expectations);
    QVERIFY2(paths.isValid(), qPrintable(paths.error));

    alteredManifest = fixture.expectations;
    alteredManifest.parameterSha256 = QStringLiteral("BAD_PARAMETER_HASH");
    writeManifest(fixture, alteredManifest);
    paths = MriRuntimeResolver::resolveForTesting(temp.path(), {}, fixture.expectations);
    QVERIFY(!paths.isValid());
    QVERIFY(paths.error.contains(QStringLiteral("baseline")));
}

void MriRuntimeResolverTest::rejectsTamperedDllEvenWhenManifestIsRewritten()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const RuntimeFixture fixture = createBundledRuntime(temp.path());
    writeFile(fixture.sdkPath, "tampered sdk");

    MriRuntimeExpectations rewritten = fixture.expectations;
    rewritten.dllSha256 = QString::fromLatin1(sha256(fixture.sdkPath));
    writeManifest(fixture, rewritten);

    const MriRuntimePaths paths = MriRuntimeResolver::resolveForTesting(temp.path(), {}, fixture.expectations);
    QVERIFY(!paths.isValid());
    QVERIFY(paths.error.contains(QStringLiteral("baseline")));
}

void MriRuntimeResolverTest::rejectsTamperedParameterAsset()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const RuntimeFixture fixture = createBundledRuntime(temp.path());
    writeFile(fixture.parPath, "tampered parameters");

    const MriRuntimePaths paths = MriRuntimeResolver::resolveForTesting(temp.path(), {}, fixture.expectations);

    QVERIFY(!paths.isValid());
    QVERIFY(paths.error.contains(QStringLiteral("PTScan.par")));
}

void MriRuntimeResolverTest::rejectsMissingAndHiddenHwCfgAssets()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    RuntimeFixture fixture = createBundledRuntime(temp.path());

    QVERIFY(QFile::remove(fixture.initPath));
    MriRuntimePaths paths = MriRuntimeResolver::resolveForTesting(temp.path(), {}, fixture.expectations);
    QVERIFY(!paths.isValid());
    QVERIFY(paths.error.contains(QStringLiteral("init.ini")));

    fixture = createBundledRuntime(temp.path());
    writeFile(fixture.initPath, "tampered init");
    paths = MriRuntimeResolver::resolveForTesting(temp.path(), {}, fixture.expectations);
    QVERIFY(!paths.isValid());
    QVERIFY(paths.error.contains(QStringLiteral("hw_cfg")));

    fixture = createBundledRuntime(temp.path());
    const QString hiddenAsset = QDir(fixture.runtimeDir).filePath(QStringLiteral("hw_cfg/hidden.cfg"));
    writeFile(hiddenAsset, "must be counted");
    setHidden(hiddenAsset);
    paths = MriRuntimeResolver::resolveForTesting(temp.path(), {}, fixture.expectations);
    QVERIFY(!paths.isValid());
    QVERIFY(paths.error.contains(QStringLiteral("hw_cfg")));
}

void MriRuntimeResolverTest::resolvesAllExplicitLegacyPathsWithoutBundledRuntime()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    MriRuntimeOverrides overrides;
    overrides.sdkPath = temp.filePath(QStringLiteral("legacy/mridll.dll"));
    overrides.initPath = temp.filePath(QStringLiteral("legacy/init.ini"));
    overrides.parameterPath = temp.filePath(QStringLiteral("legacy/PTScan.par"));
    overrides.outputPath = temp.filePath(QStringLiteral("legacy-output"));
    writeFile(overrides.sdkPath, "legacy sdk");
    writeFile(overrides.initPath, "legacy init");
    writeFile(overrides.parameterPath, "legacy par");

    const MriRuntimePaths paths = MriRuntimeResolver::resolve(temp.path(), overrides);

    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QCOMPARE(paths.sdkPath, overrides.sdkPath);
    QCOMPARE(paths.initPath, overrides.initPath);
    QCOMPARE(paths.parameterPath, overrides.parameterPath);
    QCOMPARE(paths.outputPath, overrides.outputPath);
}

QTEST_MAIN(MriRuntimeResolverTest)
#include "test_mri_runtime_resolver.moc"
