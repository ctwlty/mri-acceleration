#include "DeviceBridge.h"

#include "ProtocolMapper.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

DeviceBridge::DeviceBridge(QObject* parent)
    : QObject(parent)
    , m_connectionState(QStringLiteral("未连接"))
    , m_transferState(QStringLiteral("未启动"))
    , m_abnormalState(QStringLiteral("无"))
    , m_temperatureState(QStringLiteral("-- C"))
    , m_scanState(QStringLiteral("待机"))
    , m_scanProgress(QStringLiteral("0/0"))
    , m_sdkModeLabel(QStringLiteral("未加载"))
    , m_sdkPathLabel(QStringLiteral("未选择 DLL"))
    , m_lastError(QStringLiteral("未加载 SDK"))
{
    connect(&m_pollTimer, &QTimer::timeout, this, &DeviceBridge::refreshStatus);
}

DeviceBridge::~DeviceBridge()
{
    m_pollTimer.stop();
    if (m_state == MriSdkSessionState::Scanning || m_state == MriSdkSessionState::Stopping) {
        m_loader.abort();
    }
    m_loader.shutdown();
}

MriSdkResult DeviceBridge::loadSdk(const QString& dllPath)
{
    m_pollTimer.stop();
    const MriSdkResult result = m_loader.load(dllPath);
    m_sdkPathLabel = dllPath.isEmpty() ? QStringLiteral("未选择 DLL") : QFileInfo(dllPath).absoluteFilePath();
    if (!result.ok) {
        m_sdkModeLabel = QStringLiteral("SDK 错误");
        m_lastError = result.message;
        m_lastErrorResult = result;
        setSessionState(MriSdkSessionState::Fault);
        syncSdkStatus();
        emit operationFailed(result);
        emit logAppended(QStringLiteral("SDK 加载失败：%1").arg(result.message));
        return result;
    }

    m_sdkModeLabel = QStringLiteral("Real SDK");
    m_lastError.clear();
    m_lastErrorResult = {};
    setSessionState(MriSdkSessionState::Loaded);
    syncSdkStatus();
    emit logAppended(QStringLiteral("SDK 已加载：%1").arg(m_sdkPathLabel));
    return result;
}

bool DeviceBridge::initialize(const QString& initPath, const QString& outputPath, const QString& parPath)
{
    MriSdkConfig config;
    config.initPath = initPath;
    config.outputPath = outputPath;
    config.parameterPath = parPath;
    return connectDevice(config).ok;
}

MriSdkResult DeviceBridge::connectDevice(const MriSdkConfig& config)
{
    if (m_state != MriSdkSessionState::Loaded) {
        return reject(QStringLiteral("connect"), QStringLiteral("precondition"), QStringLiteral("请先成功加载 SDK"));
    }

    m_config = config;
    setSessionState(MriSdkSessionState::Initializing);
    const MriSdkResult result = m_loader.initialize(config);
    if (!result.ok) {
        return fail(result.stage, result.function, result.code, result.message);
    }

    const MriSdkStatus device = m_loader.status();
    m_connectionState = QStringLiteral("连接码 %1").arg(device.connection);
    m_transferState = QStringLiteral("就绪");
    m_abnormalState = QStringLiteral("无");
    m_temperatureState = QString::number(device.temperature, 'f', 1) + QStringLiteral(" C");
    setBadges(m_connectionState, m_transferState, m_abnormalState);
    emit temperatureChanged(m_temperatureState);
    emit deviceStatusChanged(device);
    setSessionState(MriSdkSessionState::Ready);
    setScan(QStringLiteral("已连接"), QStringLiteral("%1/%2").arg(device.currentScan).arg(device.totalScans));
    m_lastError.clear();
    m_lastErrorResult = {};
    syncSdkStatus();
    emit logAppended(QStringLiteral("SDK 初始化和基线校准完成，连接码=%1，ScanStatus=%2")
                         .arg(device.connection)
                         .arg(device.scan));
    return result;
}

void DeviceBridge::connectDevice()
{
    if (m_state == MriSdkSessionState::Ready) {
        refreshStatus();
        return;
    }
    emit logAppended(QStringLiteral("请先加载 SDK 并完成初始化"));
}

void DeviceBridge::precheck()
{
    if (m_state != MriSdkSessionState::Ready) {
        emit logAppended(QStringLiteral("预检失败：设备尚未就绪"));
        return;
    }
    const MriSdkStatus device = m_loader.status();
    emit deviceStatusChanged(device);
    emit logAppended(QStringLiteral("预检：连接码=%1，温度=%2 C，ScanStatus=%3")
                         .arg(device.connection)
                         .arg(device.temperature, 0, 'f', 1)
                         .arg(device.scan));
}

