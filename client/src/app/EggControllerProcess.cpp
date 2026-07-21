#include "EggControllerProcess.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>

EggControllerProcess::EggControllerProcess(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<EggControllerArtifacts>();
    connect(&m_process, &QProcess::readyReadStandardOutput,
            this, &EggControllerProcess::readStandardOutput);
    connect(&m_process, &QProcess::readyReadStandardError,
            this, &EggControllerProcess::readStandardError);
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &EggControllerProcess::handleFinished);
    connect(&m_process, &QProcess::errorOccurred,
            this, &EggControllerProcess::handleProcessError);
}

bool EggControllerProcess::start(const EggControllerLaunchConfig& config)
{
    if (isRunning()) {
        return false;
    }
    if (!QFileInfo(config.program).isFile() ||
        !QFileInfo(config.workingDirectory).isDir()) {
        return false;
    }

    m_stdoutBuffer.clear();
    m_pendingArtifacts = {};
    m_hasPendingArtifacts = false;
    m_terminalSignalEmitted = false;
    m_process.setWorkingDirectory(config.workingDirectory);
    m_process.setProgram(config.program);
    m_process.setArguments(config.arguments);
    m_process.start();
    return true;
}

bool EggControllerProcess::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void EggControllerProcess::readStandardOutput()
{
    m_stdoutBuffer.append(m_process.readAllStandardOutput());
    qsizetype newline = -1;
    while ((newline = m_stdoutBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_stdoutBuffer.left(newline).trimmed();
        m_stdoutBuffer.remove(0, newline + 1);
        processOutputLine(line);
    }
}

void EggControllerProcess::readStandardError()
{
    const QString text = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
    if (!text.isEmpty()) {
        emit logAppended(text);
    }
}

void EggControllerProcess::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    readStandardOutput();
    if (!m_stdoutBuffer.trimmed().isEmpty()) {
        processOutputLine(m_stdoutBuffer.trimmed());
        m_stdoutBuffer.clear();
    }

    if (m_terminalSignalEmitted) {
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        failOnce(QStringLiteral("eggcontrollerV2 代理异常退出，code=%1").arg(exitCode));
        return;
    }
    if (!m_hasPendingArtifacts) {
        failOnce(QStringLiteral("eggcontrollerV2 代理未返回完整结果"));
        return;
    }

    m_terminalSignalEmitted = true;
    emit completed(m_pendingArtifacts);
}

void EggControllerProcess::handleProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart || error == QProcess::Crashed) {
        failOnce(QStringLiteral("eggcontrollerV2 代理进程错误：%1").arg(m_process.errorString()));
    }
}

void EggControllerProcess::processOutputLine(const QByteArray& line)
{
    if (line.isEmpty()) {
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit logAppended(QString::fromUtf8(line));
        return;
    }

    const QJsonObject object = document.object();
    const QString event = object.value(QStringLiteral("event")).toString();
    if (event == QStringLiteral("stage")) {
        emit stageChanged(object.value(QStringLiteral("stage")).toString());
        return;
    }
    if (event == QStringLiteral("error")) {
        failOnce(object.value(QStringLiteral("message")).toString());
        return;
    }
    if (event != QStringLiteral("result")) {
        return;
    }

    EggControllerArtifacts artifacts;
    artifacts.taskId = object.value(QStringLiteral("task_id")).toString();
    artifacts.rawPath = object.value(QStringLiteral("raw_path")).toString();
    artifacts.kspaceImagePath = object.value(QStringLiteral("kspace_image_path")).toString();
    artifacts.finalImagePath = object.value(QStringLiteral("final_image_path")).toString();

    QString validationError;
    if (!validateArtifacts(artifacts, validationError)) {
        failOnce(validationError);
        return;
    }
    m_pendingArtifacts = artifacts;
    m_hasPendingArtifacts = true;
}

bool EggControllerProcess::validateArtifacts(
    const EggControllerArtifacts& artifacts, QString& error) const
{
    const QString taskId = artifacts.taskId.trimmed();
    if (taskId.isEmpty()) {
        error = QStringLiteral("eggcontrollerV2 未返回任务号");
        return false;
    }

    const QList<QPair<QString, QString>> files = {
        {QStringLiteral("RAW"), artifacts.rawPath},
        {QStringLiteral("K-space 图"), artifacts.kspaceImagePath},
        {QStringLiteral("最终图"), artifacts.finalImagePath}
    };
    for (const auto& file : files) {
        const QFileInfo info(file.second);
        if (!QDir::isAbsolutePath(file.second) || !info.isFile() || info.size() <= 0) {
            error = QStringLiteral("%1产物无效：%2").arg(file.first, file.second);
            return false;
        }
    }

    const QFileInfo kspaceInfo(artifacts.kspaceImagePath);
    const QFileInfo finalInfo(artifacts.finalImagePath);
    if (kspaceInfo.absolutePath() != finalInfo.absolutePath() ||
        QDir(kspaceInfo.absolutePath()).dirName() != taskId ||
        kspaceInfo.fileName() != QStringLiteral("kspace_%1.png").arg(taskId) ||
        finalInfo.fileName() != QStringLiteral("rgb._%1.png").arg(taskId)) {
        error = QStringLiteral("eggcontrollerV2 图片与任务号不一致：%1").arg(taskId);
        return false;
    }

    QImageReader kspaceReader(artifacts.kspaceImagePath);
    QImageReader finalReader(artifacts.finalImagePath);
    if (!kspaceReader.canRead() || !finalReader.canRead()) {
        error = QStringLiteral("eggcontrollerV2 返回的图片无法解码");
        return false;
    }
    return true;
}

void EggControllerProcess::failOnce(const QString& message)
{
    if (m_terminalSignalEmitted) {
        return;
    }
    m_terminalSignalEmitted = true;
    emit failed(message.isEmpty() ? QStringLiteral("eggcontrollerV2 代理失败") : message);
}
