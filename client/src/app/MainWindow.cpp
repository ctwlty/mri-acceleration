#include "MainWindow.h"

#include "DeviceActionAvailability.h"
#include "ImageQualityEvaluator.h"

#include <QCryptographicHash>
#include <QCloseEvent>
#include <QFrame>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSize>
#include <QSplitter>
#include <QStyle>
#include <QStringList>
#include <QTabWidget>
#include <QTime>
#include <QVBoxLayout>

static QString badgeClassForState(const QString& state)
{
    if (state == QStringLiteral("已连接") || state == QStringLiteral("扫描中") || state == QStringLiteral("Demo执行") || state == QStringLiteral("正常")) {
        return QStringLiteral("primary");
    }
    if (state == QStringLiteral("已暂停") || state == QStringLiteral("HOLD")) {
        return QStringLiteral("warning");
    }
    if (state == QStringLiteral("已终止")) {
        return QStringLiteral("danger");
    }
    return QStringLiteral("secondary");
}

static QFrame* makePanel(const QString& objectName)
{
    auto* frame = new QFrame;
    frame->setObjectName(objectName);
    frame->setProperty("class", "panel");
    return frame;
}

void MainWindow::addInfoRow(QGridLayout* form, QWidget* parent, int row, const QString& labelText, QLabel*& valueLabel)
{
    auto* label = new QLabel(labelText, parent);
    label->setObjectName("MutedLabel");
    valueLabel = new QLabel(parent);
    valueLabel->setObjectName("AppSubtitle");
    valueLabel->setWordWrap(true);
    valueLabel->setMinimumHeight(24);
    form->addWidget(label, row, 0);
    form->addWidget(valueLabel, row, 1);
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_eggController(new EggControllerProcess(this))
    , m_bridge(new DeviceBridge(this))
    , m_catalog(SceneCatalog::defaults())
{
    setObjectName("MainWindow");
    setWindowTitle(QStringLiteral("场景化核磁共振控制台"));
    resize(1560, 960);
    setMinimumSize(1380, 860);

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    rootLayout->addWidget(buildHeader());

    auto* splitter = new QSplitter(Qt::Horizontal, root);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(buildLeftPane());
    splitter->addWidget(buildCenterPane());
    splitter->addWidget(buildRightPane());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({360, 760, 440});
    rootLayout->addWidget(splitter, 1);

    rootLayout->addWidget(buildFooter());
    setCentralWidget(root);

    connect(m_bridge, &DeviceBridge::logAppended, this, &MainWindow::appendLog);
    connect(m_bridge, &DeviceBridge::badgesChanged, this, &MainWindow::updateBadges);
    connect(m_bridge, &DeviceBridge::scanStatusChanged, this, &MainWindow::updateScan);
    connect(m_bridge, &DeviceBridge::metricsChanged, this, &MainWindow::updateMetrics);
    connect(m_bridge, &DeviceBridge::temperatureChanged, this, &MainWindow::updateTemperature);
    connect(m_bridge, &DeviceBridge::sdkStatusChanged, this, &MainWindow::updateSdkStatus);
    connect(m_bridge, &DeviceBridge::sdkDiagnosticChanged, this, &MainWindow::updateSdkDiagnostic);
    connect(m_bridge, &DeviceBridge::sessionStateChanged, this, &MainWindow::updateSessionState);
    connect(m_bridge, &DeviceBridge::rawFileReady, this, [this](const QString& filePath) {
        appendLog(QStringLiteral("RAW 验收通过：%1").arg(filePath));
    });
    connect(m_bridge, &DeviceBridge::operationFailed, this, [this](const MriSdkResult& result) {
        appendLog(QStringLiteral("操作失败 [%1/%2] code=%3：%4")
                      .arg(result.stage, result.function)
                      .arg(result.code)
                      .arg(result.message));
    });
    connect(m_eggController, &EggControllerProcess::stageChanged, this, [this](const QString& stage) {
        if (m_automationStatusLabel) {
            m_automationStatusLabel->setText(stage);
        }
        appendLog(QStringLiteral("自动化基线：%1").arg(stage));
    });
    connect(m_eggController, &EggControllerProcess::logAppended,
            this, &MainWindow::appendLog);
    connect(m_eggController, &EggControllerProcess::completed,
            this, &MainWindow::showEggControllerArtifacts);
    connect(m_eggController, &EggControllerProcess::failed, this, [this](const QString& message) {
        if (m_automationStatusLabel) {
            m_automationStatusLabel->setText(QStringLiteral("Failed"));
        }
        appendLog(QStringLiteral("自动化基线失败：%1").arg(message));
        updateControlMode();
    });

    updateSessionState(MriSdkSessionState::Unloaded);

    populatePrimaryScenes();
    handleSceneChanged();
}

void MainWindow::configureEggController(const EggControllerLaunchConfig& config)
{
    m_eggControllerConfig = config;
    if (m_controlModeCombo) {
        const int index = m_controlModeCombo->findData(QStringLiteral("eggcontroller"));
        m_controlModeCombo->setCurrentIndex(index);
    }
    if (m_automationStatusLabel) {
        m_automationStatusLabel->setText(QStringLiteral("Ready"));
    }
    updateControlMode();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_eggController && m_eggController->isRunning()) {
        appendLog(QStringLiteral("自动化基线仍在运行，窗口将在入口自然返回后允许关闭"));
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

QWidget* MainWindow::buildHeader()
{
    auto* frame = makePanel("HeaderBar");
    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(12);

    auto* titleBlock = new QVBoxLayout;
    auto* title = new QLabel(QStringLiteral("场景化核磁共振控制台"), frame);
    title->setObjectName("AppTitle");
    auto* subtitle = new QLabel(QStringLiteral("科研 / 教学版"), frame);
    subtitle->setObjectName("AppSubtitle");
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);
    layout->addLayout(titleBlock);
    layout->addStretch();

    m_headerSceneValue = new QLabel(QStringLiteral("当前任务：内部结构成像模板"), frame);
    m_headerSceneValue->setObjectName("AppSubtitle");
    layout->addWidget(m_headerSceneValue);

    m_headerSdkValue = new QLabel(QStringLiteral("SDK：未加载"), frame);
    m_headerSdkValue->setObjectName("AppSubtitle");
    layout->addWidget(m_headerSdkValue);

    auto* connectBadge = new QLabel(QStringLiteral("设备：待建链"), frame);
    connectBadge->setObjectName("AppSubtitle");
    layout->addWidget(connectBadge);

    auto* transferBadge = new QLabel(QStringLiteral("参数：PTScan 基线"), frame);
    transferBadge->setObjectName("AppSubtitle");
    layout->addWidget(transferBadge);

    return frame;
}

QWidget* MainWindow::buildLeftPane()
{
    auto* frame = makePanel("Panel");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_sceneTitle = new QLabel(QStringLiteral("任务选择"), frame);
    m_sceneTitle->setObjectName("SectionTitle");
    layout->addWidget(m_sceneTitle);

    auto* filterPanel = makePanel("SelectorFilterPanel");
    auto* filterLayout = new QGridLayout(filterPanel);
    filterLayout->setContentsMargins(12, 12, 12, 12);
    filterLayout->setHorizontalSpacing(10);
    filterLayout->setVerticalSpacing(8);

    auto* primaryLabel = new QLabel(QStringLiteral("一级场景"), filterPanel);
    primaryLabel->setObjectName("MutedLabel");
    m_primarySceneCombo = new QComboBox(filterPanel);
    m_primarySceneCombo->setObjectName("PrimarySceneCombo");
    m_primarySceneCombo->setMaxVisibleItems(12);

    auto* targetLabel = new QLabel(QStringLiteral("检测对象"), filterPanel);
    targetLabel->setObjectName("MutedLabel");
    m_targetCombo = new QComboBox(filterPanel);
    m_targetCombo->setObjectName("TargetCombo");
    m_targetCombo->setMaxVisibleItems(12);

    auto* searchLabel = new QLabel(QStringLiteral("模板搜索"), filterPanel);
    searchLabel->setObjectName("MutedLabel");
    m_templateSearchEdit = new QLineEdit(filterPanel);
    m_templateSearchEdit->setObjectName("TemplateSearchEdit");
    m_templateSearchEdit->setClearButtonEnabled(true);
    m_templateSearchEdit->setPlaceholderText(QStringLiteral("输入模板、对象或协议关键词"));

    filterLayout->addWidget(primaryLabel, 0, 0);
    filterLayout->addWidget(m_primarySceneCombo, 0, 1);
    filterLayout->addWidget(targetLabel, 1, 0);
    filterLayout->addWidget(m_targetCombo, 1, 1);
    filterLayout->addWidget(searchLabel, 2, 0);
    filterLayout->addWidget(m_templateSearchEdit, 2, 1);
    filterLayout->setColumnStretch(1, 1);
    layout->addWidget(filterPanel);

    auto* templateLabel = new QLabel(QStringLiteral("推荐模板"), frame);
    templateLabel->setObjectName("MutedLabel");
    layout->addWidget(templateLabel);

    m_sceneList = new QListWidget(frame);
    m_sceneList->setObjectName("TemplateList");
    m_sceneList->setMinimumHeight(170);
    m_sceneList->setMaximumHeight(260);
    m_sceneList->setSpacing(8);
    m_sceneList->setWordWrap(true);
    m_sceneList->setUniformItemSizes(false);
    layout->addWidget(m_sceneList);

    auto* modePanel = makePanel("SelectorFilterPanel");
    auto* modeLayout = new QGridLayout(modePanel);
    modeLayout->setContentsMargins(12, 10, 12, 10);
    auto* modeLabel = new QLabel(QStringLiteral("控制方式"), modePanel);
    modeLabel->setObjectName("MutedLabel");
    m_controlModeCombo = new QComboBox(modePanel);
    m_controlModeCombo->setObjectName("ControlModeCombo");
    m_controlModeCombo->addItem(QStringLiteral("SDK 直连"), QStringLiteral("sdk"));
    m_controlModeCombo->addItem(QStringLiteral("自动化基线"), QStringLiteral("eggcontroller"));
    m_automationStatusLabel = new QLabel(QStringLiteral("未配置"), modePanel);
    m_automationStatusLabel->setObjectName("AutomationStatusLabel");
    m_automationStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    modeLayout->addWidget(modeLabel, 0, 0);
    modeLayout->addWidget(m_controlModeCombo, 0, 1);
    modeLayout->addWidget(m_automationStatusLabel, 1, 0, 1, 2);
    layout->addWidget(modePanel);

    auto* buttons = new QGridLayout;
    buttons->setHorizontalSpacing(10);
    buttons->setVerticalSpacing(10);

    m_loadSdkButton = new QPushButton(QStringLiteral("加载 SDK"), frame);
    m_loadSdkButton->setProperty("class", "secondary");
    m_loadSdkButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    m_connectButton = new QPushButton(QStringLiteral("一键建链"), frame);
    m_connectButton->setProperty("class", "primary");
    m_connectButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    auto* precheckBtn = new QPushButton(QStringLiteral("校准向导"), frame);
    precheckBtn->setProperty("class", "secondary");
    auto* dryRunBtn = new QPushButton(QStringLiteral("DRY_RUN"), frame);
    dryRunBtn->setProperty("class", "secondary");
    m_startButton = new QPushButton(QStringLiteral("开始采集"), frame);
    m_startButton->setObjectName("StartButton");
    m_startButton->setProperty("class", "success");
    m_startButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_pauseButton = new QPushButton(QStringLiteral("暂停（不支持）"), frame);
    m_pauseButton->setProperty("class", "warning");
    m_pauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    m_pauseButton->setToolTip(QStringLiteral("当前 SDK 未提供暂停/继续接口"));
    m_pauseButton->setEnabled(false);
    m_abortButton = new QPushButton(QStringLiteral("急停"), frame);
    m_abortButton->setProperty("class", "danger");
    m_abortButton->setIcon(style()->standardIcon(QStyle::SP_BrowserStop));

    buttons->addWidget(m_loadSdkButton, 0, 0);
    buttons->addWidget(m_connectButton, 0, 1);
    buttons->addWidget(precheckBtn, 1, 0);
    buttons->addWidget(dryRunBtn, 1, 1);
    buttons->addWidget(m_startButton, 2, 0);
    buttons->addWidget(m_pauseButton, 2, 1);
    buttons->addWidget(m_abortButton, 3, 0, 1, 2);
    layout->addLayout(buttons);

    layout->addStretch();

    connect(m_primarySceneCombo, &QComboBox::currentIndexChanged, this, &MainWindow::handlePrimarySceneChanged);
    connect(m_targetCombo, &QComboBox::currentIndexChanged, this, &MainWindow::handleTargetChanged);
    connect(m_templateSearchEdit, &QLineEdit::textChanged, this, &MainWindow::handleTemplateSearchChanged);
    connect(m_sceneList, &QListWidget::currentRowChanged, this, &MainWindow::handleSceneChanged);
    connect(m_controlModeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::updateControlMode);
    connect(m_loadSdkButton, &QPushButton::clicked, this, &MainWindow::handleLoadSdk);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::handleConnect);
    connect(precheckBtn, &QPushButton::clicked, this, &MainWindow::handlePrecheck);
    connect(dryRunBtn, &QPushButton::clicked, this, &MainWindow::handleDryRun);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::handleStart);
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::handlePause);
    connect(m_abortButton, &QPushButton::clicked, this, &MainWindow::handleAbort);

    return frame;
}

