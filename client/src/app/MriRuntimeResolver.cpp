#include "MriRuntimeResolver.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTemporaryFile>

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
struct RuntimeExpectations {
    QString dllSha256;
    QString initSha256;
    QString parameterSha256;
    int hwCfgFileCount = 0;
    qint64 hwCfgTotalBytes = 0;
    QString hwCfgManifestSha256;
};

const RuntimeExpectations& productionExpectations()
{
    static const RuntimeExpectations expectations{
        QStringLiteral("D32AF2B676A4956A3D9AB8707B49F47083328A5CE9236FBB5324E44C28054CE8"),
        QStringLiteral("644D2F4DAD06E5FD5AC6DF7161C63A4164F5B56F926C66DC77D3892CAD411956"),
        QStringLiteral("6FD62B50A56B802D070AE52737A57516FECE927FCE28BDA17979D4C046C36783"),
        455,
        206656,
        QStringLiteral("A8BFF731985A8886EEB53191A6AFD9F5F037931A841A50A4960738595FC45F6F")};
    return expectations;
}

QString absolutePath(const QString& path)
{
    return QFileInfo(path).absoluteFilePath();
}

QString canonicalIdentityPath(const QString& path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? absolutePath(path) : canonical);
}

QString fileHash(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex().toUpper());
}

struct DirectoryManifestRecord {
    QString relativePath;
    QString record;
};

bool invariantRelativePathLess(const DirectoryManifestRecord& left, const DirectoryManifestRecord& right)
{
#ifdef Q_OS_WIN
    return CompareStringEx(
               LOCALE_NAME_INVARIANT,
               NORM_IGNORECASE,
               reinterpret_cast<LPCWCH>(left.relativePath.utf16()),
               left.relativePath.size(),
               reinterpret_cast<LPCWCH>(right.relativePath.utf16()),
               right.relativePath.size(),
               nullptr,
               nullptr,
               0)
        == CSTR_LESS_THAN;
#else
    return QString::localeAwareCompare(left.relativePath, right.relativePath) < 0;
#endif
}

