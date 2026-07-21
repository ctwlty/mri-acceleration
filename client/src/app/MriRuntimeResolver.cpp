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

namespace {
QString absolutePath(const QString& path)
{
    return QFileInfo(path).absoluteFilePath();
}

QString fileHash(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex().toUpper());
}

QString directoryManifestHash(const QString& directoryPath)
{
    QStringList records;
    QDirIterator iterator(directoryPath, QDir::Files, QDirIterator::Subdirectories);
    const QDir directory(directoryPath);
    while (iterator.hasNext()) {
        const QFileInfo fileInfo(iterator.next());
        records.append(QStringLiteral("%1|%2|%3")
                           .arg(QDir::fromNativeSeparators(directory.relativeFilePath(fileInfo.filePath())))
                           .arg(fileInfo.size())
                           .arg(fileHash(fileInfo.filePath())));
    }
    records.sort();
    return QString::fromLatin1(
        QCryptographicHash::hash((records.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8(), QCryptographicHash::Sha256)
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

bool validateManifestObject(const QJsonObject& object, const QString& name, QString& error)
{
    if (object.isEmpty()) {
        error = QStringLiteral("MRI runtime manifest is missing %1 metadata").arg(name);
        return false;
    }
    return true;
}

bool validateBundledAssets(
    const MriRuntimePaths& paths,
    const MriRuntimeOverrides& overrides,
    QString& error)
{
    if (!overrides.sdkPath.trimmed().isEmpty()
        && !overrides.initPath.trimmed().isEmpty()
        && !overrides.parameterPath.trimmed().isEmpty()) {
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

    const QJsonObject root = document.object();
    const QJsonObject dll = root.value(QStringLiteral("mridll")).toObject();
    const QJsonObject hwCfg = root.value(QStringLiteral("hwCfg")).toObject();
    const QJsonObject parameter = root.value(QStringLiteral("parameterFile")).toObject();

    if (overrides.sdkPath.trimmed().isEmpty()) {
        if (!validateManifestObject(dll, QStringLiteral("mridll"), error)) return false;
        if (dll.value(QStringLiteral("relativePath")).toString() != QStringLiteral("mridll.dll")
            || dll.value(QStringLiteral("sha256")).toString().isEmpty()
            || fileHash(paths.sdkPath) != dll.value(QStringLiteral("sha256")).toString().toUpper()) {
            error = QStringLiteral("MRI runtime mridll.dll does not match its manifest: %1").arg(paths.sdkPath);
            return false;
        }
    }

    if (overrides.initPath.trimmed().isEmpty()) {
        if (!validateManifestObject(hwCfg, QStringLiteral("hwCfg"), error)) return false;
        const QString expectedRelativePath = hwCfg.value(QStringLiteral("relativePath")).toString();
        const QDir hwCfgDir(QDir(paths.runtimeDirectory).filePath(expectedRelativePath));
        if (expectedRelativePath != QStringLiteral("hw_cfg") || !hwCfgDir.exists()) {
            error = QStringLiteral("MRI runtime hw_cfg does not match its manifest: %1").arg(hwCfgDir.absolutePath());
            return false;
        }
        qint64 totalBytes = 0;
        int fileCount = 0;
        QDirIterator iterator(hwCfgDir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            totalBytes += QFileInfo(iterator.next()).size();
            ++fileCount;
        }
        if (fileCount != hwCfg.value(QStringLiteral("fileCount")).toInt(-1)
            || totalBytes != hwCfg.value(QStringLiteral("totalBytes")).toVariant().toLongLong()
            || directoryManifestHash(hwCfgDir.absolutePath()) != hwCfg.value(QStringLiteral("manifestSha256")).toString().toUpper()) {
            error = QStringLiteral("MRI runtime hw_cfg does not match its manifest: %1").arg(hwCfgDir.absolutePath());
            return false;
        }
    }

    if (overrides.parameterPath.trimmed().isEmpty()) {
        if (!validateManifestObject(parameter, QStringLiteral("parameterFile"), error)) return false;
        if (parameter.value(QStringLiteral("fileName")).toString() != QStringLiteral("PTScan.par")
            || parameter.value(QStringLiteral("sha256")).toString().isEmpty()
            || fileHash(paths.parameterPath) != parameter.value(QStringLiteral("sha256")).toString().toUpper()) {
            error = QStringLiteral("MRI runtime PTScan.par does not match its manifest: %1").arg(paths.parameterPath);
            return false;
        }
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
}

bool MriRuntimePaths::isValid() const
{
    return error.isEmpty();
}

MriRuntimePaths MriRuntimeResolver::resolve(const QString& applicationDir, const MriRuntimeOverrides& overrides)
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
        || !validateBundledAssets(paths, overrides, paths.error)
        || !ensureWritableOutput(paths.outputPath, paths.error)) {
        return paths;
    }
    return paths;
}