void DeviceBridge::dryRunScene(const SceneTemplate& scene)
{
    const QString outputDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("dry_run_params"));
    const auto result = ProtocolMapper::generateDryRun(scene, outputDir);
    m_lastDryRunStatus = result.status;
    m_lastDryRunPath = result.filePath;
    emit sdkDiagnosticChanged(result.status, result.filePath, ProtocolMapper::diagnosticsText(result));
    emit logAppended(result.ok
                         ? QStringLiteral("SDK DRY_RUN 完成：%1").arg(result.filePath)
                         : QStringLiteral("SDK DRY_RUN 失败：%1").arg(result.summary));
}

MriSdkResult DeviceBridge::startScan()
{
    if (m_state != MriSdkSessionState::Ready) {
        return reject(QStringLiteral("run"), QStringLiteral("precondition"), QStringLiteral("设备未处于可扫描状态"));
    }

    MriSdkResult result = m_loader.prepareScan();
    if (!result.ok) {
        return fail(result.stage, result.function, result.code, result.message);
    }

    m_rawFilesBeforeScan = rawFilesInOutput();
    m_lastRawFile.clear();
    const int runCode = m_loader.run();
    if (runCode != 0) {
        return fail(QStringLiteral("run"), QStringLiteral("Run"), runCode,
            QStringLiteral("Run 失败，返回码 %1").arg(runCode));
    }

    m_scanElapsed.start();
    m_pollTimer.start(qMax(1, m_config.pollIntervalMs));
    setSessionState(MriSdkSessionState::Scanning);
    setScan(QStringLiteral("扫描中"), QStringLiteral("0/0"));
    emit logAppended(QStringLiteral("Run 已执行，开始轮询 ScanStatus"));
    return MriSdkResult::success(QStringLiteral("run"));
}

void DeviceBridge::startScan(const SceneTemplate& scene)
{
    Q_UNUSED(scene);
    static_cast<void>(startScan());
}

void DeviceBridge::pauseScan()
{
    emit logAppended(QStringLiteral("当前 SDK 未提供暂停接口，未向设备发送命令"));
}

void DeviceBridge::resumeScan()
{
    emit logAppended(QStringLiteral("当前 SDK 未提供继续接口，未向设备发送命令"));
}

void DeviceBridge::abortScan()
{
    if (m_state != MriSdkSessionState::Scanning && m_state != MriSdkSessionState::Stopping) {
        emit logAppended(QStringLiteral("当前没有可终止的扫描"));
        return;
    }
    setSessionState(MriSdkSessionState::Stopping);
    m_loader.abort();
    m_pollTimer.stop();
    setScan(QStringLiteral("已终止"), QStringLiteral("0/0"));
    setSessionState(MriSdkSessionState::Ready);
    emit logAppended(QStringLiteral("Abort 已执行，设备会话返回就绪"));
}

void DeviceBridge::refreshStatus()
{
    if (m_state != MriSdkSessionState::Scanning
        && m_state != MriSdkSessionState::Stopping
        && m_state != MriSdkSessionState::Ready) {
        return;
    }

    if (m_state == MriSdkSessionState::Scanning
        && m_scanElapsed.isValid()
        && m_scanElapsed.elapsed() > m_config.scanTimeoutMs) {
        m_loader.abort();
        m_pollTimer.stop();
        fail(QStringLiteral("scan"), QStringLiteral("timeout"), -1, QStringLiteral("扫描超时，已执行 Abort"));
        return;
    }

    const MriSdkStatus device = m_loader.status();
    m_temperatureState = QString::number(device.temperature, 'f', 1) + QStringLiteral(" C");
    emit temperatureChanged(m_temperatureState);
    emit deviceStatusChanged(device);
    setScan(QStringLiteral("ScanStatus=%1").arg(device.scan),
        QStringLiteral("%1/%2").arg(device.currentScan).arg(device.totalScans));

    if (m_state == MriSdkSessionState::Ready) {
        return;
    }
    if (device.scan == 1 || device.scan == 2 || device.scan == 4) {
        return;
    }
    if (device.scan == -1 || device.scan == 5 || device.scan == 6) {
        m_loader.abort();
        m_pollTimer.stop();
        fail(QStringLiteral("scan"), QStringLiteral("ScanStatus"), device.scan,
            QStringLiteral("扫描异常，状态码 %1，已执行 Abort").arg(device.scan));
        return;
    }
    if (device.scan == 0 || device.scan == 3) {
        m_pollTimer.stop();
        const QString rawFile = findNewRawFile();
        if (rawFile.isEmpty()) {
            fail(QStringLiteral("scan"), QStringLiteral("raw-verification"), -1,
                QStringLiteral("扫描完成，但未发现新增的非空 RAW 文件"));
            return;
        }
        m_lastRawFile = rawFile;
        setSessionState(MriSdkSessionState::Ready);
        setScan(QStringLiteral("完成"), QStringLiteral("%1/%2").arg(device.currentScan).arg(device.totalScans));
        emit rawFileReady(m_lastRawFile);
        emit logAppended(QStringLiteral("扫描完成，RAW 文件：%1").arg(m_lastRawFile));
    }
}

