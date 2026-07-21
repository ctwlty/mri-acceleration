#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

struct EggControllerLaunchConfig {
    QString program;
    QStringList arguments;
    QString workingDirectory;
};

struct EggControllerArtifacts {
    QString taskId;
    QString rawPath;
    QString kspaceImagePath;
    QString finalImagePath;
};

Q_DECLARE_METATYPE(EggControllerArtifacts)

class EggControllerProcess : public QObject {
    Q_OBJECT

public:
    explicit EggControllerProcess(QObject* parent = nullptr);

    bool start(const EggControllerLaunchConfig& config);
    bool isRunning() const;

signals:
    void stageChanged(const QString& stage);
    void completed(const EggControllerArtifacts& artifacts);
    void failed(const QString& message);
    void logAppended(const QString& line);

private slots:
    void readStandardOutput();
    void readStandardError();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleProcessError(QProcess::ProcessError error);

private:
    void processOutputLine(const QByteArray& line);
    bool validateArtifacts(const EggControllerArtifacts& artifacts, QString& error) const;
    void failOnce(const QString& message);

    QProcess m_process;
    QByteArray m_stdoutBuffer;
    EggControllerArtifacts m_pendingArtifacts;
    bool m_hasPendingArtifacts = false;
    bool m_terminalSignalEmitted = false;
};
