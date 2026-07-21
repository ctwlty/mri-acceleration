#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QThread>

namespace {
QString argumentValue(const QStringList& arguments, const QString& name)
{
    const qsizetype index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size() ? arguments.at(index + 1) : QString{};
}

void writeEvent(const QJsonObject& event)
{
    QTextStream(stdout) << QJsonDocument(event).toJson(QJsonDocument::Compact) << Qt::endl;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QString outputRoot = argumentValue(app.arguments(), QStringLiteral("--output"));
    const QString mode = argumentValue(app.arguments(), QStringLiteral("--mode"));
    if (outputRoot.isEmpty()) {
        return 2;
    }

    QDir().mkpath(outputRoot);
    const QString taskRoot = QDir(outputRoot).filePath(QStringLiteral("test"));
    QDir().mkpath(taskRoot);
    const QString rawPath = QDir(outputRoot).filePath(QStringLiteral("PTMRIData_test.raw"));
    const QString kspacePath = QDir(taskRoot).filePath(QStringLiteral("kspace_test.png"));
    const QString finalPath = QDir(taskRoot).filePath(QStringLiteral("rgb._test.png"));

    QFile raw(rawPath);
    if (!raw.open(QIODevice::WriteOnly) || raw.write("MRI_RAW_TEST_DATA") <= 0) {
        return 3;
    }
    raw.close();

    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::white);
    if (!image.save(kspacePath) || !image.save(finalPath)) {
        return 4;
    }

    if (mode == QStringLiteral("slow")) {
        QThread::msleep(500);
    }

    writeEvent({{QStringLiteral("event"), QStringLiteral("stage")},
                {QStringLiteral("stage"), QStringLiteral("automation-entry-returned")}});
    if (mode == QStringLiteral("success") || mode == QStringLiteral("slow") ||
        mode == QStringLiteral("mismatch")) {
        writeEvent({{QStringLiteral("event"), QStringLiteral("result")},
                    {QStringLiteral("task_id"), mode == QStringLiteral("mismatch")
                                                    ? QStringLiteral("other")
                                                    : QStringLiteral("test")},
                    {QStringLiteral("raw_path"), QFileInfo(rawPath).absoluteFilePath()},
                    {QStringLiteral("kspace_image_path"), QFileInfo(kspacePath).absoluteFilePath()},
                    {QStringLiteral("final_image_path"), QFileInfo(finalPath).absoluteFilePath()}});
    }
    return 0;
}