QWidget* MainWindow::buildCenterPane()
{
    auto* frame = makePanel("Panel");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("操作链"), frame);
    title->setObjectName("SectionTitle");
    layout->addWidget(title);

    auto* chainHost = new QWidget(frame);
    auto* chainRow = new QHBoxLayout(chainHost);
    chainRow->setContentsMargins(0, 0, 0, 0);
    chainRow->setSpacing(8);
    const QString nodeTitles[] = {
        QStringLiteral("推荐模板"),
        QStringLiteral("准备与预检"),
        QStringLiteral("定位与采集"),
        QStringLiteral("获取图像"),
        QStringLiteral("处理与重建"),
        QStringLiteral("质控与交接")
    };
    for (int i = 0; i < 6; ++i) {
        chainRow->addWidget(makeOperationNode(QStringLiteral("%1").arg(i + 1, 2, 10, QLatin1Char('0')), nodeTitles[i], m_operationDetails[i]));
        if (i < 5) {
            auto* arrow = new QLabel(QStringLiteral(">"), chainHost);
            arrow->setProperty("class", "operationArrow");
            arrow->setAlignment(Qt::AlignCenter);
            arrow->setFixedWidth(16);
            chainRow->addWidget(arrow);
        }
    }
    chainRow->addStretch();

    auto* chainScroll = new QScrollArea(frame);
    chainScroll->setObjectName("OperationChainScroll");
    chainScroll->setWidgetResizable(true);
    chainScroll->setFrameShape(QFrame::NoFrame);
    chainScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    chainScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chainScroll->setFixedHeight(112);
    chainScroll->setWidget(chainHost);
    layout->addWidget(chainScroll);

    auto* viewportGrid = new QGridLayout;
    viewportGrid->setSpacing(0);
    viewportGrid->addWidget(makeDarkViewport(QStringLiteral("获取图像 / 协议"), QStringLiteral("LOC + 候选协议")), 0, 0);
    viewportGrid->addWidget(makeDarkViewport(QStringLiteral("准备与预检"), QStringLiteral("样品 / 线圈 / 存储")), 0, 1);
    viewportGrid->addWidget(makeImageViewport(QStringLiteral("K-space 中间过程"), QStringLiteral("eggcontrollerV2 原始输出"), m_kspaceImageView), 1, 0);
    viewportGrid->addWidget(makeImageViewport(QStringLiteral("最终重建图"), QStringLiteral("同次任务可见结果"), m_finalImageView), 1, 1);
    m_kspaceImageView->setObjectName("KspaceImageView");
    m_finalImageView->setObjectName("FinalImageView");
    viewportGrid->setRowStretch(0, 1);
    viewportGrid->setRowStretch(1, 1);
    viewportGrid->setColumnStretch(0, 1);
    viewportGrid->setColumnStretch(1, 1);
    layout->addLayout(viewportGrid, 1);

    m_chainSummary = new QLabel(QStringLiteral("一级场景 + 检测对象 -> 推荐任务模板 -> 展开实验链 -> 图像质控 -> 结果交接"), frame);
    m_chainSummary->setWordWrap(true);
    m_chainSummary->setObjectName("AppSubtitle");
    layout->addWidget(m_chainSummary);

    return frame;
}

