#include "app/MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

static QString loadStyleSheet()
{
    QFile file(":/app.qss");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("场景化核磁共振控制台"));
    app.setApplicationDisplayName(QStringLiteral("场景化核磁共振控制台"));
    app.setStyleSheet(loadStyleSheet());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt MRI device control console"));
    parser.addHelpOption();
    const QCommandLineOption autoConnectOption(
        QStringLiteral("auto-connect"), QStringLiteral("Load the SDK and connect after the window opens."));
    const QCommandLineOption sdkOption(
        QStringLiteral("sdk"), QStringLiteral("Path to mridll.dll."), QStringLiteral("path"));
    const QCommandLineOption initOption(
        QStringLiteral("init"), QStringLiteral("Path to init.ini."), QStringLiteral("path"));
    const QCommandLineOption parameterOption(
        QStringLiteral("par"), QStringLiteral("Path to the scan parameter file."), QStringLiteral("path"),
        QStringLiteral("C:/MRIScanner/Scan/PTScan.par"));
    const QCommandLineOption outputOption(
        QStringLiteral("output"), QStringLiteral("Directory for raw scan output."), QStringLiteral("path"),
        QStringLiteral("D:/mri_data/par0423-3"));
    parser.addOptions({autoConnectOption, sdkOption, initOption, parameterOption, outputOption});
    parser.process(app);

    MainWindow window;
    window.show();

    if (parser.isSet(autoConnectOption)) {
        const QString dllPath = QFileInfo(parser.value(sdkOption)).absoluteFilePath();
        if (parser.value(sdkOption).trimmed().isEmpty()) {
            qCritical("--auto-connect requires --sdk <path>");
            return 2;
        }

        MriSdkConfig config;
        config.initPath = parser.value(initOption).trimmed();
        if (config.initPath.isEmpty()) {
            config.initPath = QFileInfo(dllPath).absoluteDir().filePath(QStringLiteral("hw_cfg/init.ini"));
        }
        config.parameterPath = QFileInfo(parser.value(parameterOption)).absoluteFilePath();
        config.outputPath = QDir(parser.value(outputOption)).absolutePath();

        QTimer::singleShot(0, &window, [&window, dllPath, config]() {
            const MriSdkResult result = window.loadSdkAndConnect(dllPath, config);
            if (!result.ok) {
                qCritical().noquote() << QStringLiteral("Automatic device connection failed: %1 (%2)")
                                             .arg(result.message)
                                             .arg(result.code);
            }
        });
    }

    return app.exec();
}

