#include "ProtocolMapper.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

static ProtocolMapper::FieldMapping field(
    const QString& protocolId,
    const QString& presetName,
    const QString& sdkField,
    const QString& presetValue,
    const QString& status,
    const QString& note)
{
    return {protocolId, presetName, sdkField, presetValue, status, note};
}

QStringList ProtocolMapper::protocolIds(const SceneTemplate& scene)
{
    QStringList ids = scene.sequence.split(QStringLiteral("/"), Qt::SkipEmptyParts);
    for (auto& id : ids) {
        id = id.trimmed();
    }
    ids.removeDuplicates();
    return ids;
}

QVector<ProtocolMapper::FieldMapping> ProtocolMapper::whitelistForProtocol(const QString& protocolId)
{
    QVector<FieldMapping> fields;
    if (protocolId == QStringLiteral("LOC_017T")) {
        fields.append(field(protocolId, QStringLiteral("parameterFile"), QStringLiteral("SetParameterFile"), QStringLiteral("Iface/mriRely/par0423.par"), QStringLiteral("mapped"), QStringLiteral("沿用当前可跑通 par 文件；后续需拆分定位序列字段")));
        fields.append(field(protocolId, QStringLiteral("TR"), QStringLiteral("SetParameter(TR)"), QStringLiteral("800 ms"), QStringLiteral("pending"), QStringLiteral("字段存在但单位和序列作用域待核验")));
        fields.append(field(protocolId, QStringLiteral("TE"), QStringLiteral("displayOnly"), QStringLiteral("12.9 ms"), QStringLiteral("display-only"), QStringLiteral("序列标称 TE，不写入 SDK")));
        fields.append(field(protocolId, QStringLiteral("matrix"), QStringLiteral("pending:noSamples/noViews"), QStringLiteral("64x64"), QStringLiteral("pending"), QStringLiteral("底层采样字段待当前序列映射")));
        fields.append(field(protocolId, QStringLiteral("sliceThickness"), QStringLiteral("pending:sliceThickness"), QStringLiteral("5 mm"), QStringLiteral("pending"), QStringLiteral("SDK 字段名待确认")));
        return fields;
    }
    if (protocolId == QStringLiteral("FSE_A_017T")) {
        fields.append(field(protocolId, QStringLiteral("parameterFile"), QStringLiteral("SetParameterFile"), QStringLiteral("Iface/mriRely/par0423.par"), QStringLiteral("mapped"), QStringLiteral("先使用基线 par 文件生成 DRY_RUN")));
        fields.append(field(protocolId, QStringLiteral("TR"), QStringLiteral("SetParameter(TR)"), QStringLiteral("3000 ms"), QStringLiteral("pending"), QStringLiteral("需要确认 SDK 单位和 FSE 序列作用域")));
        fields.append(field(protocolId, QStringLiteral("TE"), QStringLiteral("displayOnly"), QStringLiteral("12.9 ms"), QStringLiteral("display-only"), QStringLiteral("历史序列标称 TE，不作为有效 TE 写入")));
        fields.append(field(protocolId, QStringLiteral("FOV"), QStringLiteral("pending:FOV"), QStringLiteral("50x50 mm"), QStringLiteral("pending"), QStringLiteral("FOV 到梯度/采样字段的映射待验收")));
        fields.append(field(protocolId, QStringLiteral("matrix"), QStringLiteral("pending:noSamples/noViews"), QStringLiteral("128x128 target"), QStringLiteral("pending"), QStringLiteral("目标重建矩阵，不能直接沿用历史高采样规模")));
        fields.append(field(protocolId, QStringLiteral("NEX"), QStringLiteral("pending:NEX"), QStringLiteral("1"), QStringLiteral("pending"), QStringLiteral("平均次数 SDK 字段待确认")));
        return fields;
    }
    if (protocolId == QStringLiteral("FSE_B_017T")) {
        fields = whitelistForProtocol(QStringLiteral("FSE_A_017T"));
        for (auto& item : fields) {
            item.protocolId = protocolId;
            if (item.presetName == QStringLiteral("TR")) {
                item.presetValue = QStringLiteral("4000 ms");
            } else if (item.presetName == QStringLiteral("NEX")) {
                item.presetValue = QStringLiteral("5");
            } else if (item.presetName == QStringLiteral("matrix")) {
                item.presetValue = QStringLiteral("128x128 target");
            }
        }
        return fields;
    }
    if (protocolId == QStringLiteral("FSE_ANAT_017T")) {
        fields.append(field(protocolId, QStringLiteral("TR"), QStringLiteral("SetParameter(TR)"), QStringLiteral("1500 ms"), QStringLiteral("pending"), QStringLiteral("需确认单位与序列字段")));
        fields.append(field(protocolId, QStringLiteral("effectiveTE"), QStringLiteral("pending:TE/echoTrain"), QStringLiteral("70 ms"), QStringLiteral("pending"), QStringLiteral("需核对 ETL、ESP、中心回波")));
        fields.append(field(protocolId, QStringLiteral("ETL"), QStringLiteral("pending:ETL"), QStringLiteral("8"), QStringLiteral("pending"), QStringLiteral("回波列字段待确认")));
        fields.append(field(protocolId, QStringLiteral("matrix"), QStringLiteral("pending:noSamples/noViews"), QStringLiteral("96x96"), QStringLiteral("pending"), QStringLiteral("底层字段待映射")));
        return fields;
    }
    if (protocolId == QStringLiteral("CPMG64_017T")) {
        fields.append(field(protocolId, QStringLiteral("centerFrequency"), QStringLiteral("SetTxCenterFre/SetRxCenterFre"), QStringLiteral("sweep determined"), QStringLiteral("pending"), QStringLiteral("实际中心频率必须由扫频确定")));
        fields.append(field(protocolId, QStringLiteral("TR"), QStringLiteral("SetParameter(TR)"), QStringLiteral("3000 ms"), QStringLiteral("pending"), QStringLiteral("SDK 字段待确认")));
        fields.append(field(protocolId, QStringLiteral("echoCount"), QStringLiteral("pending:echoCount"), QStringLiteral("64"), QStringLiteral("pending"), QStringLiteral("CPMG 序列字段待确认")));
        return fields;
    }

    fields.append(field(protocolId, QStringLiteral("protocol"), QStringLiteral("none"), QStringLiteral("开发预设"), QStringLiteral("pending"), QStringLiteral("该协议尚未进入 SDK 白名单")));
    return fields;
}