QWidget* MainWindow::buildRightPane()
{
    auto* frame = makePanel("Panel");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("质控与诊断"), frame);
    title->setObjectName("SectionTitle");
    layout->addWidget(title);

    auto* metricGrid = new QGridLayout;
    metricGrid->setSpacing(10);
    metricGrid->addWidget(makeMetricCard(QStringLiteral("SNR"), m_snrValue), 0, 0);
    metricGrid->addWidget(makeMetricCard(QStringLiteral("均匀性"), m_uniformityValue), 0, 1);
    metricGrid->addWidget(makeMetricCard(QStringLiteral("畸变/尺寸"), m_peakValue), 1, 0);
    metricGrid->addWidget(makeMetricCard(QStringLiteral("重复稳定"), m_areaValue), 1, 1);
    m_snrValue->setObjectName(QStringLiteral("QualitySnrValue"));
    m_uniformityValue->setObjectName(QStringLiteral("QualityUniformityValue"));
    m_peakValue->setObjectName(QStringLiteral("QualitySizeValue"));
    m_areaValue->setObjectName(QStringLiteral("QualityStabilityValue"));
    layout->addLayout(metricGrid);

    auto* tabs = new QTabWidget(frame);
    tabs->setObjectName("RightPaneTabs");

    auto* presetTab = new QWidget(tabs);
    auto* presetLayout = new QVBoxLayout(presetTab);
    presetLayout->setContentsMargins(12, 12, 12, 12);
    presetLayout->setSpacing(10);
    auto* presetForm = new QGridLayout;
    presetForm->setHorizontalSpacing(10);
    presetForm->setVerticalSpacing(8);
    addInfoRow(presetForm, presetTab, 0, QStringLiteral("版本"), m_presetVersionValue);
    addInfoRow(presetForm, presetTab, 1, QStringLiteral("参数状态"), m_parameterStatusValue);
    addInfoRow(presetForm, presetTab, 2, QStringLiteral("Run"), m_runGateValue);
    addInfoRow(presetForm, presetTab, 3, QStringLiteral("SDK 映射"), m_sdkMappingValue);
    addInfoRow(presetForm, presetTab, 4, QStringLiteral("物理检查"), m_physicsCheckValue);
    addInfoRow(presetForm, presetTab, 5, QStringLiteral("交接包"), m_handoffValue);
    presetForm->setColumnStretch(1, 1);
    presetLayout->addLayout(presetForm);

    auto* parameterTitle = new QLabel(QStringLiteral("协议参数明细（显示，不直接写入 SDK）"), presetTab);
    parameterTitle->setObjectName("MutedLabel");
    presetLayout->addWidget(parameterTitle);

    m_parameterDetailsView = new QPlainTextEdit(presetTab);
    m_parameterDetailsView->setReadOnly(true);
    m_parameterDetailsView->setMinimumHeight(220);
    m_parameterDetailsView->setPlainText(QStringLiteral("选择任务模板后显示 TR、TE、FOV、矩阵、层厚、NEX 等参数。当前仅作开发预设展示，待序列字段映射后才能写入 SDK。"));
    presetLayout->addWidget(m_parameterDetailsView, 1);
    tabs->addTab(presetTab, QStringLiteral("参数预设"));

    auto* diagnosticTab = new QWidget(tabs);
    auto* diagnosticLayout = new QVBoxLayout(diagnosticTab);
    diagnosticLayout->setContentsMargins(12, 12, 12, 12);
    diagnosticLayout->setSpacing(10);
    auto* diagnosticTitle = new QLabel(QStringLiteral("SDK 诊断 / 字段白名单"), diagnosticTab);
    diagnosticTitle->setObjectName("MutedLabel");
    diagnosticLayout->addWidget(diagnosticTitle);

    m_sdkDiagnosticView = new QPlainTextEdit(diagnosticTab);
    m_sdkDiagnosticView->setReadOnly(true);
    m_sdkDiagnosticView->setPlainText(QStringLiteral("尚未生成 DRY_RUN。\n点击左侧 DRY_RUN 可生成 SDK 字段白名单和参数文件预览。\n真实 Run 继续保持 HOLD。"));
    diagnosticLayout->addWidget(m_sdkDiagnosticView, 1);
    tabs->addTab(diagnosticTab, QStringLiteral("SDK 诊断"));

    auto* logTab = new QWidget(tabs);
    auto* logLayout = new QVBoxLayout(logTab);
    logLayout->setContentsMargins(12, 12, 12, 12);
    logLayout->setSpacing(10);
    auto* logTitle = new QLabel(QStringLiteral("日志"), logTab);
    logTitle->setObjectName("MutedLabel");
    logLayout->addWidget(logTitle);

    m_logView = new QPlainTextEdit(logTab);
    m_logView->setReadOnly(true);
    m_logView->setPlainText(
        QStringLiteral("任务模板已加载\n设备入口：手动加载 mridll.dll\n实机基线：PTScan.par\n场景参数暂不写入 SDK\n扫描结果以新增 RAW 文件验收"));
    logLayout->addWidget(m_logView, 1);
    tabs->addTab(logTab, QStringLiteral("日志"));

    layout->addWidget(tabs, 1);

    return frame;
}