QString directoryManifestHash(const QString& directoryPath)
{
    QList<DirectoryManifestRecord> entries;
    QDirIterator iterator(
        directoryPath,
        QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    const QDir directory(directoryPath);
    while (iterator.hasNext()) {
        const QFileInfo fileInfo(iterator.next());
        QString relativePath = directory.relativeFilePath(fileInfo.filePath());
        relativePath.replace(QLatin1Char('/'), QLatin1Char('\\'));
        entries.append({
            relativePath,
            QStringLiteral("%1|%2|%3")
                .arg(relativePath)
                .arg(fileInfo.size())
                .arg(fileHash(fileInfo.filePath()))});
    }
    std::stable_sort(entries.begin(), entries.end(), invariantRelativePathLess);
    QStringList records;
    records.reserve(entries.size());
    for (const auto& entry : entries) {
        records.append(entry.record);
    }
    return QString::fromLatin1(
        QCryptographicHash::hash(records.join(QLatin1Char('\n')).toUtf8(), QCryptographicHash::Sha256)
            .toHex()
            .toUpper());
}

bool validateFile(const QString& path, const QString& label, QString& error)
{
    if (!QFileInfo(path).isFile()) {
        error = QStringLiteral("MRI runtime %1 is missing or is not a file: %2").arg(label, path);
        return false;
    }
    return true;
}

bool manifestMatchesProductionExpectations(
    const QJsonObject& root,
    const RuntimeExpectations& expectations,
    bool usesBundledSdk,
    bool usesBundledInit,
    bool usesBundledParameter,
    QString& error)
{
    const QJsonObject dll = root.value(QStringLiteral("mridll")).toObject();
    const QJsonObject hwCfg = root.value(QStringLiteral("hwCfg")).toObject();
    const QJsonObject parameter = root.value(QStringLiteral("parameterFile")).toObject();
    if (usesBundledSdk
        && (dll.value(QStringLiteral("relativePath")).toString() != QStringLiteral("mridll.dll")
            || dll.value(QStringLiteral("sha256")).toString().toUpper() != expectations.dllSha256)) {
        error = QStringLiteral("MRI runtime manifest does not match the compiled production baseline");
        return false;
    }
    if (usesBundledInit
        && (hwCfg.value(QStringLiteral("relativePath")).toString() != QStringLiteral("hw_cfg")
            || hwCfg.value(QStringLiteral("fileCount")).toInt(-1) != expectations.hwCfgFileCount
            || hwCfg.value(QStringLiteral("totalBytes")).toVariant().toLongLong() != expectations.hwCfgTotalBytes
            || hwCfg.value(QStringLiteral("manifestSha256")).toString().toUpper() != expectations.hwCfgManifestSha256
            || hwCfg.value(QStringLiteral("initSha256")).toString().toUpper() != expectations.initSha256)) {
        error = QStringLiteral("MRI runtime manifest does not match the compiled production baseline");
        return false;
    }
    if (usesBundledParameter
        && (parameter.value(QStringLiteral("fileName")).toString() != QStringLiteral("PTScan.par")
            || parameter.value(QStringLiteral("sha256")).toString().toUpper() != expectations.parameterSha256)) {
        error = QStringLiteral("MRI runtime manifest does not match the compiled production baseline");
        return false;
    }
    return true;
}

bool validateBundledAssets(
    const MriRuntimePaths& paths,
    const MriRuntimeOverrides& overrides,
    const RuntimeExpectations& expectations,
    QString& error)
{
    const bool usesBundledSdk = overrides.sdkPath.trimmed().isEmpty();
    const bool usesBundledInit = overrides.initPath.trimmed().isEmpty();
    const bool usesBundledParameter = overrides.parameterPath.trimmed().isEmpty();
    if (!usesBundledSdk && !usesBundledInit && !usesBundledParameter) {
        return true;
    }

    const QString manifestPath = QDir(paths.runtimeDirectory).filePath(QStringLiteral("mri-runtime-manifest.json"));
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("MRI runtime manifest is missing or unreadable: %1").arg(manifestPath);
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("MRI runtime manifest is invalid: %1").arg(manifestPath);
        return false;
    }
    if (!manifestMatchesProductionExpectations(
            document.object(), expectations, usesBundledSdk, usesBundledInit, usesBundledParameter, error)) {
        return false;
    }

    if (usesBundledSdk && fileHash(paths.sdkPath) != expectations.dllSha256) {
        error = QStringLiteral("MRI runtime mridll.dll does not match the compiled production baseline: %1").arg(paths.sdkPath);
        return false;
    }
    if (usesBundledInit) {
        const QDir hwCfgDir(QDir(paths.runtimeDirectory).filePath(QStringLiteral("hw_cfg")));
        qint64 totalBytes = 0;
        int fileCount = 0;
        QDirIterator iterator(
            hwCfgDir.absolutePath(),
            QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
            QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            totalBytes += QFileInfo(iterator.next()).size();
            ++fileCount;
        }
        if (!hwCfgDir.exists()
            || fileHash(paths.initPath) != expectations.initSha256
            || fileCount != expectations.hwCfgFileCount
            || totalBytes != expectations.hwCfgTotalBytes
            || directoryManifestHash(hwCfgDir.absolutePath()) != expectations.hwCfgManifestSha256) {
            error = QStringLiteral("MRI runtime hw_cfg does not match the compiled production baseline: %1").arg(hwCfgDir.absolutePath());
            return false;
        }
    }
    if (usesBundledParameter && fileHash(paths.parameterPath) != expectations.parameterSha256) {
        error = QStringLiteral("MRI runtime PTScan.par does not match the compiled production baseline: %1").arg(paths.parameterPath);
        return false;
    }
    return true;
}

