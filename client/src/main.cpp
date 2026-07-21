#include "app/MainWindow.h"
#include "app/MriRuntimeResolver.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QStringList>
#include <QTimer>

static QString loadStyleSheet()
{
    QFile file(":/app.qss");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

#ifdef MRI_RUNTIME_RESOLVER_TESTING
static MriRuntimePaths resolveRuntimeForTest(
    const QString& applicationDir,
    const MriRuntimeOverrides& overrides)
{
    const QStringList values = qEnvironmentVariable("MRI_RUNTIME_TEST_EXPECTATIONS").split(QLatin1Char('|'));
    if (values.size() != 6) {
        return MriRuntimeResolver::resolve(applicationDir, overrides);
    }
    bool fileCountOk = false;
    bool totalBytesOk = false;
    MriRuntimeExpectations expectations;
    expectations.dllSha256 = values.at(0);
    expectations.initSha256 = values.at(1);
    expectations.parameterSha256 = values.at(2);
    expectations.hwCfgFileCount = values.at(3).toInt(&fileCountOk);
    expectations.hwCfgTotalBytes = values.at(4).toLongLong(&totalBytesOk);
    expectations.hwCfgManifestSha256 = values.at(5);
    if (!fileCountOk || !totalBytesOk) {
        return MriRuntimeResolver::resolve(applicationDir, overrides);
    }
    return MriRuntimeResolver::resolveForTesting(applicationDir, overrides, expectations);
}

static void writeRuntimeTestTrace(const QString& message)
{
    const QString path = qEnvironmentVariable("MRI_RUNTIME_TEST_TRACE_FILE");
    if (path.isEmpty()) {
        return;
    }
    QFile trace(path);
    if (trace.open(QIODevice::WriteOnly | QIODevice::Text)) {
        trace.write(message.toUtf8());
    }
}
#endif

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
        QStringLiteral("par"), QStringLiteral("Path to the scan parameter file."), QStringLiteral("path"));
    const QCommandLineOption outputOption(
        QStringLiteral("output"), QStringLiteral("Directory for raw scan output."), QStringLiteral("path"));
    parser.addOptions({autoConnectOption, sdkOption, initOption, parameterOption, outputOption});
    parser.process(app);

    MriRuntimeOverrides overrides;
    overrides.sdkPath = parser.value(sdkOption);
    overrides.initPath = parser.value(initOption);
    overrides.parameterPath = parser.value(parameterOption);
    overrides.outputPath = parser.value(outputOption);
#ifdef MRI_RUNTIME_RESOLVER_TESTING
    const MriRuntimePaths runtimePaths = resolveRuntimeForTest(app.applicationDirPath(), overrides);
#else
    const MriRuntimePaths runtimePaths = MriRuntimeResolver::resolve(app.applicationDirPath(), overrides);
#endif

    MainWindow window(runtimePaths.runtimeDirectory);
    window.show();

    if (parser.isSet(autoConnectOption)) {
        if (!runtimePaths.isValid()) {
            qCritical().noquote() << QStringLiteral("Automatic device connection skipped: %1").arg(runtimePaths.error);
#ifdef MRI_RUNTIME_RESOLVER_TESTING
            writeRuntimeTestTrace(QStringLiteral("Automatic device connection skipped: %1").arg(runtimePaths.error));
#endif
            QMetaObject::invokeMethod(
                &window,
                "appendLog",
                Qt::QueuedConnection,
                Q_ARG(QString, QStringLiteral("MRI runtime error: %1").arg(runtimePaths.error)));
        } else {
            MriSdkConfig config;
            config.initPath = runtimePaths.initPath;
            config.parameterPath = runtimePaths.parameterPath;
            config.outputPath = runtimePaths.outputPath;
            config.verifiedRuntimeAndParameterIdentity = runtimePaths.verifiedRuntimeAndParameterIdentity;
            QTimer::singleShot(0, &window, [&window, runtimePaths, config]() {
                const MriSdkResult result = window.loadSdkAndConnect(runtimePaths.sdkPath, config);
                if (!result.ok) {
                    qCritical().noquote() << QStringLiteral("Automatic device connection failed: %1 (%2)")
                                                 .arg(result.message)
                                                 .arg(result.code);
                }
            });
        }
    }

    return app.exec();
}