QWidget* MainWindow::buildFooter()
{
    auto* frame = makePanel("FooterBar");
    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(18, 10, 18, 10);
    layout->setSpacing(18);

    auto addFooter = [&](const QString& labelText, QLabel*& valueLabel) {
        auto* box = new QVBoxLayout;
        auto* label = new QLabel(labelText, frame);
        label->setObjectName("MutedLabel");
        valueLabel = new QLabel(frame);
        valueLabel->setObjectName("AppSubtitle");
        box->addWidget(label);
        box->addWidget(valueLabel);
        layout->addLayout(box);
    };
    addFooter(QStringLiteral("连接"), m_footerConnectionValue);
    addFooter(QStringLiteral("扫描"), m_footerScanValue);
    addFooter(QStringLiteral("温度"), m_footerTemperatureValue);
    addFooter(QStringLiteral("异常"), m_footerAbnormalValue);
    addFooter(QStringLiteral("SDK"), m_footerSdkValue);
    layout->addStretch();

    m_connectionBadge = new QLabel(QStringLiteral("未连接"), frame);
    m_transferBadge = new QLabel(QStringLiteral("普通接收"), frame);
    m_abnormalBadge = new QLabel(QStringLiteral("无"), frame);
    m_scanStateBadge = new QLabel(QStringLiteral("待机"), frame);
    m_scanProgressBadge = new QLabel(QStringLiteral("0/0"), frame);
    m_temperatureBadge = new QLabel(QStringLiteral("31.4 C"), frame);

    QLabel* badges[] = {
        m_connectionBadge, m_transferBadge, m_abnormalBadge, m_scanStateBadge, m_scanProgressBadge, m_temperatureBadge
    };
    for (auto* badge : badges) {
        badge->setObjectName("MutedLabel");
        layout->addWidget(badge);
    }

    return frame;
}