bool ensureWritableOutput(const QString& outputPath, QString& error)
{
    if (!QDir().mkpath(outputPath)) {
        error = QStringLiteral("MRI output directory cannot be created: %1").arg(outputPath);
        return false;
    }
    QTemporaryFile probe(QDir(outputPath).filePath(QStringLiteral(".mri-runtime-write-XXXXXX")));
    if (!probe.open()) {
        error = QStringLiteral("MRI output directory is not writable: %1").arg(outputPath);
        return false;
    }
    return true;
}

bool hasVerifiedBaselineIdentity(const MriRuntimePaths& paths, const RuntimeExpectations& expectations)
{
    const QDir hwCfgDir(QFileInfo(paths.initPath).absoluteDir());
    qint64 totalBytes = 0;
    int fileCount = 0;
    QDirIterator iterator(
        hwCfgDir.absolutePath(),
        QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        totalBytes += QFileInfo(iterator.next()).size();
        ++fileCount;
    }
    return fileHash(paths.sdkPath) == expectations.dllSha256
        && fileHash(paths.initPath) == expectations.initSha256
        && fileHash(paths.parameterPath) == expectations.parameterSha256
        && fileCount == expectations.hwCfgFileCount
        && totalBytes == expectations.hwCfgTotalBytes
        && directoryManifestHash(hwCfgDir.absolutePath()) == expectations.hwCfgManifestSha256;
}

}

bool MriRuntimeResolver::proofMatchesActualIdentity(const BaselineIdentityProof& proof)
{
    const QDir hwCfgDir(proof.m_hwCfgPath);
    qint64 totalBytes = 0;
    int fileCount = 0;
    QDirIterator iterator(
        hwCfgDir.absolutePath(),
        QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        totalBytes += QFileInfo(iterator.next()).size();
        ++fileCount;
    }
    return proof.isValid()
        && canonicalIdentityPath(proof.m_sdkPath) == proof.m_sdkPath
        && canonicalIdentityPath(proof.m_initPath) == proof.m_initPath
        && canonicalIdentityPath(proof.m_hwCfgPath) == proof.m_hwCfgPath
        && canonicalIdentityPath(proof.m_parameterPath) == proof.m_parameterPath
        && fileHash(proof.m_sdkPath) == proof.m_dllSha256
        && fileHash(proof.m_initPath) == proof.m_initSha256
        && fileHash(proof.m_parameterPath) == proof.m_parameterSha256
        && hwCfgDir.exists()
        && fileCount == proof.m_hwCfgFileCount
        && totalBytes == proof.m_hwCfgTotalBytes
        && directoryManifestHash(proof.m_hwCfgPath) == proof.m_hwCfgManifestSha256;
}

namespace {

MriRuntimePaths resolveWithExpectations(
    const QString& applicationDir,
    const MriRuntimeOverrides& overrides,
    const RuntimeExpectations& expectations)
{
    MriRuntimePaths paths;
    const QString appDir = absolutePath(applicationDir);
    paths.runtimeDirectory = QDir(appDir).filePath(QStringLiteral("mri-runtime"));
    paths.sdkPath = absolutePath(overrides.sdkPath.trimmed().isEmpty()
        ? QDir(paths.runtimeDirectory).filePath(QStringLiteral("mridll.dll"))
        : overrides.sdkPath);
    paths.initPath = absolutePath(overrides.initPath.trimmed().isEmpty()
        ? QDir(paths.runtimeDirectory).filePath(QStringLiteral("hw_cfg/init.ini"))
        : overrides.initPath);
    paths.parameterPath = absolutePath(overrides.parameterPath.trimmed().isEmpty()
        ? QDir(paths.runtimeDirectory).filePath(QStringLiteral("profiles/PTScan.par"))
        : overrides.parameterPath);
    paths.outputPath = absolutePath(overrides.outputPath.trimmed().isEmpty()
        ? QDir(appDir).filePath(QStringLiteral("mri-output"))
        : overrides.outputPath);

    if (!validateFile(paths.sdkPath, QStringLiteral("mridll.dll"), paths.error)
        || !validateFile(paths.initPath, QStringLiteral("init.ini"), paths.error)
        || !validateFile(paths.parameterPath, QStringLiteral("PTScan.par"), paths.error)
        || !validateBundledAssets(paths, overrides, expectations, paths.error)
        || !ensureWritableOutput(paths.outputPath, paths.error)) {
        return paths;
    }
    return paths;
}
}

