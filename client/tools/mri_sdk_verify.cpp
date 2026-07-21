#include "app/DeviceBridge.h"
#include "app/MriSdkTypes.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QStringConverter>
#include <QTextStream>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("mri_sdk_verify"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Native Qt/C++ MRI SDK verification tool"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption sdkOption(QStringLiteral("sdk"), QStringLiteral("Path to mridll.dll"), QStringLiteral("path"));
    const QCommandLineOption initOption(QStringLiteral("init"), QStringLiteral("Path to init.ini"), QStringLiteral("path"));
    const QCommandLineOption parOption(QStringLiteral("par"), QStringLiteral("Path to PTScan.par"), QStringLiteral("path"));
    const QCommandLineOption outputOption(QStringLiteral("output"), QStringLiteral("RAW output directory"), QStringLiteral("path"));
    parser.addOptions({sdkOption, initOption, parOption, outputOption});
    parser.process(app);

    QTextStream out(stdout);
    QTextStream err(stderr);
    out.setEncoding(QStringConverter::Utf8);
    err.setEncoding(QStringConverter::Utf8);

    if (parser.value(sdkOption).isEmpty()
        || parser.value(initOption).isEmpty()
        || parser.value(parOption).isEmpty()
        || parser.value(outputOption).isEmpty()) {
        err << "ERROR missing required --sdk/--init/--par/--output option" << Qt::endl;
        return 2;
    }

    qRegisterMetaType<MriSdkSessionState>();
    qRegisterMetaType<MriSdkResult>();
    qRegisterMetaType<MriSdkStatus>();

    DeviceBridge bridge;
    MriSdkStatus lastStatus;
    QObject::connect(&bridge, &DeviceBridge::logAppended, &app, [&out](const QString& line) {
        out << "LOG " << line << Qt::endl;
    });
    QObject::connect(&bridge, &DeviceBridge::deviceStatusChanged, &app, [&lastStatus](const MriSdkStatus& status) {
        lastStatus = status;
    });
    MriSdkConfig config;
    config.initPath = QFileInfo(parser.value(initOption)).absoluteFilePath();
    config.parameterPath = QFileInfo(parser.value(parOption)).absoluteFilePath();
    config.outputPath = QFileInfo(parser.value(outputOption)).absoluteFilePath();
    config.outputPrefix = "PTMRIData";
    config.systemSelection = 3;

    MriSdkResult result = bridge.loadSdk(QFileInfo(parser.value(sdkOption)).absoluteFilePath());
    if (!result.ok) {
        err << "ERROR load function=" << result.function << " code=" << result.code
            << " message=" << result.message << Qt::endl;
        return 3;
    }
    result = bridge.connectDevice(config);
    if (!result.ok) {
        err << "ERROR initialize function=" << result.function << " code=" << result.code
            << " message=" << result.message << Qt::endl;
        return 4;
    }

    out << "INITIALIZED connection=" << lastStatus.connection
        << " temperature=" << lastStatus.temperature
        << " scanStatus=" << lastStatus.scan
        << " current=" << lastStatus.currentScan
        << " total=" << lastStatus.totalScans << Qt::endl;

    return 0;
}
