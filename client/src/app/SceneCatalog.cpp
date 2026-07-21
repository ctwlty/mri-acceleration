#include "SceneCatalog.h"

static QString loc017tParams()
{
    return QStringLiteral("LOC_017T：TR 800 ms；序列标称 TE 12.9 ms；横断 FOV 50x50 mm；冠/矢 FOV 70x50 mm；矩阵 64x64；层厚 5 mm；每平面 3 层；NEX 1。");
}

static QString fseA017tParams()
{
    return QStringLiteral("FSE_A_017T：TR 3000 ms；序列标称 TE 12.9 ms；采样周期 20 us；过采样 2；FOV 50x50 mm；目标矩阵 128x128；层厚 3.5 mm；间隔 2.0 mm；13 层；NEX 1；读出梯度候选约 11.74 mT/m / 19.6 A。");
}

static QString fseB017tParams()
{
    return QStringLiteral("FSE_B_017T：TR 4000 ms；序列标称 TE 12.9 ms；采样周期 20 us；过采样 2；FOV 50x50 mm；目标矩阵 128x128；层厚 3.5 mm；间隔 1.25 mm；15 层；NEX 5；读出梯度候选约 11.74 mT/m / 19.6 A。");
}

static QString fseAnat017tParams()
{
    return QStringLiteral("FSE_ANAT_017T：TR 1500 ms；有效 TE 70 ms；ETL 8；ESP 10 ms；中心回波 7；FOV 35x35 mm；矩阵 96x96；层厚 1.2 mm；间隔 0.2 mm；20 层；NEX 4。");
}

static SceneTemplate makeTemplate(
    const QString& primaryScene,
    const QString& target,
    const QString& name,
    const QString& outputType,
    const QString& protocols,
    const QString& acquisition,
    const QString& preparation,
    const QString& positioning,
    const QString& reconstruction,
    const QString& qcOutput,
    const QString& parameterDetails,
    const QString& risks,
    const QString& handoff,
    const QString& snr,
    const QString& uniformity,
    const QString& distortion,
    const QString& stability,
    const QString& note)
{
    SceneTemplate scene;
    scene.name = name;
    scene.target = target;
    scene.sequence = protocols;
    scene.stepA = preparation;
    scene.stepB = positioning;
    scene.processing = reconstruction;
    scene.analysis = qcOutput;
    scene.snr = snr;
    scene.uniformity = uniformity;
    scene.peak = distortion;
    scene.area = stability;
    scene.note = note;
    scene.primaryScene = primaryScene;
    scene.outputType = outputType;
    scene.acquisitionProtocol = acquisition;
    scene.preparation = preparation;
    scene.positioning = positioning;
    scene.reconstruction = reconstruction;
    scene.qcOutput = qcOutput;
    scene.presetVersion = QStringLiteral("presetVersion=2");
    scene.adaptationStatus = QStringLiteral("待设备适配");
    scene.runGate = QStringLiteral("HOLD");
    scene.parameterStatus = QStringLiteral("开发预设");
    scene.parameterDetails = parameterDetails;
    scene.sdkMappingStatus = QStringLiteral("待序列字段映射");
    scene.physicsCheckStatus = QStringLiteral("开发边界检查通过");
    scene.riskTags = risks;
    scene.handoffTarget = handoff;
    return scene;
}

