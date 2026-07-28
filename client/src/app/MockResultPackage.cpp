#include "MockResultPackage.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace {
const QStringList& artifactNames()
{
    static const QStringList names = {
        QStringLiteral("parameter-snapshot.json"),
        QStringLiteral("mock-source.json"),
        QStringLiteral("standard-mock-result.png"),
        QStringLiteral("mock-qc.json"),
        QStringLiteral("audit-events.json"),
        QStringLiteral("task-note.txt")
    };
    return names;
}

QByteArray sha256(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
        .toHex()
        .toUpper();
}

bool isSha256(const QByteArray& value)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9A-Fa-f]{64}$"));
    return pattern.match(QString::fromLatin1(value)).hasMatch();
}

bool isSafeIdentifier(const QString& value)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$"));
    return value != QStringLiteral(".") && value != QStringLiteral("..")
        && pattern.match(value).hasMatch();
}

QByteArray jsonBytes(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

QByteArray jsonBytes(const QJsonArray& array)
{
    return QJsonDocument(array).toJson(QJsonDocument::Indented);
}

bool writeFile(const QString& path, const QByteArray& bytes, QString& error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("无法写入 %1：%2").arg(path, file.errorString());
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        error = QStringLiteral("写入不完整 %1：%2").arg(path, file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        error = QStringLiteral("无法提交 %1：%2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool inspectFile(const QString& absolutePath,
                 const QString& relativePath,
                 PackageArtifactInfo& result,
                 QString& error)
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("无法读取已写入工件 %1：%2")
                    .arg(absolutePath, file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();
    result.relativePath = QDir::fromNativeSeparators(relativePath);
    result.byteSize = bytes.size();
    result.sha256 = sha256(bytes);
    return true;
}

QString validateInput(const MockPackageInput& input)
{
    if (input.rootDirectory.trimmed().isEmpty())
        return QStringLiteral("结果根目录为空");
    if (!isSafeIdentifier(input.runId))
        return QStringLiteral("run ID 不能安全用作结果目录名");
    if (input.snapshot.snapshotId.trimmed().isEmpty())
        return QStringLiteral("snapshot ID 为空");
    if (input.snapshot.dataSource != DataSourceKind::Mock)
        return QStringLiteral("结果包只允许 dataSource=MOCK");
    if (input.softwareCommit.trimmed().isEmpty())
        return QStringLiteral("softwareCommit 为空");
    const QStringList draftErrors = input.snapshot.parameters.validationErrors();
    if (!draftErrors.isEmpty())
        return draftErrors.join(QStringLiteral("；"));
    const QString inputRoot = QDir(input.rootDirectory).absolutePath();
    const QString snapshotRoot =
        QDir(input.snapshot.parameters.outputRoot).absolutePath();
    const Qt::CaseSensitivity pathCaseSensitivity =
#ifdef Q_OS_WIN
        Qt::CaseInsensitive;
#else
        Qt::CaseSensitive;
#endif
    if (QString::compare(QDir::cleanPath(inputRoot),
                         QDir::cleanPath(snapshotRoot),
                         pathCaseSensitivity)
        != 0) {
        return QStringLiteral("结果根目录与冻结参数快照不一致");
    }
    if (input.standardResultPng.isEmpty())
        return QStringLiteral("标准 Mock 结果 PNG 为空");
    if (!input.reconstruction.isValid())
        return QStringLiteral("Mock 重建来源或哈希无效");
    const QByteArray actualImageHash = sha256(input.standardResultPng);
    if (input.reconstruction.byteSize != input.standardResultPng.size()
        || input.reconstruction.pngSha256.toUpper() != actualImageHash) {
        return QStringLiteral("标准 Mock 结果与重建来源大小或哈希不一致");
    }
    if (!input.qc.isValid()
        || input.qc.imageSha256.toUpper() != actualImageHash) {
        return QStringLiteral("Mock QC 图像哈希与标准结果不一致");
    }
    if (input.taskNote.trimmed().isEmpty())
        return QStringLiteral("任务说明为空");
    if (!input.createdAtUtc.isValid())
        return QStringLiteral("结果包创建时间无效");
    if (input.auditEvents.isEmpty())
        return QStringLiteral("审计事件为空");
    for (const MockAuditEvent& event : input.auditEvents) {
        if (event.dataSource != DataSourceKind::Mock)
            return QStringLiteral("审计事件包含非 MOCK 数据源");
        if (!event.runId.isEmpty() && event.runId != input.runId)
            return QStringLiteral("审计事件 run ID 与结果包不一致");
    }
    return {};
}

QJsonObject artifactMap(const QVector<PackageArtifactInfo>& artifacts)
{
    QJsonArray array;
    for (const PackageArtifactInfo& artifact : artifacts)
        array.append(artifact.toJson());
    return {{QStringLiteral("artifacts"), array}};
}

PackageVerification verificationError(
    const QString& packageDirectory,
    const QString& issue,
    const QJsonObject& manifest = {})
{
    PackageVerification result;
    result.integrity = PackageIntegrity::Error;
    result.packageDirectory = packageDirectory;
    result.manifest = manifest;
    result.issues.append(issue);
    return result;
}
}

QJsonObject PackageArtifactInfo::toJson() const
{
    return {
        {QStringLiteral("path"), QDir::fromNativeSeparators(relativePath)},
        {QStringLiteral("bytes"), static_cast<double>(byteSize)},
        {QStringLiteral("sha256"), QString::fromLatin1(sha256.toUpper())}
    };
}

QString MockResultPackage::defaultRoot()
{
    QString userProfile = qEnvironmentVariable("USERPROFILE").trimmed();
    if (userProfile.isEmpty())
        userProfile = QDir::homePath();
    return QDir::cleanPath(
        QDir(userProfile).filePath(QStringLiteral("Documents/ScenarioNmr/MockRuns")));
}

PackageWriteResult MockResultPackage::write(const MockPackageInput& input)
{
    PackageWriteResult result;
    const QString validationError = validateInput(input);
    if (!validationError.isEmpty()) {
        result.error = validationError;
        return result;
    }

    const QString rootPath = QDir(input.rootDirectory).absolutePath();
    const QFileInfo rootInfo(rootPath);
    if (rootInfo.exists() && !rootInfo.isDir()) {
        result.error = QStringLiteral("结果根路径已存在但不是目录：%1").arg(rootPath);
        return result;
    }
    if (!rootInfo.exists() && !QDir().mkpath(rootPath)) {
        result.error = QStringLiteral("无法创建结果根目录：%1").arg(rootPath);
        return result;
    }

    QDir root(rootPath);
    const QString finalDirectory = root.filePath(input.runId);
    if (QFileInfo::exists(finalDirectory)) {
        result.error = QStringLiteral("结果包目录已存在，拒绝覆盖：%1")
                           .arg(finalDirectory);
        return result;
    }

    const QString stagingName =
        QStringLiteral(".%1.staging-%2")
            .arg(input.runId,
                 QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString stagingDirectory = root.filePath(stagingName);
    if (!root.mkdir(stagingName)) {
        result.error = QStringLiteral("无法创建结果包暂存目录：%1")
                           .arg(stagingDirectory);
        return result;
    }
    const auto cleanupStaging = [&stagingDirectory] {
        QDir(stagingDirectory).removeRecursively();
    };

    QJsonObject snapshot = input.snapshot.toJson();
    snapshot.insert(QStringLiteral("runId"), input.runId);
    snapshot.insert(QStringLiteral("softwareCommit"), input.softwareCommit);

    QJsonObject source = input.reconstruction.toJson();
    source.insert(QStringLiteral("schemaVersion"),
                  QStringLiteral("agent-mri.mock-source.v1"));
    source.insert(QStringLiteral("dataSource"), QStringLiteral("MOCK"));
    source.insert(QStringLiteral("kind"),
                  QStringLiteral("BUNDLED_MOCK_ASSET"));
    source.insert(QStringLiteral("runId"), input.runId);
    source.insert(QStringLiteral("snapshotId"), input.snapshot.snapshotId);

    QJsonObject qc = input.qc.toJson();
    qc.insert(QStringLiteral("schemaVersion"),
              QStringLiteral("agent-mri.mock-qc.v1"));
    qc.insert(QStringLiteral("dataSource"), QStringLiteral("MOCK"));
    qc.insert(QStringLiteral("runId"), input.runId);
    qc.insert(QStringLiteral("snapshotId"), input.snapshot.snapshotId);

    QJsonArray audit;
    for (const MockAuditEvent& event : input.auditEvents)
        audit.append(event.toJson());
    MockAuditEvent packageSaved;
    packageSaved.name = QStringLiteral("PACKAGE_SAVED");
    packageSaved.occurredAtUtc = input.createdAtUtc.toUTC();
    packageSaved.runId = input.runId;
    packageSaved.state = MockWorkflowState::Packaged;
    packageSaved.dataSource = DataSourceKind::Mock;
    packageSaved.details = {
        {QStringLiteral("packagePath"),
         QFileInfo(finalDirectory).absoluteFilePath()}
    };
    audit.append(packageSaved.toJson());

    const QList<QPair<QString, QByteArray>> files = {
        {QStringLiteral("parameter-snapshot.json"), jsonBytes(snapshot)},
        {QStringLiteral("mock-source.json"), jsonBytes(source)},
        {QStringLiteral("standard-mock-result.png"), input.standardResultPng},
        {QStringLiteral("mock-qc.json"), jsonBytes(qc)},
        {QStringLiteral("audit-events.json"), jsonBytes(audit)},
        {QStringLiteral("task-note.txt"), input.taskNote.toUtf8()}
    };

    QString error;
    for (const auto& file : files) {
        const QString absolutePath =
            QDir(stagingDirectory).filePath(file.first);
        if (!writeFile(absolutePath, file.second, error)) {
            cleanupStaging();
            result.error = error;
            return result;
        }
        PackageArtifactInfo artifact;
        if (!inspectFile(absolutePath, file.first, artifact, error)) {
            cleanupStaging();
            result.error = error;
            return result;
        }
        result.artifacts.append(artifact);
    }

    QJsonObject manifest = artifactMap(result.artifacts);
    manifest.insert(QStringLiteral("schemaVersion"),
                    QStringLiteral("agent-mri.mock-package.v1"));
    manifest.insert(QStringLiteral("dataSource"), QStringLiteral("MOCK"));
    manifest.insert(QStringLiteral("softwareCommit"), input.softwareCommit);
    manifest.insert(QStringLiteral("runId"), input.runId);
    manifest.insert(QStringLiteral("snapshotId"), input.snapshot.snapshotId);
    manifest.insert(QStringLiteral("sampleId"),
                    input.snapshot.parameters.sampleId);
    manifest.insert(QStringLiteral("templateId"),
                    input.snapshot.parameters.templateId);
    manifest.insert(QStringLiteral("templateName"),
                    input.snapshot.parameters.templateName);
    manifest.insert(
        QStringLiteral("createdAtUtc"),
        input.createdAtUtc.toUTC().toString(Qt::ISODateWithMs));
    if (!writeFile(
            QDir(stagingDirectory).filePath(QStringLiteral("manifest.json")),
            jsonBytes(manifest), error)) {
        cleanupStaging();
        result.error = error;
        return result;
    }

    if (!root.rename(stagingName, input.runId)) {
        cleanupStaging();
        result.error = QStringLiteral(
            "无法将暂存结果包原子重命名为最终目录：%1")
                           .arg(finalDirectory);
        return result;
    }

    result.ok = true;
    result.packageDirectory = QFileInfo(finalDirectory).absoluteFilePath();
    return result;
}

PackageVerification MockResultPackage::verify(
    const QString& packageDirectory)
{
    const QString absoluteDirectory =
        QFileInfo(packageDirectory).absoluteFilePath();
    const QFileInfo directoryInfo(absoluteDirectory);
    if (!directoryInfo.isDir()) {
        return verificationError(
            absoluteDirectory,
            QStringLiteral("结果包目录不存在或不是目录"));
    }

    QFile manifestFile(
        QDir(absoluteDirectory).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return verificationError(
            absoluteDirectory,
            QStringLiteral("manifest.json 不可读：%1")
                .arg(manifestFile.errorString()));
    }
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        return verificationError(
            absoluteDirectory,
            QStringLiteral("manifest.json 解析失败：%1")
                .arg(parseError.errorString()));
    }
    const QJsonObject manifest = document.object();
    if (manifest.value(QStringLiteral("schemaVersion")).toString()
            != QStringLiteral("agent-mri.mock-package.v1")) {
        return verificationError(
            absoluteDirectory,
            QStringLiteral("manifest schemaVersion 无效"), manifest);
    }
    if (manifest.value(QStringLiteral("dataSource")).toString()
            != QStringLiteral("MOCK")) {
        return verificationError(
            absoluteDirectory,
            QStringLiteral("manifest dataSource 不是 MOCK"), manifest);
    }
    if (manifest.value(QStringLiteral("runId")).toString().isEmpty()
        || manifest.value(QStringLiteral("snapshotId")).toString().isEmpty()
        || manifest.value(QStringLiteral("softwareCommit")).toString().isEmpty()) {
        return verificationError(
            absoluteDirectory,
            QStringLiteral("manifest 缺少 runId、snapshotId 或 softwareCommit"),
            manifest);
    }
    const QJsonValue artifactsValue =
        manifest.value(QStringLiteral("artifacts"));
    if (!artifactsValue.isArray()) {
        return verificationError(
            absoluteDirectory,
            QStringLiteral("manifest artifacts 不是数组"), manifest);
    }

    PackageVerification result;
    result.integrity = PackageIntegrity::Valid;
    result.packageDirectory = absoluteDirectory;
    result.manifest = manifest;

    QSet<QString> expected(artifactNames().cbegin(), artifactNames().cend());
    QSet<QString> seen;
    const QJsonArray artifacts = artifactsValue.toArray();
    for (const QJsonValue& value : artifacts) {
        if (!value.isObject()) {
            result.integrity = PackageIntegrity::Error;
            result.issues.append(QStringLiteral("manifest 含非对象工件记录"));
            continue;
        }
        const QJsonObject object = value.toObject();
        const QString path = object.value(QStringLiteral("path")).toString();
        const qint64 expectedBytes =
            static_cast<qint64>(object.value(QStringLiteral("bytes")).toDouble(-1));
        const QByteArray expectedHash =
            object.value(QStringLiteral("sha256")).toString().toLatin1().toUpper();
        if (!expected.contains(path) || seen.contains(path)
            || expectedBytes < 0 || !isSha256(expectedHash)) {
            result.integrity = PackageIntegrity::Error;
            result.issues.append(
                QStringLiteral("manifest 工件记录无效或重复：%1").arg(path));
            continue;
        }
        seen.insert(path);
        PackageArtifactInfo artifact;
        artifact.relativePath = path;
        artifact.byteSize = expectedBytes;
        artifact.sha256 = expectedHash;
        result.artifacts.append(artifact);

        QFile file(QDir(absoluteDirectory).filePath(path));
        if (!file.open(QIODevice::ReadOnly)) {
            if (result.integrity != PackageIntegrity::Error)
                result.integrity = PackageIntegrity::Warning;
            result.issues.append(
                QStringLiteral("工件缺失或不可读：%1").arg(path));
            continue;
        }
        const QByteArray bytes = file.readAll();
        if (bytes.size() != expectedBytes || sha256(bytes) != expectedHash) {
            if (result.integrity != PackageIntegrity::Error)
                result.integrity = PackageIntegrity::Warning;
            result.issues.append(
                QStringLiteral("工件大小或哈希不匹配：%1").arg(path));
        }
    }
    if (seen != expected) {
        result.integrity = PackageIntegrity::Error;
        for (const QString& missing : expected - seen) {
            result.issues.append(
                QStringLiteral("manifest 缺少工件记录：%1").arg(missing));
        }
    }

    const auto identityError = [&result](const QString& issue) {
        result.integrity = PackageIntegrity::Error;
        result.issues.append(issue);
    };
    const auto readObject =
        [&absoluteDirectory, &identityError](
            const QString& name, QJsonObject& object) {
        QFile file(QDir(absoluteDirectory).filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            identityError(
                QStringLiteral("身份工件不可读：%1").arg(name));
            return false;
        }
        QJsonParseError error;
        const QJsonDocument document =
            QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError
            || !document.isObject()) {
            identityError(
                QStringLiteral("身份工件不是有效 JSON 对象：%1")
                    .arg(name));
            return false;
        }
        object = document.object();
        return true;
    };
    const QString manifestRunId =
        manifest.value(QStringLiteral("runId")).toString();
    const QString manifestSnapshotId =
        manifest.value(QStringLiteral("snapshotId")).toString();
    const QString manifestSampleId =
        manifest.value(QStringLiteral("sampleId")).toString();
    const QString manifestTemplateId =
        manifest.value(QStringLiteral("templateId")).toString();
    const QString manifestTemplateName =
        manifest.value(QStringLiteral("templateName")).toString();
    const QString manifestSoftwareCommit =
        manifest.value(QStringLiteral("softwareCommit")).toString();
    if (manifestRunId != QFileInfo(absoluteDirectory).fileName()) {
        identityError(
            QStringLiteral("manifest runId 与结果包目录名不一致"));
    }

    QJsonObject snapshot;
    if (readObject(QStringLiteral("parameter-snapshot.json"), snapshot)) {
        const QList<QPair<QString, QString>> expectedIdentity = {
            {QStringLiteral("runId"), manifestRunId},
            {QStringLiteral("snapshotId"), manifestSnapshotId},
            {QStringLiteral("sampleId"), manifestSampleId},
            {QStringLiteral("templateId"), manifestTemplateId},
            {QStringLiteral("templateName"), manifestTemplateName},
            {QStringLiteral("softwareCommit"), manifestSoftwareCommit},
            {QStringLiteral("dataSource"), QStringLiteral("MOCK")}
        };
        for (const auto& identity : expectedIdentity) {
            if (snapshot.value(identity.first).toString()
                != identity.second) {
                identityError(
                    QStringLiteral(
                        "manifest 与参数快照身份不一致：%1")
                        .arg(identity.first));
            }
        }
    }

    QJsonObject source;
    if (readObject(QStringLiteral("mock-source.json"), source)) {
        if (source.value(QStringLiteral("runId")).toString()
                != manifestRunId
            || source.value(QStringLiteral("snapshotId")).toString()
                != manifestSnapshotId
            || source.value(QStringLiteral("dataSource")).toString()
                != QStringLiteral("MOCK")) {
            identityError(
                QStringLiteral("manifest 与 Mock 来源身份不一致"));
        }
    }

    QJsonObject qc;
    if (readObject(QStringLiteral("mock-qc.json"), qc)) {
        if (qc.value(QStringLiteral("runId")).toString()
                != manifestRunId
            || qc.value(QStringLiteral("snapshotId")).toString()
                != manifestSnapshotId
            || qc.value(QStringLiteral("dataSource")).toString()
                != QStringLiteral("MOCK")) {
            identityError(
                QStringLiteral("manifest 与 Mock QC 身份不一致"));
        }
    }

    QFile imageFile(
        QDir(absoluteDirectory)
            .filePath(QStringLiteral("standard-mock-result.png")));
    if (imageFile.open(QIODevice::ReadOnly)) {
        const QString imageHash =
            QString::fromLatin1(sha256(imageFile.readAll()));
        if (!source.isEmpty()
            && source.value(QStringLiteral("sha256")).toString().toUpper()
                != imageHash) {
            identityError(
                QStringLiteral("Mock 来源图像哈希与标准结果不一致"));
        }
        if (!qc.isEmpty()
            && qc.value(QStringLiteral("imageSha256")).toString().toUpper()
                != imageHash) {
            identityError(
                QStringLiteral("Mock QC 图像哈希与标准结果不一致"));
        }
    }

    QFile auditFile(
        QDir(absoluteDirectory).filePath(
            QStringLiteral("audit-events.json")));
    if (!auditFile.open(QIODevice::ReadOnly)) {
        identityError(QStringLiteral("审计事件不可读"));
    } else {
        QJsonParseError error;
        const QJsonDocument document =
            QJsonDocument::fromJson(auditFile.readAll(), &error);
        if (error.error != QJsonParseError::NoError
            || !document.isArray()) {
            identityError(QStringLiteral("审计事件不是有效 JSON 数组"));
        } else {
            bool packageSaved = false;
            for (const QJsonValue& value : document.array()) {
                const QJsonObject event = value.toObject();
                const QString eventRunId =
                    event.value(QStringLiteral("runId")).toString();
                if ((!eventRunId.isEmpty()
                     && eventRunId != manifestRunId)
                    || event.value(QStringLiteral("dataSource")).toString()
                        != QStringLiteral("MOCK")) {
                    identityError(
                        QStringLiteral("manifest 与审计事件身份不一致"));
                    break;
                }
                packageSaved =
                    packageSaved
                    || event.value(QStringLiteral("event")).toString()
                        == QStringLiteral("PACKAGE_SAVED");
            }
            if (!packageSaved)
                identityError(
                    QStringLiteral("审计事件缺少 PACKAGE_SAVED"));
        }
    }
    return result;
}

HistoryLoadResult MockResultPackage::loadHistory(
    const QString& rootDirectory)
{
    HistoryLoadResult result;
    const QString absoluteRoot = QDir(rootDirectory).absolutePath();
    const QFileInfo rootInfo(absoluteRoot);
    if (!rootInfo.exists()) {
        result.ok = true;
        return result;
    }
    if (!rootInfo.isDir()) {
        result.error = QStringLiteral("历史结果根不是目录：%1").arg(absoluteRoot);
        return result;
    }
    const QDir root(absoluteRoot);
    const QFileInfoList directories =
        root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& directory : directories) {
        if (directory.fileName().contains(QStringLiteral(".staging-")))
            continue;
        const QString manifestPath =
            QDir(directory.absoluteFilePath())
                .filePath(QStringLiteral("manifest.json"));
        if (!QFileInfo::exists(manifestPath))
            continue;

        const PackageVerification verification =
            verify(directory.absoluteFilePath());
        MockHistoryRecord record;
        record.packageDirectory = directory.absoluteFilePath();
        record.integrity = verification.integrity;
        record.issues = verification.issues;
        record.manifest = verification.manifest;
        record.runId =
            verification.manifest.value(QStringLiteral("runId")).toString();
        if (record.runId.isEmpty())
            record.runId = directory.fileName();
        record.snapshotId =
            verification.manifest.value(QStringLiteral("snapshotId")).toString();
        record.sampleId =
            verification.manifest.value(QStringLiteral("sampleId")).toString();
        record.templateId =
            verification.manifest.value(QStringLiteral("templateId")).toString();
        record.templateName =
            verification.manifest.value(QStringLiteral("templateName")).toString();
        record.createdAtUtc =
            verification.manifest.value(QStringLiteral("createdAtUtc")).toString();
        if (verification.integrity == PackageIntegrity::Valid) {
            record.previewImagePath =
                QDir(directory.absoluteFilePath())
                    .filePath(QStringLiteral("standard-mock-result.png"));
        }
        result.records.append(record);
    }

    std::sort(
        result.records.begin(), result.records.end(),
        [](const MockHistoryRecord& left, const MockHistoryRecord& right) {
            if (left.createdAtUtc != right.createdAtUtc)
                return left.createdAtUtc > right.createdAtUtc;
            return left.runId < right.runId;
        });
    result.ok = true;
    return result;
}