QWidget* MainWindow::makeMetricCard(const QString& name, QLabel*& valueLabel)
{
    auto* frame = makePanel("MetricCard");
    frame->setMinimumWidth(150);
    frame->setMinimumHeight(86);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);
    auto* nameLabel = new QLabel(name, frame);
    nameLabel->setProperty("class", "metricName");
    valueLabel = new QLabel(QStringLiteral("--"), frame);
    valueLabel->setProperty("class", "metricValue");
    valueLabel->setWordWrap(true);
    valueLabel->setMinimumHeight(34);
    valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(nameLabel);
    layout->addWidget(valueLabel, 1);
    return frame;
}

QWidget* MainWindow::makeOperationNode(const QString& step, const QString& title, QLabel*& detailLabel)
{
    auto* frame = makePanel("OperationNode");
    frame->setMinimumSize(154, 88);
    frame->setMaximumHeight(92);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(8);
    auto* stepLabel = new QLabel(step, frame);
    stepLabel->setProperty("class", "operationStep");
    stepLabel->setAlignment(Qt::AlignCenter);
    stepLabel->setFixedSize(28, 24);
    auto* titleLabel = new QLabel(title, frame);
    titleLabel->setProperty("class", "operationTitle");
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    topRow->addWidget(stepLabel);
    topRow->addWidget(titleLabel, 1);
    layout->addLayout(topRow);

    detailLabel = new QLabel(QStringLiteral("--"), frame);
    detailLabel->setProperty("class", "operationDetail");
    detailLabel->setWordWrap(true);
    detailLabel->setMinimumHeight(32);
    detailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(detailLabel, 1);
    return frame;
}