QList<SceneTemplate> SceneCatalog::defaults()
{
    return {
        makeTemplate(
            QStringLiteral("结构与形态成像"),
            QStringLiteral("标准模体、组织样品、根茎样品"),
            QStringLiteral("内部结构成像模板"),
            QStringLiteral("2D/3D 结构图像、协议和 QC 记录"),
            QStringLiteral("LOC_017T / FSE_A_017T / FSE_B_017T"),
            QStringLiteral("定位序列 + FSE A/B 候选协议"),
            QStringLiteral("记录样品信息，检查线圈、资源和存储"),
            QStringLiteral("确认方向、中心和完整覆盖后采集"),
            QStringLiteral("RAW 解析、标准重建和基础处理"),
            QStringLiteral("通用图像质控；输出图像、协议和 QC 记录"),
            loc017tParams() + QStringLiteral("\n") + fseA017tParams() + QStringLiteral("\n") + fseB017tParams(),
            QStringLiteral("SDK 字段待回填；12.9 ms 仅为序列标称 TE"),
            QStringLiteral("图像、协议、QC 记录"),
            QStringLiteral("42.8"),
            QStringLiteral("96.1%"),
            QStringLiteral("待复核"),
            QStringLiteral("重复扫描待验证"),
            QStringLiteral("首版用于标准模体和内部结构演示，真实 Run 保持 HOLD。")),
        makeTemplate(
            QStringLiteral("结构与形态成像"),
            QStringLiteral("小鼠及目标血管区域"),
            QStringLiteral("小鼠血管结构成像模板"),
            QStringLiteral("解剖参考、血管结构图像、QC 记录"),
            QStringLiteral("LOC_017T / FSE_ANAT_017T / TOF_017T"),
            QStringLiteral("定位 + 解剖参考 + 血管专用候选协议"),
            QStringLiteral("记录活体/离体、固定、运动及必要生理条件"),
            QStringLiteral("定位目标区域并确认覆盖"),
            QStringLiteral("标准重建和空间方向整理"),
            QStringLiteral("通用 QC + 运动与血管可辨识度；正式分割交外部软件"),
            loc017tParams() + QStringLiteral("\n") + fseAnat017tParams() + QStringLiteral("\nTOF_017T：TR 30 ms；TE 初值 5 ms；翻转角 60 deg；FOV 35x35 mm；矩阵 96x96；层厚 1.5 mm；24 层；NEX 4；流动补偿候选开启，启用后 TE 自动采用最短可实现值。"),
            QStringLiteral("运动、血管可辨识度、TOF 最短 TE 待实测"),
            QStringLiteral("图像、解剖参考、QC 记录；分割交外部软件"),
            QStringLiteral("38.6"),
            QStringLiteral("94.0%"),
            QStringLiteral("运动需确认"),
            QStringLiteral("生理同步待接入"),
            QStringLiteral("用于低场小鼠血管结构研究，暂不输出自动血管分割结论。")),
        makeTemplate(
            QStringLiteral("参数与成分定量"),
            QStringLiteral("小鼠肝脏、脾脏等内部脏器"),
            QStringLiteral("铁沉积敏感 / QSM 研究模板"),
            QStringLiteral("磁化率参数图、相位数据、QC 记录"),
            QStringLiteral("LOC_017T / FSE_ANAT_017T / MEGRE_PHASE_017T"),
            QStringLiteral("定位 + 解剖参考 + 磁化率敏感候选采集"),
            QStringLiteral("确认目标器官、活体/离体状态和必要元数据"),
            QStringLiteral("定位器官并确认 FOV、方向和覆盖"),
            QStringLiteral("复数重建及 QSM 候选处理"),
            QStringLiteral("输出磁化率参数图和 QC；铁含量需另行标定"),
            loc017tParams() + QStringLiteral("\n") + fseAnat017tParams() + QStringLiteral("\nMEGRE_PHASE_017T：TR 100 ms；TE 初值 [4,10,16,22,28,34,40] ms；翻转角 20 deg；FOV 45x45x56 mm；矩阵 64x64x48；NEX 4；3D 多回波单极性读出，保存复数/相位数据；最长 TE 依据实测 T2* 调整。"),
            QStringLiteral("相位稳定性、最长 TE、R2* 与铁含量标定"),
            QStringLiteral("复数/相位数据、参数图、QC；铁含量交外部标定"),
            QStringLiteral("36.9"),
            QStringLiteral("92.7%"),
            QStringLiteral("相位稳定性待评估"),
            QStringLiteral("多回波一致性待验证"),
            QStringLiteral("当前只作为相位稳定性和 R2* 研究模板，不宣称铁含量定量完成。")),
        makeTemplate(
            QStringLiteral("参数与成分定量"),
            QStringLiteral("生蚝整体或指定组织"),
            QStringLiteral("生蚝脂肪 / 组成定量数据采集模板"),
            QStringLiteral("组成敏感数据、ROI、真值编号、QC 记录"),
            QStringLiteral("LOC_017T / FSE_A_017T / MEGRE_WF_017T"),
            QStringLiteral("定位 + 组成敏感候选协议 + 参考样"),
            QStringLiteral("记录批次、重量、温度、ROI 和参考真值"),
            QStringLiteral("确认 ROI 覆盖，采集样品和参考样"),
            QStringLiteral("重建或信号解析，生成候选参数或特征"),
            QStringLiteral("输出数据、ROI、真值编号和 QC；最终定量交外部软件"),
            loc017tParams() + QStringLiteral("\n") + fseA017tParams() + QStringLiteral("\nMEGRE_WF_017T：TR 80 ms；TE [4.0,10.8,17.5,24.3,31.1,37.9] ms；回波间隔 6.77 ms；相邻水脂相位步进约 60 deg；翻转角 20 deg；FOV 48x48x64 mm；矩阵 64x64x64；NEX 4；保存复数数据和中心频率/B0 信息。"),
            QStringLiteral("水油模体、化学真值标定、中心频率/B0 信息"),
            QStringLiteral("图像/特征、ROI、真值编号、QC"),
            QStringLiteral("35.4"),
            QStringLiteral("91.8%"),
            QStringLiteral("ROI 覆盖需确认"),
            QStringLiteral("参考样重复待验证"),
            QStringLiteral("用于组成特征采集，脂肪分数需外部标定后确认。")),
        makeTemplate(
            QStringLiteral("参数与成分定量"),
            QStringLiteral("单粒或批量种子"),
            QStringLiteral("种子水分 / 油分定量数据采集模板"),
            QStringLiteral("弛豫/频谱特征、样品信息、QC 记录"),
            QStringLiteral("LOC_017T / FSE_A_017T / CPMG64_017T"),
            QStringLiteral("组成敏感图像、弛豫或频谱候选协议"),
            QStringLiteral("记录品种、批次、质量、温度和装样方式"),
            QStringLiteral("确认样品区域并按模板重复采集"),
            QStringLiteral("图像/信号解析和特征提取"),
            QStringLiteral("输出特征、样品信息、真值编号和 QC；最终定量需标定验证"),
            loc017tParams() + QStringLiteral("\n") + fseA017tParams() + QStringLiteral("\nCPMG64_017T：理论搜频初值 7.23817 MHz；实际中心频率由扫频确定；TR 3000 ms；回波间隔 6 ms；64 回波；末回波时间 384 ms；NEX 8；重复 3 次；温度 25±1°C。"),
            QStringLiteral("实际中心频率由扫频确定；温度需稳定"),
            QStringLiteral("特征、样品信息、真值编号、QC"),
            QStringLiteral("41.1"),
            QStringLiteral("95.2%"),
            QStringLiteral("装样一致性需确认"),
            QStringLiteral("3 次重复"),
            QStringLiteral("CPMG64_017T 为开发预设，中心频率和温控待实机回填。")),
        makeTemplate(
            QStringLiteral("品质与性状预测"),
            QStringLiteral("水果整体或指定果肉区域"),
            QStringLiteral("水果糖度预测数据采集模板"),
            QStringLiteral("标准化图像特征、模型输入、QC 记录"),
            QStringLiteral("LOC_017T / FSE_A_017T / DWI_017T"),
            QStringLiteral("定位 + 标准化多对比候选采集"),
            QStringLiteral("记录品种、成熟度、温度并绑定糖度真值"),
            QStringLiteral("固定摆位，确认覆盖并标准化采集"),
            QStringLiteral("重建、ROI/特征提取和数据标准化"),
            QStringLiteral("输出模型输入和 QC；糖度由外部模型预测"),
            loc017tParams() + QStringLiteral("\n") + fseA017tParams() + QStringLiteral("\nDWI_017T：TR 2000 ms；TE 采用最短可实现值，工程初值 100 ms；b 值 [0,200,500] s/mm2；delta 20 ms；Delta 50 ms；理想矩形扩散梯度约 [0,12.70,20.08] mT/m；3 个正交方向；FOV 48x48 mm；矩阵 64x64；层厚 4 mm；间隔 0.25 mm；15 层；NEX 6。"),
            QStringLiteral("最终 b 值按完整实测梯度波形计算"),
            QStringLiteral("模型输入、ROI、真值编号、QC"),
            QStringLiteral("33.8"),
            QStringLiteral("90.6%"),
            QStringLiteral("DWI 畸变待复核"),
            QStringLiteral("跨批次标准化待验证"),
            QStringLiteral("控制台只生成模型输入，不直接给出糖度预测结果。")),
        makeTemplate(
            QStringLiteral("动态与功能成像"),
            QStringLiteral("小鼠脑"),
            QStringLiteral("小鼠脑低场动态功能研究模板（探索性）"),
            QStringLiteral("4D 时序、解剖参考、事件日志、QC 记录"),
            QStringLiteral("LOC_017T / FSE_ANAT_017T / PDFSE_DYN_017T"),
            QStringLiteral("定位 + 解剖参考 + 快速功能时序"),
            QStringLiteral("检查固定、生理状态、刺激范式和同步条件"),
            QStringLiteral("确认脑区覆盖并记录刺激和时间戳"),
            QStringLiteral("4D 重建及基础运动、漂移检查"),
            QStringLiteral("输出 4D 时序、解剖参考、事件日志和 QC；功能分区交外部软件"),
            loc017tParams() + QStringLiteral("\n") + fseAnat017tParams() + QStringLiteral("\nPDFSE_DYN_017T：shot TR 1000 ms；TE 24 ms；ETL 8；相位编码 shots 8；预计 volume time 8 s；FOV 35x35 mm；矩阵 64x64；层厚 1.2 mm；目标脑区 8 层；30 个有效时间点；预扫描 2 个时间点；NEX 1；外部刺激触发。"),
            QStringLiteral("非 BOLD、SEEP/时序稳定性探索；同步触发待接入"),
            QStringLiteral("4D 时序、事件日志、QC；功能统计交外部软件"),
            QStringLiteral("31.5"),
            QStringLiteral("89.9%"),
            QStringLiteral("运动/漂移需确认"),
            QStringLiteral("30 个时间点"),
            QStringLiteral("探索性低场动态模板，不直接输出功能分区结论。"))
    };
}
