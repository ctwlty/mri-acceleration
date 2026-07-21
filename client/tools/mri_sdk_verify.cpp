#include "app/DeviceBridge.h"
#include "app/MriSdkTypes.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>

namespace {
int parsePositiveInt(const QCommandLineParser& parser, const QCommandLineOption& option, int fallback)
{
    bool ok = false;
    const int value = parser.value(option).toInt(&ok);
    return ok && value > 0 ? value : fallback;
}
}

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
    const QCommandLineOption scanOption(QStringLiteral("scan"), QStringLiteral("Run one scan after initialization"));
    const QCommandLineOption pollOption(QStringLiteral("poll-ms"), QStringLiteral("Scan status polling interval"), QStringLiteral("milliseconds"), QStringLiteral("1000"));
    const QCommandLineOption timeoutOption(QStringLiteral("timeout-ms"), QStringLiteral("Scan timeout"), QStringLiteral("milliseconds"), QStringLiteral("1800000"));
    parser.addOptions({sdkOption, initOption, parOption, outputOption, scanOption, pollOption, timeoutOption});
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
    QString rawFile;
    QObject::connect(&bridge, &DeviceBridge::logAppended, &app, [&out](const QString& line) {
        out << "LOG " << line << Qt::endl;
    });
    QObject::connect(&bridge, &DeviceBridge::deviceStatusChanged, &app, [&lastStatus](const MriSdkStatus& status) {
        lastStatus = status;
    });
    QObject::connect(&bridge, &DeviceBridge::rawFileReady, &app, [&rawFile](const QString& path) {
        rawFile = path;
    });

    MriSdkConfig config;
    config.initPath = QFileInfo(parser.value(initOption)).absoluteFilePath();
    config.parameterPath = QFileInfo(parser.value(parOption)).absoluteFilePath();
    config.outputPath = QFileInfo(parser.value(outputOption)).absoluteFilePath();
    config.outputPrefix = "PTMRIData";
    config.systemSelection = 3;
    config.pollIntervalMs = parsePositiveInt(parser, pollOption, 1000);
    config.scanTimeoutMs = parsePositiveInt(parser, timeoutOption, 30 * 60 * 1000);

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

    if (!parser.isSet(scanOption)) {
        return 0;
    }

    QEventLoop scanLoop;
    bool sawScanning = false;
    int scanExitCode = 8;
    QObject::connect(&bridge, &DeviceBridge::sessionStateChanged, &scanLoop,
        [&scanLoop, &sawScanning, &scanExitCode](MriSdkSessionState state) {
            if (state == MriSdkSessionState::Scanning) {
                sawScanning = true;
            } else if (sawScanning && state == MriSdkSessionState::Ready) {
                scanExitCode = 0;
                scanLoop.quit();
            } else if (state == MriSdkSessionState::Fault) {
                scanExitCode = 6;
                scanLoop.quit();
            }
        });

    bridge.selectExecutionGate(ExecutionGate::VerifiedBaseline);
    if (!bridge.precheck().passed) {
        err << "ERROR baseline precheck failed" << Qt::endl;
        return 5;
    }
    result = bridge.startScan(bridge.executionContext());
    if (!result.ok) {
        err << "ERROR run function=" << result.function << " code=" << result.code
            << " message=" << result.message << Qt::endl;
        return 5;
    }
    sawScanning = true;

    QTimer hardTimeout;
    hardTimeout.setSingleShot(true);
    QObject::connect(&hardTimeout, &QTimer::timeout, &scanLoop, [&scanLoop, &scanExitCode]() {
        scanExitCode = 7;
        scanLoop.quit();
    });
    hardTimeout.start(config.scanTimeoutMs + config.rawSettleTimeoutMs
        + qMax(5000, config.pollIntervalMs * 2));
    scanLoop.exec();

    if (scanExitCode != 0) {
        const MriSdkResult failure = bridge.lastErrorResult();
        err << "ERROR scan function=" << failure.function << " code=" << failure.code
            << " message=" << failure.message << Qt::endl;
        return scanExitCode;
    }
    out << "SCAN_COMPLETED raw=" << rawFile << Qt::endl;
    return 0;
}