QWidget* MainWindow::makeDarkViewport(const QString& title, const QString& subtitle)
{
    auto* frame = makePanel("DarkPanel");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(0);

    auto* tag = new QLabel(title, frame);
    tag->setProperty("class", "overlayTag");
    tag->setAlignment(Qt::AlignCenter);
    tag->setFixedWidth(120);

    auto* desc = new QLabel(subtitle, frame);
    desc->setObjectName("AppSubtitle");
    desc->setStyleSheet("color: #bcc5d0;");

    auto* spacer = new QWidget(frame);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(desc);
    bottom->addStretch();

    layout->addWidget(tag, 0, Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(spacer, 1);
    layout->addLayout(bottom);
    return frame;
}

QWidget* MainWindow::makeImageViewport(
    const QString& title, const QString& subtitle, QLabel*& imageView)
{
    auto* frame = makePanel("DarkPanel");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    auto* tag = new QLabel(title, frame);
    tag->setProperty("class", "overlayTag");
    tag->setAlignment(Qt::AlignCenter);
    tag->setMinimumWidth(140);

    imageView = new QLabel(QStringLiteral("等待自动化基线产物"), frame);
    imageView->setAlignment(Qt::AlignCenter);
    imageView->setMinimumSize(220, 170);
    imageView->setStyleSheet(QStringLiteral("color: #8e99a8; background: #10151c;"));

    auto* desc = new QLabel(subtitle, frame);
    desc->setObjectName("AppSubtitle");
    desc->setStyleSheet("color: #bcc5d0;");

    layout->addWidget(tag, 0, Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(imageView, 1);
    layout->addWidget(desc);
    return frame;
}

QString MainWindow::selectedPrimaryScene() const
{
    if (!m_primarySceneCombo || m_primarySceneCombo->currentIndex() < 0) {
        return m_catalog.isEmpty() ? QString() : m_catalog.first().primaryScene;
    }
    return m_primarySceneCombo->currentData().toString();
}

QString MainWindow::selectedTarget() const
{
    if (!m_targetCombo || m_targetCombo->currentIndex() < 0) {
        return QString();
    }
    return m_targetCombo->currentData().toString();
}

void MainWindow::populatePrimaryScenes()
{
    if (!m_primarySceneCombo) {
        return;
    }

    const QSignalBlocker blocker(m_primarySceneCombo);
    m_primarySceneCombo->clear();

    QStringList primaryScenes;
    for (const auto& scene : m_catalog) {
        if (!primaryScenes.contains(scene.primaryScene)) {
            primaryScenes.append(scene.primaryScene);
        }
    }

    for (const auto& primaryScene : primaryScenes) {
        int count = 0;
        for (const auto& scene : m_catalog) {
            if (scene.primaryScene == primaryScene) {
                ++count;
            }
        }
        m_primarySceneCombo->addItem(primaryScene + QStringLiteral("（%1）").arg(count), primaryScene);
    }

    if (m_primarySceneCombo->count() > 0) {
        m_primarySceneCombo->setCurrentIndex(0);
    }
    populateTargetsForScene();
}

void MainWindow::populateTargetsForScene()
{
    if (!m_targetCombo) {
        return;
    }

    const QString primaryScene = selectedPrimaryScene();
    const QSignalBlocker blocker(m_targetCombo);
    m_targetCombo->clear();

    QStringList targets;
    for (const auto& scene : m_catalog) {
        if (scene.primaryScene == primaryScene && !targets.contains(scene.target)) {
            targets.append(scene.target);
        }
    }

    for (const auto& target : targets) {
        m_targetCombo->addItem(target, target);
    }

    if (m_targetCombo->count() > 0) {
        m_targetCombo->setCurrentIndex(0);
    }
    populateTemplatesForSelection();
}

void MainWindow::populateTemplatesForSelection()
{
    if (!m_sceneList) {
        return;
    }

    const QString primaryScene = selectedPrimaryScene();
    const QString target = selectedTarget();
    const QString keyword = m_templateSearchEdit ? m_templateSearchEdit->text().trimmed() : QString();
    const QSignalBlocker blocker(m_sceneList);
    m_sceneList->clear();

    for (int i = 0; i < m_catalog.size(); ++i) {
        const auto& scene = m_catalog.at(i);
        if (scene.primaryScene != primaryScene || scene.target != target) {
            continue;
        }
        const QString searchable = scene.name + QStringLiteral(" ") + scene.target + QStringLiteral(" ") + scene.sequence
            + QStringLiteral(" ") + scene.outputType + QStringLiteral(" ") + scene.acquisitionProtocol;
        if (!keyword.isEmpty() && !searchable.contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }
        auto* item = new QListWidgetItem(scene.name + QStringLiteral("\n") + scene.outputType);
        item->setData(Qt::UserRole, i);
        item->setToolTip(scene.sequence + QStringLiteral("\n") + scene.note);
        item->setSizeHint(QSize(0, 72));
        m_sceneList->addItem(item);
    }

    if (m_sceneList->count() > 0) {
        m_sceneList->setCurrentRow(0);
    }
}

void MainWindow::handlePrimarySceneChanged()
{
    populateTargetsForScene();
    handleSceneChanged();
}

void MainWindow::handleTargetChanged()
{
    populateTemplatesForSelection();
    handleSceneChanged();
}

void MainWindow::handleTemplateSearchChanged()
{
    populateTemplatesForSelection();
    handleSceneChanged();
}

void MainWindow::handleSceneChanged()
{
    applyScene(currentScene());
}

MriSdkResult MainWindow::loadSdkAndConnect(const QString& dllPath, const MriSdkConfig& config)
{
    const MriSdkResult loadResult = m_bridge->loadSdk(dllPath);
    if (!loadResult.ok) {
        return loadResult;
    }

    m_selectedDllPath = QFileInfo(dllPath).absoluteFilePath();
    appendLog(QStringLiteral("Auto-connect: init=%1, par=%2, output=%3")
                  .arg(config.initPath, config.parameterPath, config.outputPath));
    return m_bridge->connectDevice(config);
}

MriSdkSessionState MainWindow::deviceSessionState() const
{
    return m_bridge->sessionState();
}

void MainWindow::handleConnect()
{
    if (m_selectedDllPath.isEmpty()) {
        appendLog(QStringLiteral("请先选择 mridll.dll"));
        return;
    }

    const QDir sdkRoot = QFileInfo(m_selectedDllPath).absoluteDir();
    MriSdkConfig config;
    config.initPath = sdkRoot.filePath(QStringLiteral("hw_cfg/init.ini"));
    config.parameterPath = QStringLiteral("C:/MRIScanner/Scan/PTScan.par");
    config.outputPath = QStringLiteral("D:/mri_data/par0423-3");
    config.outputPrefix = "PTMRIData";
    config.systemSelection = 3;
    appendLog(QStringLiteral("开始建链：init=%1；par=%2；output=%3")
                  .arg(config.initPath, config.parameterPath, config.outputPath));
    static_cast<void>(m_bridge->connectDevice(config));
}

void MainWindow::handleLoadSdk()
{
    const QString dllPath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择 mridll.dll"),
        QString(),
        QStringLiteral("DLL 文件 (*.dll)"));
    if (dllPath.isEmpty()) {
        return;
    }
    const MriSdkResult result = m_bridge->loadSdk(dllPath);
    if (result.ok) {
        m_selectedDllPath = QFileInfo(dllPath).absoluteFilePath();
    }
}

void MainWindow::handlePrecheck()
{
    m_bridge->precheck();
}

void MainWindow::handleDryRun()
{
    m_bridge->dryRunScene(currentScene());
}

void MainWindow::handleStart()
{
    if (isEggControllerMode()) {
        if (m_bridge->sessionState() != MriSdkSessionState::Unloaded &&
            m_bridge->sessionState() != MriSdkSessionState::Closed) {
            appendLog(QStringLiteral("自动化基线拒绝启动：Qt 直接 SDK 会话仍在占用"));
            return;
        }
        if (m_kspaceImageView) {
            m_kspaceImageView->clear();
            m_kspaceImageView->setText(QStringLiteral("等待 K-space 图"));
        }
        if (m_finalImageView) {
            m_finalImageView->clear();
            m_finalImageView->setText(QStringLiteral("等待最终图"));
        }
        if (m_automationStatusLabel) {
            m_automationStatusLabel->setText(QStringLiteral("Starting"));
        }
        if (!m_eggController->start(m_eggControllerConfig)) {
            m_automationStatusLabel->setText(QStringLiteral("Failed"));
            appendLog(QStringLiteral("自动化基线启动失败：Python、代理或工作目录无效"));
        }
        updateControlMode();
        return;
    }
    static_cast<void>(m_bridge->startScan());
}

void MainWindow::handlePause()
{
    m_bridge->pauseScan();
}

void MainWindow::handleResume()
{
    m_bridge->resumeScan();
}

void MainWindow::handleAbort()
{
    m_bridge->abortScan();
}

void MainWindow::appendLog(const QString& line)
{
    if (m_logView) {
        m_logView->appendPlainText(QTime::currentTime().toString("hh:mm:ss") + QStringLiteral(" ") + line);
    }
}

void MainWindow::updateBadges(const QString& connection, const QString& transfer, const QString& abnormal)
{
    m_connectionBadge->setText(connection);
    m_transferBadge->setText(transfer);
    m_abnormalBadge->setText(abnormal);
    m_footerConnectionValue->setText(connection);
    m_footerAbnormalValue->setText(abnormal);

    m_connectionBadge->setProperty("class", badgeClassForState(connection));
    m_transferBadge->setProperty("class", badgeClassForState(transfer));
    m_abnormalBadge->setProperty("class", badgeClassForState(abnormal));
}

void MainWindow::updateScan(const QString& scanState, const QString& scanProgress)
{
    m_scanStateBadge->setText(scanState);
    m_scanProgressBadge->setText(scanProgress);
    m_footerScanValue->setText(scanState + QStringLiteral(" / ") + scanProgress);
    m_scanStateBadge->setProperty("class", badgeClassForState(scanState));
    m_scanProgressBadge->setProperty("class", badgeClassForState(scanState));
}

void MainWindow::updateMetrics(const QString& snr, const QString& uniformity, const QString& peak, const QString& area)
{
    m_snrValue->setText(snr);
    m_uniformityValue->setText(uniformity);
    m_peakValue->setText(peak);
    m_areaValue->setText(area);
}

void MainWindow::updateTemperature(const QString& temperature)
{
    m_temperatureBadge->setText(temperature);
    m_footerTemperatureValue->setText(temperature);
}

void MainWindow::updateSdkStatus(const QString& modeLabel, const QString& pathLabel, const QString& errorLabel)
{
    if (m_headerSdkValue) {
        m_headerSdkValue->setText(QStringLiteral("SDK：%1").arg(modeLabel));
    }
    if (m_footerSdkValue) {
        m_footerSdkValue->setText(pathLabel);
    }
    if (!errorLabel.isEmpty() && errorLabel != QStringLiteral("未加载 SDK")) {
        appendLog(QStringLiteral("SDK 状态：%1").arg(errorLabel));
    }
}

void MainWindow::updateSdkDiagnostic(const QString& status, const QString& filePath, const QString& details)
{
    if (m_sdkDiagnosticView) {
        m_sdkDiagnosticView->setPlainText(details);
    }
    appendLog(QStringLiteral("SDK 诊断：%1 %2").arg(status, filePath));
}

void MainWindow::updateSessionState(MriSdkSessionState state)
{
    if (isEggControllerMode()) {
        updateControlMode();
        return;
    }
    const DeviceActionAvailability actions = actionsForState(state);
    if (m_loadSdkButton) m_loadSdkButton->setEnabled(actions.canLoadSdk);
    if (m_connectButton) m_connectButton->setEnabled(actions.canConnect);
    if (m_startButton) m_startButton->setEnabled(actions.canRun);
    if (m_pauseButton) m_pauseButton->setEnabled(false);
    if (m_abortButton) m_abortButton->setEnabled(actions.canAbort);
}

void MainWindow::updateControlMode()
{
    if (!isEggControllerMode()) {
        if (m_controlModeCombo) m_controlModeCombo->setEnabled(!m_eggController->isRunning());
        updateSessionState(m_bridge->sessionState());
        return;
    }

    const bool directSessionFree = m_bridge->sessionState() == MriSdkSessionState::Unloaded ||
                                   m_bridge->sessionState() == MriSdkSessionState::Closed;
    const bool configured = QFileInfo(m_eggControllerConfig.program).isFile() &&
                            QFileInfo(m_eggControllerConfig.workingDirectory).isDir();
    if (m_controlModeCombo) m_controlModeCombo->setEnabled(!m_eggController->isRunning());
    if (m_loadSdkButton) m_loadSdkButton->setEnabled(false);
    if (m_connectButton) m_connectButton->setEnabled(false);
    if (m_startButton) m_startButton->setEnabled(configured && directSessionFree && !m_eggController->isRunning());
    if (m_pauseButton) m_pauseButton->setEnabled(false);
    if (m_abortButton) m_abortButton->setEnabled(false);
}

bool MainWindow::isEggControllerMode() const
{
    return m_controlModeCombo &&
           m_controlModeCombo->currentData().toString() == QStringLiteral("eggcontroller");
}

void MainWindow::showEggControllerArtifacts(const EggControllerArtifacts& artifacts)
{
    const QPixmap kspace(artifacts.kspaceImagePath);
    const QPixmap finalImage(artifacts.finalImagePath);
    if (kspace.isNull() || finalImage.isNull()) {
        if (m_automationStatusLabel) {
            m_automationStatusLabel->setText(QStringLiteral("Failed"));
        }
        appendLog(QStringLiteral("自动化基线失败：Qt 无法加载返回图片"));
        updateControlMode();
        return;
    }

    m_kspaceImageView->setPixmap(kspace.scaled(
        m_kspaceImageView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_finalImageView->setPixmap(finalImage.scaled(
        m_finalImageView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    const ImageQualityResult quality = ImageQualityEvaluator::evaluate(finalImage.toImage());
    if (quality.ok) {
        updateMetrics(
            QStringLiteral("%1 dB").arg(quality.snrDb, 0, 'f', 1),
            QStringLiteral("%1 %").arg(quality.uniformityPercent, 0, 'f', 1),
            QStringLiteral("%1 × %2 px")
                .arg(quality.objectSizePixels.width())
                .arg(quality.objectSizePixels.height()),
            QStringLiteral("不可评估（需重复）"));
        appendLog(QStringLiteral("只读图像质控（8-bit 显示图像级估计）：SNR=%1 dB，均匀性=%2 %，对象尺寸=%3×%4 px；重复稳定性需要多次成像")
                      .arg(quality.snrDb, 0, 'f', 1)
                      .arg(quality.uniformityPercent, 0, 'f', 1)
                      .arg(quality.objectSizePixels.width())
                      .arg(quality.objectSizePixels.height()));
    } else {
        updateMetrics(
            QStringLiteral("不可评估"),
            QStringLiteral("不可评估"),
            QStringLiteral("不可评估"),
            QStringLiteral("不可评估（需重复）"));
        appendLog(QStringLiteral("只读图像质控未完成：%1").arg(quality.error));
    }

    const auto appendEvidence = [this](const QString& label, const QString& path) {
        QFile file(path);
        QByteArray hash;
        if (file.open(QIODevice::ReadOnly)) {
            QCryptographicHash hasher(QCryptographicHash::Sha256);
            hasher.addData(&file);
            hash = hasher.result().toHex().toUpper();
        }
        const QFileInfo info(path);
        appendLog(QStringLiteral("%1：%2；%3 字节；%4；SHA-256 %5")
                      .arg(label, info.absoluteFilePath())
                      .arg(info.size())
                      .arg(info.lastModified().toUTC().toString(Qt::ISODateWithMs), QString::fromLatin1(hash)));
    };
    appendEvidence(QStringLiteral("本次 RAW"), artifacts.rawPath);
    appendEvidence(QStringLiteral("本次 K-space 图"), artifacts.kspaceImagePath);
    appendEvidence(QStringLiteral("本次最终图"), artifacts.finalImagePath);
    appendLog(QStringLiteral("自动化任务 %1 已完成并显示；不会自动启动第二次扫描").arg(artifacts.taskId));
    if (m_automationStatusLabel) {
        m_automationStatusLabel->setText(QStringLiteral("Ready"));
    }
    updateControlMode();
}

void MainWindow::applyScene(const SceneTemplate& scene)
{
    if (m_sceneTitle) {
        m_sceneTitle->setText(QStringLiteral("任务选择"));
    }
    if (m_sceneTarget) {
        m_sceneTarget->setText(scene.primaryScene);
    }
    if (m_sceneSequence) {
        m_sceneSequence->setText(scene.target);
    }
    if (m_sceneStepA) {
        m_sceneStepA->setText(scene.name);
    }
    if (m_sceneStepB) {
        m_sceneStepB->setText(scene.sequence);
    }
    if (m_sceneProcessing) {
        m_sceneProcessing->setText(scene.runGate);
    }
    if (m_sceneAnalysis) {
        m_sceneAnalysis->setText(scene.handoffTarget);
    }
    if (m_sceneNote) {
        m_sceneNote->setText(QStringLiteral("协议参数见右侧；当前参数仅显示，不直接写入 SDK。"));
    }
    if (m_headerSceneValue) {
        m_headerSceneValue->setText(QStringLiteral("当前任务：") + scene.name);
    }
    if (m_chainSummary) {
        m_chainSummary->setText(
            QStringLiteral("获取图像：%1\n准备与预检：%2\n定位与采集：%3\n处理与重建：%4\n质控与输出：%5")
                .arg(scene.acquisitionProtocol, scene.preparation, scene.positioning, scene.reconstruction, scene.qcOutput));
    }
    if (m_presetVersionValue) {
        m_presetVersionValue->setText(scene.presetVersion);
    }
    if (m_parameterStatusValue) {
        m_parameterStatusValue->setText(scene.parameterStatus + QStringLiteral(" / ") + scene.adaptationStatus);
    }
    if (m_runGateValue) {
        m_runGateValue->setText(scene.runGate);
    }
    if (m_sdkMappingValue) {
        m_sdkMappingValue->setText(scene.sdkMappingStatus);
    }
    if (m_physicsCheckValue) {
        m_physicsCheckValue->setText(scene.physicsCheckStatus);
    }
    if (m_handoffValue) {
        m_handoffValue->setText(scene.handoffTarget);
    }
    if (m_parameterDetailsView) {
        m_parameterDetailsView->setPlainText(scene.parameterDetails);
    }
    if (m_sdkDiagnosticView) {
        m_sdkDiagnosticView->setPlainText(
            QStringLiteral("当前模板：%1\nSDK 映射：%2\n点击左侧 DRY_RUN 生成字段白名单和参数文件预览。\n真实 Run=%3。")
                .arg(scene.name, scene.sdkMappingStatus, scene.runGate));
    }
    setOperationChain(scene);
    updateMetrics(scene.snr, scene.uniformity, scene.peak, scene.area);
}

SceneTemplate MainWindow::currentScene() const
{
    if (m_sceneList && m_sceneList->currentItem()) {
        const int catalogIndex = m_sceneList->currentItem()->data(Qt::UserRole).toInt();
        if (catalogIndex >= 0 && catalogIndex < m_catalog.size()) {
            return m_catalog.at(catalogIndex);
        }
    }
    return m_catalog.isEmpty() ? SceneTemplate() : m_catalog.first();
}

void MainWindow::setOperationChain(const SceneTemplate& scene)
{
    const QString values[6] = {
        scene.name,
        scene.preparation,
        scene.positioning,
        scene.acquisitionProtocol,
        scene.reconstruction,
        scene.qcOutput + QStringLiteral("；") + scene.handoffTarget
    };
    for (int i = 0; i < 6; ++i) {
        if (m_operationDetails[i]) {
            m_operationDetails[i]->setText(values[i]);
        }
    }
}
