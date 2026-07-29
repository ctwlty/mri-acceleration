#include "MockWorkflow.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QUuid>

#include <cmath>
#include <utility>

namespace {
bool isSha256(const QByteArray& value)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9A-Fa-f]{64}$"));
    return pattern.match(QString::fromLatin1(value)).hasMatch();
}

QDateTime normalizedUtc(const QDateTime& value)
{
    return value.isValid() ? value.toUTC() : QDateTime::currentDateTimeUtc();
}
}

QString dataSourceKindName(DataSourceKind kind)
{
    switch (kind) {
    case DataSourceKind::Mock:
        return QStringLiteral("MOCK");
    case DataSourceKind::HistoricalRaw:
        return QStringLiteral("HISTORICAL_RAW");
    case DataSourceKind::LiveBlocked:
        return QStringLiteral("LIVE_BLOCKED");
    }
    return QStringLiteral("UNKNOWN");
}

QString mockWorkflowStateName(MockWorkflowState state)
{
    switch (state) {
    case MockWorkflowState::Empty:
        return QStringLiteral("Empty");
    case MockWorkflowState::Prepared:
        return QStringLiteral("Prepared");
    case MockWorkflowState::Running:
        return QStringLiteral("Running");
    case MockWorkflowState::Paused:
        return QStringLiteral("Paused");
    case MockWorkflowState::Cancelled:
        return QStringLiteral("Cancelled");
    case MockWorkflowState::Processing:
        return QStringLiteral("Processing");
    case MockWorkflowState::Reconstructed:
        return QStringLiteral("Reconstructed");
    case MockWorkflowState::QcReady:
        return QStringLiteral("QcReady");
    case MockWorkflowState::Packaged:
        return QStringLiteral("Packaged");
    case MockWorkflowState::Failed:
        return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

MockActionResult MockActionResult::success()
{
    return {true, {}};
}

MockActionResult MockActionResult::failure(const QString& error)
{
    return {false, error};
}

QStringList MockParameterDraft::validationErrors() const
{
    QStringList errors;
    if (scene.trimmed().isEmpty())
        errors.append(QStringLiteral("场景为空"));
    if (object.trimmed().isEmpty())
        errors.append(QStringLiteral("检测对象为空"));
    if (sampleId.trimmed().isEmpty())
        errors.append(QStringLiteral("样品 ID 为空"));
    if (templateId.trimmed().isEmpty() || templateName.trimmed().isEmpty())
        errors.append(QStringLiteral("模板身份不完整"));
    if (protocolChain.isEmpty())
        errors.append(QStringLiteral("协议链为空"));
    if (orientation != QStringLiteral("横断"))
        errors.append(QStringLiteral("主路径方向必须为横断"));
    if (imagingTarget.trimmed().isEmpty())
        errors.append(QStringLiteral("成像目标为空"));
    if (!(fovReadMm > 0.0) || !(fovPhaseMm > 0.0))
        errors.append(QStringLiteral("FOV 必须大于零"));
    if (matrixRead <= 0 || matrixPhase <= 0)
        errors.append(QStringLiteral("矩阵必须大于零"));
    if (!(trMs > 0.0) || !(teMs > 0.0) || !(teMs < trMs))
        errors.append(QStringLiteral("TR/TE 无效，必须满足 0 < TE < TR"));
    if (!(sliceThicknessMm > 0.0) || sliceCount <= 0)
        errors.append(QStringLiteral("层厚和层数必须大于零"));
    if (!std::isfinite(sliceGapMm) || sliceGapMm < 0.0)
        errors.append(QStringLiteral("层间距不能小于零"));
    if (nex <= 0)
        errors.append(QStringLiteral("NEX 必须大于零"));
    if (!std::isfinite(coverageX) || !std::isfinite(coverageY)
        || !std::isfinite(coverageWidth)
        || !std::isfinite(coverageHeight)
        || coverageX < 0.0 || coverageY < 0.0
        || coverageWidth <= 0.0 || coverageHeight <= 0.0
        || coverageX + coverageWidth > 1.0
        || coverageY + coverageHeight > 1.0) {
        errors.append(QStringLiteral("定位覆盖框无效"));
    }
    if (!std::isfinite(coverageCenterX)
        || !std::isfinite(coverageCenterY)
        || !std::isfinite(slicePosition)
        || coverageCenterX < 0.0 || coverageCenterX > 1.0
        || coverageCenterY < 0.0 || coverageCenterY > 1.0
        || slicePosition < 0.0 || slicePosition > 1.0) {
        errors.append(QStringLiteral("定位中心或切片位置无效"));
    }
    if (std::isfinite(coverageCenterX)
        && std::isfinite(coverageCenterY)
        && std::isfinite(coverageX)
        && std::isfinite(coverageY)
        && std::isfinite(coverageWidth)
        && std::isfinite(coverageHeight)
        && (coverageCenterX < coverageX
            || coverageCenterX > coverageX + coverageWidth
            || coverageCenterY < coverageY
            || coverageCenterY > coverageY + coverageHeight)) {
        errors.append(
            QStringLiteral("定位中心必须位于覆盖框内"));
    }
    if (outputRoot.trimmed().isEmpty())
        errors.append(QStringLiteral("结果根目录为空"));
    return errors;
}

QJsonObject MockParameterDraft::toJson() const
{
    QJsonArray protocols;
    for (const QString& protocol : protocolChain)
        protocols.append(protocol);
    return {
        {QStringLiteral("scene"), scene},
        {QStringLiteral("object"), object},
        {QStringLiteral("sampleId"), sampleId},
        {QStringLiteral("templateId"), templateId},
        {QStringLiteral("templateName"), templateName},
        {QStringLiteral("protocolChain"), protocols},
        {QStringLiteral("orientation"), orientation},
        {QStringLiteral("imagingTarget"), imagingTarget},
        {QStringLiteral("fovReadMm"), fovReadMm},
        {QStringLiteral("fovPhaseMm"), fovPhaseMm},
        {QStringLiteral("matrixRead"), matrixRead},
        {QStringLiteral("matrixPhase"), matrixPhase},
        {QStringLiteral("trMs"), trMs},
        {QStringLiteral("teMs"), teMs},
        {QStringLiteral("sliceThicknessMm"), sliceThicknessMm},
        {QStringLiteral("sliceGapMm"), sliceGapMm},
        {QStringLiteral("sliceCount"), sliceCount},
        {QStringLiteral("nex"), nex},
        {QStringLiteral("readPhaseSwapped"), readPhaseSwapped},
        {QStringLiteral("planningCoverageModified"),
         planningCoverageModified},
        {QStringLiteral("coverageX"), coverageX},
        {QStringLiteral("coverageY"), coverageY},
        {QStringLiteral("coverageWidth"), coverageWidth},
        {QStringLiteral("coverageHeight"), coverageHeight},
        {QStringLiteral("coverageCenterX"), coverageCenterX},
        {QStringLiteral("coverageCenterY"), coverageCenterY},
        {QStringLiteral("slicePosition"), slicePosition},
        {QStringLiteral("outputRoot"), outputRoot}
    };
}

QStringList MockPreparationEvidence::missingReasons() const
{
    QStringList reasons;
    if (!preparationConfirmed)
        reasons.append(QStringLiteral("Mock 水模预设尚未确认"));
    if (!protocolConfirmed)
        reasons.append(QStringLiteral("Mock 协议草稿尚未确认"));
    if (!localizationConfirmed)
        reasons.append(QStringLiteral("Mock 横断位定位尚未确认"));
    if (!outputRootWritable) {
        reasons.append(
            outputRootError.trimmed().isEmpty()
                ? QStringLiteral("结果根目录不可写")
                : outputRootError);
    }
    return reasons;
}

bool MockStartConfirmations::allConfirmed() const
{
    return mockSourceConfirmed && outputConfirmed
        && noDeviceSideEffectsConfirmed;
}

QStringList MockStartConfirmations::missingReasons() const
{
    QStringList reasons;
    if (!mockSourceConfirmed)
        reasons.append(QStringLiteral("未确认数据源为 MOCK"));
    if (!outputConfirmed)
        reasons.append(QStringLiteral("未确认结果根目录"));
    if (!noDeviceSideEffectsConfirmed)
        reasons.append(QStringLiteral("未确认不会触发 SDK、设备、Run 或 Abort"));
    return reasons;
}

QJsonObject MockParameterSnapshot::toJson() const
{
    QJsonObject result = parameters.toJson();
    result.insert(QStringLiteral("snapshotId"), snapshotId);
    result.insert(QStringLiteral("dataSource"), dataSourceKindName(dataSource));
    return result;
}

QJsonObject MockAuditEvent::toJson() const
{
    QJsonObject result {
        {QStringLiteral("event"), name},
        {QStringLiteral("occurredAtUtc"),
         occurredAtUtc.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("runId"), runId},
        {QStringLiteral("state"), mockWorkflowStateName(state)},
        {QStringLiteral("dataSource"), dataSourceKindName(dataSource)}
    };
    if (!details.isEmpty())
        result.insert(QStringLiteral("details"), details);
    return result;
}

bool MockReconstructionArtifact::isValid() const
{
    return !logicalSource.trimmed().isEmpty() && byteSize > 0
        && isSha256(pngSha256);
}

QJsonObject MockReconstructionArtifact::toJson() const
{
    return {
        {QStringLiteral("logicalSource"), logicalSource},
        {QStringLiteral("byteSize"), static_cast<double>(byteSize)},
        {QStringLiteral("sha256"), QString::fromLatin1(pngSha256.toUpper())}
    };
}

bool MockQcMetrics::isValid() const
{
    return std::isfinite(snrDb) && std::isfinite(uniformityPercent)
        && uniformityPercent >= 0.0 && uniformityPercent <= 100.0
        && objectSizePixels.width() > 0 && objectSizePixels.height() > 0
        && isSha256(imageSha256);
}

QJsonObject MockQcMetrics::toJson() const
{
    return {
        {QStringLiteral("snrDb"), snrDb},
        {QStringLiteral("uniformityPercent"), uniformityPercent},
        {QStringLiteral("objectWidthPixels"), objectSizePixels.width()},
        {QStringLiteral("objectHeightPixels"), objectSizePixels.height()},
        {QStringLiteral("imageSha256"),
         QString::fromLatin1(imageSha256.toUpper())},
        {QStringLiteral("scope"),
         QStringLiteral("MOCK 图像级只读评估；科研结论由人确认")}
    };
}

MockWorkflow::MockWorkflow(MockWorkflowDependencies dependencies)
    : m_dependencies(std::move(dependencies))
{
    if (!m_dependencies.nextRunId) {
        m_dependencies.nextRunId = [] {
            return QStringLiteral("RUN-MOCK-%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper());
        };
    }
    if (!m_dependencies.nextSnapshotId) {
        m_dependencies.nextSnapshotId = [] {
            return QStringLiteral("SNAP-MOCK-%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper());
        };
    }
    if (!m_dependencies.nowUtc)
        m_dependencies.nowUtc = [] { return QDateTime::currentDateTimeUtc(); };
}

MockActionResult MockWorkflow::selectDataSource(DataSourceKind kind)
{
    if (m_state != MockWorkflowState::Empty
        && m_state != MockWorkflowState::Prepared) {
        return MockActionResult::failure(
            QStringLiteral("活动运行期间不能切换数据源"));
    }
    m_dataSource = kind;
    m_lastError.clear();
    return MockActionResult::success();
}

MockActionResult MockWorkflow::prepare(
    const MockParameterDraft& draft,
    const MockPreparationEvidence& preparation)
{
    if (m_state != MockWorkflowState::Empty
        && m_state != MockWorkflowState::Prepared) {
        return MockActionResult::failure(
            QStringLiteral("当前状态不能重新准备 Mock 运行"));
    }
    const QStringList errors = draft.validationErrors();
    if (!errors.isEmpty())
        return MockActionResult::failure(errors.join(QStringLiteral("；")));
    const QStringList preparationBlocks = preparation.missingReasons();
    if (!preparationBlocks.isEmpty())
        return MockActionResult::failure(
            preparationBlocks.join(QStringLiteral("；")));
    m_draft = draft;
    m_preparation = preparation;
    m_state = MockWorkflowState::Prepared;
    m_lastError.clear();
    return MockActionResult::success();
}

MockActionResult MockWorkflow::start(
    const MockStartConfirmations& confirmations)
{
    if (m_state != MockWorkflowState::Prepared)
        return MockActionResult::failure(QStringLiteral("Mock 运行尚未准备完成"));
    const QStringList sourceBlocks = executionBlockReasons();
    if (!sourceBlocks.isEmpty())
        return MockActionResult::failure(sourceBlocks.join(QStringLiteral("；")));
    const QStringList confirmationBlocks = confirmations.missingReasons();
    if (!confirmationBlocks.isEmpty()) {
        return MockActionResult::failure(
            confirmationBlocks.join(QStringLiteral("；")));
    }

    const QString nextRunId = m_dependencies.nextRunId().trimmed();
    const QString nextSnapshotId = m_dependencies.nextSnapshotId().trimmed();
    if (nextRunId.isEmpty() || nextSnapshotId.isEmpty()) {
        return MockActionResult::failure(
            QStringLiteral("无法生成唯一 run ID 或 snapshot ID"));
    }

    m_runId = nextRunId;
    m_snapshot.snapshotId = nextSnapshotId;
    m_snapshot.dataSource = m_dataSource;
    m_snapshot.parameters = m_draft;
    m_progressPercent = 0;
    clearResultData();
    m_state = MockWorkflowState::Running;
    m_lastError.clear();
    appendAudit(QStringLiteral("SNAPSHOT_FROZEN"), m_state,
                {{QStringLiteral("snapshotId"), m_snapshot.snapshotId}});
    appendAudit(QStringLiteral("MOCK_RUN_STARTED"), m_state);
    return MockActionResult::success();
}

MockActionResult MockWorkflow::setProgress(int percent)
{
    if (m_state != MockWorkflowState::Running)
        return MockActionResult::failure(QStringLiteral("只有 Running 状态可更新进度"));
    if (percent < m_progressPercent || percent < 0 || percent > 100)
        return MockActionResult::failure(QStringLiteral("Mock 进度必须在 0–100 内单调递增"));
    m_progressPercent = percent;
    appendAudit(QStringLiteral("MOCK_RUN_PROGRESS"), m_state,
                {{QStringLiteral("percent"), percent}});
    if (percent == 100) {
        m_state = MockWorkflowState::Processing;
        appendAudit(QStringLiteral("MOCK_RUN_COMPLETED"), m_state);
        appendAudit(QStringLiteral("MOCK_PROCESSING_STARTED"), m_state);
    }
    return MockActionResult::success();
}

MockActionResult MockWorkflow::pause()
{
    if (m_state != MockWorkflowState::Running)
        return MockActionResult::failure(QStringLiteral("只有 Running 状态可暂停"));
    m_state = MockWorkflowState::Paused;
    appendAudit(QStringLiteral("MOCK_RUN_PAUSED"), m_state);
    return MockActionResult::success();
}

MockActionResult MockWorkflow::resume()
{
    if (m_state != MockWorkflowState::Paused)
        return MockActionResult::failure(QStringLiteral("只有 Paused 状态可继续"));
    m_state = MockWorkflowState::Running;
    appendAudit(QStringLiteral("MOCK_RUN_RESUMED"), m_state);
    return MockActionResult::success();
}

MockActionResult MockWorkflow::cancel()
{
    if (m_state != MockWorkflowState::Running
        && m_state != MockWorkflowState::Paused) {
        return MockActionResult::failure(
            QStringLiteral("只有 Running 或 Paused 状态可取消"));
    }
    clearResultData();
    m_state = MockWorkflowState::Cancelled;
    m_lastError = QStringLiteral("Mock 运行已取消");
    appendAudit(QStringLiteral("MOCK_RUN_CANCELLED"), m_state);
    return MockActionResult::success();
}

MockActionResult MockWorkflow::fail(const QString& error)
{
    if (m_state == MockWorkflowState::Empty
        || m_state == MockWorkflowState::Prepared
        || m_state == MockWorkflowState::Cancelled
        || m_state == MockWorkflowState::Packaged
        || m_state == MockWorkflowState::Failed) {
        return MockActionResult::failure(QStringLiteral("当前状态不能记录运行失败"));
    }
    const QString failure =
        error.trimmed().isEmpty() ? QStringLiteral("Mock 工作流失败") : error;
    clearResultData();
    m_state = MockWorkflowState::Failed;
    m_lastError = failure;
    appendAudit(QStringLiteral("MOCK_RUN_FAILED"), m_state,
                {{QStringLiteral("error"), failure}});
    return MockActionResult::success();
}

MockActionResult MockWorkflow::retryProcessing()
{
    if (m_state != MockWorkflowState::Failed || m_runId.isEmpty()
        || m_snapshot.snapshotId.isEmpty()) {
        return MockActionResult::failure(
            QStringLiteral("当前失败状态没有可重试的 Mock 处理身份"));
    }
    clearResultData();
    m_state = MockWorkflowState::Processing;
    m_lastError.clear();
    appendAudit(QStringLiteral("MOCK_PROCESSING_RETRIED"), m_state);
    return MockActionResult::success();
}

MockActionResult MockWorkflow::bindReconstruction(
    const MockReconstructionArtifact& reconstruction)
{
    if (m_state != MockWorkflowState::Processing)
        return MockActionResult::failure(QStringLiteral("Mock 处理尚未就绪"));
    if (!reconstruction.isValid()) {
        const QString error = QStringLiteral("Mock 重建资产身份或哈希无效");
        fail(error);
        return MockActionResult::failure(error);
    }
    m_reconstruction = reconstruction;
    m_qc.reset();
    m_state = MockWorkflowState::Reconstructed;
    m_lastError.clear();
    appendAudit(QStringLiteral("MOCK_ASSET_BOUND"), m_state,
                reconstruction.toJson());
    appendAudit(QStringLiteral("MOCK_RECONSTRUCTED"), m_state);
    return MockActionResult::success();
}

MockActionResult MockWorkflow::recordQcSuccess(const MockQcMetrics& qc)
{
    if (m_state != MockWorkflowState::Reconstructed || !m_reconstruction) {
        return MockActionResult::failure(
            QStringLiteral("只有已重建的 Mock 图像可计算 QC"));
    }
    if (!qc.isValid()
        || qc.imageSha256.toUpper()
            != m_reconstruction->pngSha256.toUpper()) {
        const QString error =
            QStringLiteral("Mock QC 图像哈希与重建图像不一致");
        fail(error);
        return MockActionResult::failure(error);
    }
    m_qc = qc;
    m_state = MockWorkflowState::QcReady;
    m_lastError.clear();
    appendAudit(QStringLiteral("QC_COMPLETED"), m_state, qc.toJson());
    return MockActionResult::success();
}

MockActionResult MockWorkflow::recordQcFailure(const QString& error)
{
    if (m_state != MockWorkflowState::Reconstructed)
        return MockActionResult::failure(QStringLiteral("当前没有可失败的 QC 任务"));
    const QString failure =
        error.trimmed().isEmpty() ? QStringLiteral("Mock QC 失败") : error;
    const MockActionResult failed = fail(failure);
    if (failed.ok)
        appendAudit(QStringLiteral("QC_FAILED"), m_state,
                    {{QStringLiteral("error"), failure}});
    return failed.ok ? MockActionResult::failure(failure) : failed;
}

MockActionResult MockWorkflow::confirmResult()
{
    if (m_state != MockWorkflowState::QcReady || !m_reconstruction || !m_qc)
        return MockActionResult::failure(QStringLiteral("Mock QC 尚未完成"));
    m_resultConfirmed = true;
    appendAudit(QStringLiteral("RESULT_CONFIRMED"), m_state);
    return MockActionResult::success();
}

MockActionResult MockWorkflow::markPackaged(const QString& packagePath)
{
    if (m_state != MockWorkflowState::QcReady || !m_resultConfirmed)
        return MockActionResult::failure(QStringLiteral("Mock 结果尚未确认，不能封存"));
    if (packagePath.trimmed().isEmpty())
        return MockActionResult::failure(QStringLiteral("结果包路径为空"));
    m_packagePath = packagePath;
    m_state = MockWorkflowState::Packaged;
    m_lastError.clear();
    appendAudit(QStringLiteral("PACKAGE_SAVED"), m_state,
                {{QStringLiteral("packagePath"), packagePath}});
    return MockActionResult::success();
}

MockActionResult MockWorkflow::recordPackageFailure(const QString& error)
{
    if (m_state != MockWorkflowState::QcReady)
        return MockActionResult::failure(QStringLiteral("当前状态不能记录封存失败"));
    m_lastError =
        error.trimmed().isEmpty() ? QStringLiteral("结果包封存失败") : error;
    appendAudit(QStringLiteral("PACKAGE_SAVE_FAILED"), m_state,
                {{QStringLiteral("error"), m_lastError}});
    return MockActionResult::success();
}

DataSourceKind MockWorkflow::dataSource() const
{
    return m_dataSource;
}

MockWorkflowState MockWorkflow::state() const
{
    return m_state;
}

QStringList MockWorkflow::executionBlockReasons() const
{
    if (m_dataSource == DataSourceKind::Mock)
        return {};
    if (m_dataSource == DataSourceKind::HistoricalRaw) {
        return {
            QStringLiteral("历史 RAW 尚未建立受控解析与来源绑定")
        };
    }
    return {
        QStringLiteral("SDK 文件身份和版本未验证"),
        QStringLiteral("设备身份或 IDLE 状态未验证"),
        QStringLiteral("协议与横断位映射未验证"),
        QStringLiteral("真实参数快照未冻结"),
        QStringLiteral("隔离输出目录未验证"),
        QStringLiteral("唯一 Run owner 未验证"),
        QStringLiteral("本次单次 Run 人工授权未取得")
    };
}

QString MockWorkflow::runId() const
{
    return m_runId;
}

QString MockWorkflow::snapshotId() const
{
    return m_snapshot.snapshotId;
}

const MockParameterSnapshot& MockWorkflow::snapshot() const
{
    return m_snapshot;
}

int MockWorkflow::progressPercent() const
{
    return m_progressPercent;
}

const QVector<MockAuditEvent>& MockWorkflow::auditEvents() const
{
    return m_auditEvents;
}

bool MockWorkflow::hasReconstruction() const
{
    return m_reconstruction.has_value();
}

const std::optional<MockReconstructionArtifact>&
MockWorkflow::reconstruction() const
{
    return m_reconstruction;
}

bool MockWorkflow::hasQc() const
{
    return m_qc.has_value();
}

const std::optional<MockQcMetrics>& MockWorkflow::qc() const
{
    return m_qc;
}

bool MockWorkflow::resultConfirmed() const
{
    return m_resultConfirmed;
}

QString MockWorkflow::packagePath() const
{
    return m_packagePath;
}

QString MockWorkflow::lastError() const
{
    return m_lastError;
}

void MockWorkflow::appendAudit(
    const QString& name,
    MockWorkflowState state,
    const QJsonObject& details)
{
    MockAuditEvent event;
    event.name = name;
    event.occurredAtUtc = normalizedUtc(m_dependencies.nowUtc());
    event.runId = m_runId;
    event.state = state;
    event.dataSource = m_dataSource;
    event.details = details;
    m_auditEvents.append(event);
}

void MockWorkflow::clearResultData()
{
    m_reconstruction.reset();
    m_qc.reset();
    m_resultConfirmed = false;
    m_packagePath.clear();
}
