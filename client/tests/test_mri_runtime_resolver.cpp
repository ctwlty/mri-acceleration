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
    QDirIterator iterator(directoryPath, QDir::Files, QDirIterator::Subdirectories);
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

void createBundledRuntime(const QString& applicationDir)
{
    const QString runtimeDir = QDir(applicationDir).filePath(QStringLiteral("mri-runtime"));
    const QString sdkPath = QDir(runtimeDir).filePath(QStringLiteral("mridll.dll"));
    const QString initPath = QDir(runtimeDir).filePath(QStringLiteral("hw_cfg/init.ini"));
    const QString parPath = QDir(runtimeDir).filePath(QStringLiteral("profiles/PTScan.par"));
    writeFile(sdkPath, "fake sdk");
    writeFile(initPath, "fake init");
    writeFile(parPath, "fake parameters");
    const QString hwCfgPath = QDir(runtimeDir).filePath(QStringLiteral("hw_cfg"));

    QJsonObject manifest{
        {QStringLiteral("mridll"), QJsonObject{
            {QStringLiteral("relativePath"), QStringLiteral("mridll.dll")},
            {QStringLiteral("sha256"), QString::fromLatin1(sha256(sdkPath))}}},
        {QStringLiteral("hwCfg"), QJsonObject{
            {QStringLiteral("relativePath"), QStringLiteral("hw_cfg")},
            {QStringLiteral("fileCount"), 1},
            {QStringLiteral("totalBytes"), QFileInfo(initPath).size()},
            {QStringLiteral("manifestSha256"), QString::fromLatin1(directoryManifestSha256(hwCfgPath))}}},
        {QStringLiteral("parameterFile"), QJsonObject{
            {QStringLiteral("fileName"), QStringLiteral("PTScan.par")},
            {QStringLiteral("sha256"), QString::fromLatin1(sha256(parPath))}}}
    };
    writeFile(QDir(runtimeDir).filePath(QStringLiteral("mri-runtime-manifest.json")),
              QJsonDocument(manifest).toJson(QJsonDocument::Compact));
}
}

class MriRuntimeResolverTest : public QObject {
    Q_OBJECT

private slots:
    void resolvesBundledDefaultsAndCreatesOutputDirectory();
    void overridesOnlySpecifiedField();
    void rejectsMissingOrInvalidBundledAssets();
};

void MriRuntimeResolverTest::resolvesBundledDefaultsAndCreatesOutputDirectory()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    createBundledRuntime(temp.path());

    const MriRuntimePaths paths = MriRuntimeResolver::resolve(temp.path(), {});

    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QCOMPARE(paths.sdkPath, QDir(temp.path()).filePath(QStringLiteral("mri-runtime/mridll.dll")));
    QCOMPARE(paths.initPath, QDir(temp.path()).filePath(QStringLiteral("mri-runtime/hw_cfg/init.ini")));
    QCOMPARE(paths.parameterPath, QDir(temp.path()).filePath(QStringLiteral("mri-runtime/profiles/PTScan.par")));
    QCOMPARE(paths.outputPath, QDir(temp.path()).filePath(QStringLiteral("mri-output")));
    QVERIFY(QFileInfo::exists(paths.outputPath));
    QVERIFY(QFileInfo(paths.outputPath).isDir());
}

void MriRuntimeResolverTest::overridesOnlySpecifiedField()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    createBundledRuntime(temp.path());
    const QString customSdk = temp.filePath(QStringLiteral("custom/alternate.dll"));
    const QString customInit = temp.filePath(QStringLiteral("custom/alternate.ini"));
    const QString customPar = temp.filePath(QStringLiteral("custom/alternate.par"));
    const QString customOutput = temp.filePath(QStringLiteral("custom-output"));
    writeFile(customSdk, "alternate sdk");
    writeFile(customInit, "alternate init");
    writeFile(customPar, "alternate parameters");

    MriRuntimeOverrides overrides;
    const QString bundledSdk = QDir(temp.path()).filePath(QStringLiteral("mri-runtime/mridll.dll"));
    const QString bundledInit = QDir(temp.path()).filePath(QStringLiteral("mri-runtime/hw_cfg/init.ini"));
    const QString bundledPar = QDir(temp.path()).filePath(QStringLiteral("mri-runtime/profiles/PTScan.par"));
    const QString defaultOutput = QDir(temp.path()).filePath(QStringLiteral("mri-output"));

    overrides.sdkPath = customSdk;
    MriRuntimePaths paths = MriRuntimeResolver::resolve(temp.path(), overrides);
    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QCOMPARE(paths.sdkPath, customSdk);
    QCOMPARE(paths.initPath, bundledInit);
    QCOMPARE(paths.parameterPath, bundledPar);
    QCOMPARE(paths.outputPath, defaultOutput);

    overrides = {};
    overrides.initPath = customInit;
    paths = MriRuntimeResolver::resolve(temp.path(), overrides);
    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QCOMPARE(paths.sdkPath, bundledSdk);
    QCOMPARE(paths.initPath, customInit);
    QCOMPARE(paths.parameterPath, bundledPar);
    QCOMPARE(paths.outputPath, defaultOutput);

    overrides = {};
    overrides.parameterPath = customPar;
    paths = MriRuntimeResolver::resolve(temp.path(), overrides);
    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QCOMPARE(paths.sdkPath, bundledSdk);
    QCOMPARE(paths.initPath, bundledInit);
    QCOMPARE(paths.parameterPath, customPar);
    QCOMPARE(paths.outputPath, defaultOutput);

    overrides = {};
    overrides.outputPath = customOutput;
    paths = MriRuntimeResolver::resolve(temp.path(), overrides);
    QVERIFY2(paths.isValid(), qPrintable(paths.error));
    QCOMPARE(paths.sdkPath, bundledSdk);
    QCOMPARE(paths.initPath, bundledInit);
    QCOMPARE(paths.parameterPath, bundledPar);
    QCOMPARE(paths.outputPath, customOutput);
    QVERIFY(QFileInfo::exists(customOutput));
}

void MriRuntimeResolverTest::rejectsMissingOrInvalidBundledAssets()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    createBundledRuntime(temp.path());

    QVERIFY(QFile::remove(QDir(temp.path()).filePath(QStringLiteral("mri-runtime/mridll.dll"))));
    MriRuntimePaths paths = MriRuntimeResolver::resolve(temp.path(), {});
    QVERIFY(!paths.isValid());
    QVERIFY(paths.error.contains(QStringLiteral("mridll.dll")));

    createBundledRuntime(temp.path());
    writeFile(QDir(temp.path()).filePath(QStringLiteral("mri-runtime/profiles/PTScan.par")), "tampered parameters");
    paths = MriRuntimeResolver::resolve(temp.path(), {});
    QVERIFY(!paths.isValid());
    QVERIFY(paths.error.contains(QStringLiteral("PTScan.par")));
}

QTEST_MAIN(MriRuntimeResolverTest)
#include "test_mri_runtime_resolver.moc"