bool MriRuntimePaths::isValid() const
{
    return error.isEmpty();
}

MriRuntimePaths MriRuntimeResolver::resolve(const QString& applicationDir, const MriRuntimeOverrides& overrides)
{
    MriRuntimePaths paths = resolveWithExpectations(applicationDir, overrides, productionExpectations());
    if (paths.isValid() && hasVerifiedBaselineIdentity(paths, productionExpectations())) {
        const RuntimeExpectations& expectations = productionExpectations();
        paths.identityProof = mintIdentityProof(paths, expectations.dllSha256, expectations.initSha256,
            expectations.parameterSha256, expectations.hwCfgFileCount, expectations.hwCfgTotalBytes,
            expectations.hwCfgManifestSha256);
    }
    return paths;
}

bool MriRuntimeResolver::verifyBaselineIdentityNow(
    const QString& sdkPath,
    const MriSdkConfig& config,
    const BaselineIdentityProof& proof)
{
    return canonicalIdentityPath(sdkPath) == proof.m_sdkPath
        && canonicalIdentityPath(config.initPath) == proof.m_initPath
        && canonicalIdentityPath(QFileInfo(config.initPath).absolutePath()) == proof.m_hwCfgPath
        && canonicalIdentityPath(config.parameterPath) == proof.m_parameterPath
        && proofMatchesActualIdentity(proof);
}

bool MriRuntimeResolver::verifyBaselineIdentityNow(
    const QString& sdkPath,
    const BaselineIdentityProof& proof)
{
    return canonicalIdentityPath(sdkPath) == proof.m_sdkPath && proofMatchesActualIdentity(proof);
}

BaselineIdentityProof MriRuntimeResolver::mintIdentityProof(
    const MriRuntimePaths& paths,
    const QString& dllSha256,
    const QString& initSha256,
    const QString& parameterSha256,
    int hwCfgFileCount,
    qint64 hwCfgTotalBytes,
    const QString& hwCfgManifestSha256)
{
    return BaselineIdentityProof(
        canonicalIdentityPath(paths.sdkPath),
        canonicalIdentityPath(paths.initPath),
        canonicalIdentityPath(QFileInfo(paths.initPath).absolutePath()),
        canonicalIdentityPath(paths.parameterPath),
        dllSha256.toUpper(), initSha256.toUpper(), parameterSha256.toUpper(),
        hwCfgFileCount, hwCfgTotalBytes, hwCfgManifestSha256.toUpper());
}

#ifdef MRI_RUNTIME_RESOLVER_TESTING
MriRuntimePaths MriRuntimeResolver::resolveForTesting(
    const QString& applicationDir,
    const MriRuntimeOverrides& overrides,
    const MriRuntimeExpectations& expectations)
{
    MriRuntimePaths paths = resolveWithExpectations(
        applicationDir,
        overrides,
        RuntimeExpectations{
            expectations.dllSha256.toUpper(),
            expectations.initSha256.toUpper(),
            expectations.parameterSha256.toUpper(),
            expectations.hwCfgFileCount,
            expectations.hwCfgTotalBytes,
            expectations.hwCfgManifestSha256.toUpper()});
    if (paths.isValid() && hasVerifiedBaselineIdentity(paths, RuntimeExpectations{
            expectations.dllSha256.toUpper(), expectations.initSha256.toUpper(), expectations.parameterSha256.toUpper(),
            expectations.hwCfgFileCount, expectations.hwCfgTotalBytes, expectations.hwCfgManifestSha256.toUpper()})) {
        paths.identityProof = mintIdentityProof(paths, expectations.dllSha256, expectations.initSha256,
            expectations.parameterSha256, expectations.hwCfgFileCount, expectations.hwCfgTotalBytes,
            expectations.hwCfgManifestSha256);
    }
    return paths;
}
#endif