MriSdkSessionState DeviceBridge::sessionState() const { return m_state; }
MriSdkResult DeviceBridge::lastErrorResult() const { return m_lastErrorResult; }
QString DeviceBridge::lastRawFile() const { return m_lastRawFile; }
QString DeviceBridge::connectionState() const { return m_connectionState; }
QString DeviceBridge::transferState() const { return m_transferState; }
QString DeviceBridge::abnormalState() const { return m_abnormalState; }
QString DeviceBridge::temperatureState() const { return m_temperatureState; }
QString DeviceBridge::scanState() const { return m_scanState; }
QString DeviceBridge::scanProgress() const { return m_scanProgress; }
QString DeviceBridge::sdkModeLabel() const { return m_sdkModeLabel; }
QString DeviceBridge::sdkPathLabel() const { return m_sdkPathLabel; }
QString DeviceBridge::lastError() const { return m_lastError; }

void DeviceBridge::syncSdkStatus()
{
    emit sdkStatusChanged(m_sdkModeLabel, m_sdkPathLabel, m_lastError);
}

void DeviceBridge::applyDemoMetrics(const SceneTemplate& scene)
{
    emit metricsChanged(scene.snr, scene.uniformity, scene.peak, scene.area);
}

void DeviceBridge::setBadges(const QString& connection, const QString& transfer, const QString& abnormal)
{
    m_connectionState = connection;
    m_transferState = transfer;
    m_abnormalState = abnormal;
    emit badgesChanged(m_connectionState, m_transferState, m_abnormalState);
}

void DeviceBridge::setScan(const QString& scanState, const QString& progress)
{
    m_scanState = scanState;
    m_scanProgress = progress;
    emit scanStatusChanged(m_scanState, m_scanProgress);
}

void DeviceBridge::setSessionState(MriSdkSessionState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit sessionStateChanged(m_state);
}

MriSdkResult DeviceBridge::reject(
    const QString& stage,
    const QString& function,
    const QString& message)
{
    m_lastErrorResult = MriSdkResult::failure(stage, function, -1, message);
    m_lastError = message;
    syncSdkStatus();
    emit operationFailed(m_lastErrorResult);
    emit logAppended(message);
    return m_lastErrorResult;
}

MriSdkResult DeviceBridge::fail(
    const QString& stage,
    const QString& function,
    int code,
    const QString& message)
{
    m_lastErrorResult = MriSdkResult::failure(stage, function, code, message);
    m_lastError = message;
    m_abnormalState = message;
    setBadges(m_connectionState, m_transferState, m_abnormalState);
    setSessionState(MriSdkSessionState::Fault);
    syncSdkStatus();
    emit operationFailed(m_lastErrorResult);
    emit logAppended(QStringLiteral("%1：%2（返回码 %3）").arg(function, message).arg(code));
    return m_lastErrorResult;
}

QSet<QString> DeviceBridge::rawFilesInOutput() const
{
    QSet<QString> files;
    const QFileInfoList entries = QDir(m_config.outputPath).entryInfoList(
        QStringList {QStringLiteral("*.raw")}, QDir::Files, QDir::Time);
    for (const QFileInfo& entry : entries) {
        files.insert(entry.absoluteFilePath());
    }
    return files;
}

QString DeviceBridge::findNewRawFile() const
{
    const QFileInfoList entries = QDir(m_config.outputPath).entryInfoList(
        QStringList {QStringLiteral("*.raw")}, QDir::Files, QDir::Time);
    for (const QFileInfo& entry : entries) {
        if (!m_rawFilesBeforeScan.contains(entry.absoluteFilePath()) && entry.size() > 0) {
            return entry.absoluteFilePath();
        }
    }
    return {};
}
