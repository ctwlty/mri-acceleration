#include "DeviceBridge.h"

#include "ProtocolMapper.h"

#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QStringList>

DeviceBridge::DeviceBridge(QObject* parent)
    : QObject(parent)
    , m_connectionState("未连接")
    , m_transferState("普通接收")
    , m_abnormalState("无")
    , m_temperatureState("31.4 C")
    , m_scanState("待机")
    , m_scanProgress("0/0")
    , m_sdkModeLabel("Demo")
    , m_sdkPathLabel("未选择 DLL")
    , m_lastError("未加载 SDK")
{
}

bool DeviceBridge::loadSdk(const QString& dllPath)
{
    const bool ok = m_loader.load(dllPath);
    m_sdkModeLabel = m_loader.mode() == MriSdkLoader::Mode::Real ? QStringLiteral("Real SDK") : QStringLiteral("Demo");
    m_sdkPathLabel = m_loader.dllPath().isEmpty() ? QStringLiteral("未选择 DLL") : m_loader.dllPath();
    m_lastError = m_loader.lastError();
    syncSdkStatus();
    emit logAppended(ok
                         ? QStringLiteral("SDK 已加载：%1").arg(m_sdkModeLabel)
                         : QStringLiteral("SDK 回退到 Demo：%1").arg(m_lastError));
    return ok;
}

bool DeviceBridge::initialize(const QString& initPath, const QString& outputPath, const QString& parPath)
{
    const bool ok = m_loader.initialize(initPath, outputPath, parPath, true);
    m_sdkModeLabel = m_loader.mode() == MriSdkLoader::Mode::Real ? QStringLiteral("Real SDK") : QStringLiteral("Demo");
    m_sdkPathLabel = m_loader.dllPath().isEmpty() ? QStringLiteral("未选择 DLL") : m_loader.dllPath();
    m_lastError = m_loader.lastError();
    syncSdkStatus();
    emit logAppended(ok
                         ? QStringLiteral("Init 成功：%1").arg(initPath)
                         : QStringLiteral("初始化失败：%1").arg(m_lastError));
    return ok;
}

void DeviceBridge::connectDevice()
{
    if (m_loader.mode() == MriSdkLoader::Mode::Real) {
        setBadges("已连接", m_transferState, m_abnormalState);
        setScan("已连接", "准备就绪");
        const double temp = m_loader.temperature();
        m_temperatureState = QString::number(temp, 'f', 1) + QStringLiteral(" C");
        emit temperatureChanged(m_temperatureState);
        emit logAppended(QStringLiteral("Init/ConfigFile 运行完成，设备连接正常"));
        return;
    }

    setBadges("已连接", "Demo 接收", "无");
    setScan("已连接", "准备就绪");
    m_temperatureState = QStringLiteral("31.4 C");
    emit temperatureChanged(m_temperatureState);
    emit logAppended(QStringLiteral("Demo 模式：设备连接已模拟"));
}

void DeviceBridge::precheck()
{
    emit logAppended(QStringLiteral("预检完成：样品、线圈、存储与设备状态已检查"));
    emit logAppended(QStringLiteral("建议先执行 DRY_RUN，确认 SDK 字段白名单与参数文件预览"));
    emit logAppended(QStringLiteral("真实 Run 仍受模板 HOLD 门禁控制"));
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
    for (const auto& line : result.diagnostics) {
        emit logAppended(QStringLiteral("DRY_RUN：%1").arg(line));
    }
}

void DeviceBridge::startScan(const SceneTemplate& scene)
{
    if (m_loader.mode() == MriSdkLoader::Mode::Real) {
        if (scene.runGate == QStringLiteral("HOLD")) {
            setScan(QStringLiteral("HOLD"), QStringLiteral("0/0"));
            emit logAppended(QStringLiteral("真实 Run HOLD：%1 仍为%2").arg(scene.name, scene.adaptationStatus));
            emit logAppended(QStringLiteral("请完成设备适配、SDK 字段映射和采集验收后再解除 HOLD"));
            return;
        }
        if (m_loader.prepareForScene(scene) != 0) {
            m_lastError = QStringLiteral("场景准备失败");
            syncSdkStatus();
            emit logAppended(QStringLiteral("场景准备失败"));
            return;
        }
        if (m_loader.run() != 0) {
            m_lastError = QStringLiteral("Run 执行失败");
            syncSdkStatus();
            emit logAppended(QStringLiteral("Run 执行失败"));
            return;
        }
        setScan("扫描中", QStringLiteral("%1/%2").arg(m_loader.currentScanNo()).arg(m_loader.totalScanNo()));
        emit logAppended(QStringLiteral("Run 已执行，等待扫描完成"));
        return;
    }

    setScan("Demo执行", QStringLiteral("1/8"));
    applyDemoMetrics(scene);
    emit logAppended(QStringLiteral("Demo 模式：%1 已执行 UI 流程").arg(scene.name));
    emit logAppended(QStringLiteral("参数预设 %1，真实 Run=%2，SDK 映射=%3")
                         .arg(scene.presetVersion, scene.runGate, scene.sdkMappingStatus));
    emit logAppended(QStringLiteral("协议参数当前仅显示，不写入 SDK 控制字段"));
    emit logAppended(QStringLiteral("QC 指标已刷新，结果交接包等待用户确认"));
}

void DeviceBridge::pauseScan()
{
    setScan("已暂停", m_scanProgress);
    emit logAppended("Pause 已执行");
}

void DeviceBridge::resumeScan()
{
    setScan("扫描中", m_scanProgress);
    emit logAppended("Continue 已执行");
}

void DeviceBridge::abortScan()
{
    m_loader.abort();
    setScan("已终止", "0/0");
    emit logAppended("Abort 已执行，系统回到待机状态");
}

QString DeviceBridge::connectionState() const
{
    return m_connectionState;
}

QString DeviceBridge::transferState() const
{
    return m_transferState;
}

QString DeviceBridge::abnormalState() const
{
    return m_abnormalState;
}

QString DeviceBridge::temperatureState() const
{
    return m_temperatureState;
}

QString DeviceBridge::scanState() const
{
    return m_scanState;
}

QString DeviceBridge::scanProgress() const
{
    return m_scanProgress;
}

QString DeviceBridge::sdkModeLabel() const
{
    return m_sdkModeLabel;
}

QString DeviceBridge::sdkPathLabel() const
{
    return m_sdkPathLabel;
}

QString DeviceBridge::lastError() const
{
    return m_lastError;
}

void DeviceBridge::syncSdkStatus()
{
    emit sdkStatusChanged(m_sdkModeLabel, m_sdkPathLabel, m_lastError);
}

void DeviceBridge::applyDemoMetrics(const SceneTemplate& scene)
{
    m_lastError.clear();
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