QVector<ProtocolMapper::FieldMapping> ProtocolMapper::mapScene(const SceneTemplate& scene)
{
    QVector<FieldMapping> fields;
    for (const auto& protocolId : protocolIds(scene)) {
        fields += whitelistForProtocol(protocolId);
    }
    return fields;
}

QString ProtocolMapper::safeFileStem(const QString& value)
{
    QString stem = value;
    stem.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_\\-]+")), QStringLiteral("_"));
    stem = stem.trimmed();
    return stem.isEmpty() ? QStringLiteral("scene") : stem.left(80);
}

ProtocolMapper::DryRunResult ProtocolMapper::generateDryRun(const SceneTemplate& scene, const QString& outputDir)
{
    DryRunResult result;
    result.fields = mapScene(scene);

    int mapped = 0;
    int pending = 0;
    int displayOnly = 0;
    for (const auto& item : result.fields) {
        if (item.status == QStringLiteral("mapped")) {
            ++mapped;
        } else if (item.status == QStringLiteral("display-only")) {
            ++displayOnly;
        } else {
            ++pending;
        }
    }

    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        result.status = QStringLiteral("DRY_RUN_FAILED");
        result.summary = QStringLiteral("无法创建 DRY_RUN 输出目录：%1").arg(outputDir);
        result.diagnostics.append(result.summary);
        return result;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
    const QString fileName = safeFileStem(scene.name) + QStringLiteral("_") + timestamp + QStringLiteral(".dryrun.par");
    result.filePath = dir.filePath(fileName);

    QFile file(result.filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result.status = QStringLiteral("DRY_RUN_FAILED");
        result.summary = QStringLiteral("无法写入 DRY_RUN 文件：%1").arg(result.filePath);
        result.diagnostics.append(result.summary);
        return result;
    }

    QTextStream out(&file);
    out << "# Scenario NMR SDK DRY_RUN\n";
    out << "# Template: " << scene.name << "\n";
    out << "# PrimaryScene: " << scene.primaryScene << "\n";
    out << "# Target: " << scene.target << "\n";
    out << "# RunGate: " << scene.runGate << "\n";
    out << "# GeneratedAt: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
    out << "[sdk_whitelist]\n";
    for (const auto& item : result.fields) {
        out << item.protocolId << "." << item.presetName
            << " sdkField=\"" << item.sdkField
            << "\" value=\"" << item.presetValue
            << "\" status=\"" << item.status
            << "\" note=\"" << item.note << "\"\n";
    }
    out << "\n[guard]\n";
    out << "realRun=HOLD\n";
    out << "writeToSdk=false\n";
    out << "requires=sequenceFieldMapping,deviceAdaptation,acquisitionAcceptance\n";
    file.close();

    result.ok = true;
    result.status = pending == 0 ? QStringLiteral("DRY_RUN_READY") : QStringLiteral("DRY_RUN_PENDING");
    result.summary = QStringLiteral("DRY_RUN 已生成：mapped=%1, pending=%2, displayOnly=%3").arg(mapped).arg(pending).arg(displayOnly);
    result.diagnostics.append(result.summary);
    result.diagnostics.append(QStringLiteral("输出文件：%1").arg(result.filePath));
    result.diagnostics.append(QStringLiteral("真实 Run 仍保持 HOLD；本文件不写入 SDK"));
    return result;
}

QString ProtocolMapper::diagnosticsText(const DryRunResult& result)
{
    QStringList lines;
    lines << QStringLiteral("状态：%1").arg(result.status);
    if (!result.filePath.isEmpty()) {
        lines << QStringLiteral("文件：%1").arg(result.filePath);
    }
    lines << result.summary;
    lines << QStringLiteral("");
    for (const auto& item : result.fields) {
        lines << QStringLiteral("[%1] %2 -> %3 = %4")
                     .arg(item.status, item.protocolId + QStringLiteral(".") + item.presetName, item.sdkField, item.presetValue);
        lines << QStringLiteral("  %1").arg(item.note);
    }
    return lines.join(QStringLiteral("\n"));
}
