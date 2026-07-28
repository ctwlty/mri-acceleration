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
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPainter>
#include <QMouseEvent>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSize>
#include <QSplitter>
#include <QStyle>
#include <QStringList>
#include <QTableWidget>
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
    connect(m_bridge, &DeviceBridge::deviceStatusChanged, this, &MainWindow::updatePrecheckStatus);
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
    refreshWorkflow();
}

class LocalizationPlannerView final : public QWidget {
public:
    explicit LocalizationPlannerView(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("LocalizationPlannerView"));
        setMinimumHeight(260);
        setMouseTracking(true);
        setProperty("readPhaseSwapped", false);
        setProperty("planningCoverageModified", false);
    }

    void swapReadPhase()
    {
        m_axesSwapped = !m_axesSwapped;
        setProperty("readPhaseSwapped", m_axesSwapped);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(QStringLiteral("#111923")));
        painter.setRenderHint(QPainter::Antialiasing);
        const int margin = 18;
        const int gap = 12;
        const int tileWidth = qMax(90, (width() - 2 * margin - 2 * gap) / 3);
        const int tileHeight = qMax(150, height() - 46);
        const QStringList views = {QStringLiteral("轴位"), QStringLiteral("冠状"), QStringLiteral("矢状")};
        for (int index = 0; index < views.size(); ++index) {
            const QRect tile(margin + index * (tileWidth + gap), 26, tileWidth, tileHeight);
            painter.setPen(QPen(QColor(QStringLiteral("#50657e")), 1));
            painter.setBrush(QColor(QStringLiteral("#1c2a3a")));
            painter.drawRoundedRect(tile, 6, 6);
            painter.setPen(QColor(QStringLiteral("#b8d0e8")));
            painter.drawText(tile.left() + 8, tile.top() + 18, views.at(index));
            const QRectF coverage(tile.left() + tile.width() * m_boxX,
                                  tile.top() + tile.height() * m_boxY,
                                  tile.width() * m_boxWidth,
                                  tile.height() * m_boxHeight);
            painter.setPen(QPen(QColor(QStringLiteral("#4ed2c2")), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(coverage);
            const QPointF center(coverage.left() + coverage.width() * m_centerX,
                                 coverage.top() + coverage.height() * m_centerY);
            painter.setPen(QPen(QColor(QStringLiteral("#f3bb55")), 2));
            painter.drawLine(QPointF(center.x() - 9, center.y()), QPointF(center.x() + 9, center.y()));
            painter.drawLine(QPointF(center.x(), center.y() - 9), QPointF(center.x(), center.y() + 9));
            painter.setPen(QPen(QColor(QStringLiteral("#d998e6")), 2));
            const qreal sliceY = coverage.top() + coverage.height() * m_slice;
            painter.drawLine(coverage.left(), sliceY, coverage.right(), sliceY);
            painter.setPen(QColor(QStringLiteral("#9fb6cd")));
            painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
            painter.drawText(tile.left() + 8, tile.bottom() - 8,
                             index == 0
                                 ? QStringLiteral("%1 / %2").arg(m_axesSwapped ? QStringLiteral("Phase") : QStringLiteral("Read"), m_axesSwapped ? QStringLiteral("Read") : QStringLiteral("Phase"))
                                 : QStringLiteral("Mock LOC · 规划示例"));
        }
        painter.setPen(QColor(QStringLiteral("#d7e4ef")));
        painter.drawText(18, 16, QStringLiteral("三标准方位 · 拖动中心十字、覆盖框或切片线（Mock 规划）"));
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        m_dragMode = 1;
        if (event->position().y() > height() * 0.62) {
            m_dragMode = 3;
        } else if (event->position().x() > width() * 0.72) {
            m_dragMode = 2;
        }
        mouseMoveEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_dragMode == 0 || !(event->buttons() & Qt::LeftButton)) {
            return;
        }
        const qreal x = qBound<qreal>(0.08, event->position().x() / qMax(1, width()), 0.92);
        const qreal y = qBound<qreal>(0.10, event->position().y() / qMax(1, height()), 0.90);
        if (m_dragMode == 1) {
            m_centerX = x;
            m_centerY = y;
        } else if (m_dragMode == 2) {
            m_boxWidth = qBound<qreal>(0.30, x, 0.78);
            m_boxHeight = qBound<qreal>(0.30, y, 0.78);
        } else {
            m_slice = y;
        }
        setProperty("planningCoverageModified", true);
        update();
    }

    void mouseReleaseEvent(QMouseEvent*) override { m_dragMode = 0; }

private:
    bool m_axesSwapped = false;
    int m_dragMode = 0;
    qreal m_boxX = 0.16;
    qreal m_boxY = 0.20;
    qreal m_boxWidth = 0.66;
    qreal m_boxHeight = 0.56;
    qreal m_centerX = 0.50;
    qreal m_centerY = 0.50;
    qreal m_slice = 0.50;
};

class MockImagingCanvas final : public QWidget {
public:
    explicit MockImagingCanvas(const QString& objectName, const QString& caption, QWidget* parent = nullptr)
        : QWidget(parent), m_caption(caption)
    {
        setObjectName(objectName);
        setMinimumHeight(250);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor(QStringLiteral("#111923")));

        const QRectF viewport = rect().adjusted(22, 34, -22, -28);
        painter.setPen(QPen(QColor(QStringLiteral("#38546b")), 1));
        painter.setBrush(QColor(QStringLiteral("#162536")));
        painter.drawRoundedRect(viewport, 8, 8);
        const QPointF center = viewport.center();
        const qreal radius = qMin(viewport.width(), viewport.height()) * 0.33;
        QRadialGradient gradient(center, radius);
        gradient.setColorAt(0.0, QColor(QStringLiteral("#d2e9ef")));
        gradient.setColorAt(0.45, QColor(QStringLiteral("#6ca7bb")));
        gradient.setColorAt(1.0, QColor(QStringLiteral("#17324a")));
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawEllipse(center, radius, radius * 0.78);
        painter.setPen(QPen(QColor(QStringLiteral("#f1c65a")), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center + QPointF(radius * 0.16, -radius * 0.08), radius * 0.34, radius * 0.26);
        painter.setPen(QPen(QColor(QStringLiteral("#50d0c0")), 1));
        painter.drawLine(QPointF(viewport.left() + 14, center.y()), QPointF(viewport.right() - 14, center.y()));
        painter.drawLine(QPointF(center.x(), viewport.top() + 14), QPointF(center.x(), viewport.bottom() - 14));
        painter.setPen(QColor(QStringLiteral("#d8e6ef")));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
        painter.drawText(QRectF(18, 8, width() - 36, 22), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("Mock / 设计示例 · %1").arg(m_caption));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
        painter.setPen(QColor(QStringLiteral("#9db2c4")));
        painter.drawText(QRectF(18, height() - 22, width() - 36, 16), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("非设备采集图像；不关联 RAW、SDK 或真实扫描"));
    }

private:
    QString m_caption;
};

void MainWindow::configureEggController(const EggControllerLaunchConfig& config)
{
    m_eggControllerConfig = config;
    if (m_automationStatusLabel) {
        m_automationStatusLabel->setText(QStringLiteral("已配置；真实入口保持 HOLD"));
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
    m_controlModeCombo->addItem(QStringLiteral("Mock 工作流（安全）"), QStringLiteral("mock"));
    m_controlModeCombo->setEnabled(false);
    m_automationStatusLabel = new QLabel(QStringLiteral("真实设备：HOLD"), modePanel);
    m_automationStatusLabel->setObjectName("AutomationStatusLabel");
    m_automationStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    modeLayout->addWidget(modeLabel, 0, 0);
    modeLayout->addWidget(m_controlModeCombo, 0, 1);
    modeLayout->addWidget(m_automationStatusLabel, 1, 0, 1, 2);
    layout->addWidget(modePanel);

    auto* buttons = new QGridLayout;
    buttons->setHorizontalSpacing(10);
    buttons->setVerticalSpacing(10);

    m_loadSdkButton = new QPushButton(QStringLiteral("真实 SDK（HOLD）"), frame);
    m_loadSdkButton->setProperty("class", "secondary");
    m_loadSdkButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    m_connectButton = new QPushButton(QStringLiteral("真实设备连接（HOLD）"), frame);
    m_connectButton->setProperty("class", "primary");
    m_connectButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    auto* precheckBtn = new QPushButton(QStringLiteral("真实预检（HOLD）"), frame);
    precheckBtn->setProperty("class", "secondary");
    auto* dryRunBtn = new QPushButton(QStringLiteral("参数快照（Mock）"), frame);
    dryRunBtn->setProperty("class", "secondary");
    m_startButton = new QPushButton(QStringLiteral("真实 Run（HOLD）"), frame);
    m_startButton->setObjectName("RealRunButton");
    m_startButton->setProperty("class", "success");
    m_startButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_pauseButton = new QPushButton(QStringLiteral("暂停（不支持）"), frame);
    m_pauseButton->setProperty("class", "warning");
    m_pauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    m_pauseButton->setToolTip(QStringLiteral("当前 SDK 未提供暂停/继续接口"));
    m_pauseButton->setEnabled(false);
    m_abortButton = new QPushButton(QStringLiteral("真实 Abort（HOLD）"), frame);
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

    m_loadSdkButton->setEnabled(false);
    m_connectButton->setEnabled(false);
    precheckBtn->setEnabled(false);
    m_startButton->setEnabled(false);
    m_abortButton->setEnabled(false);

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
    layout->setSpacing(10);

    auto* statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(0, 0, 0, 0);
    m_workflowStatusLabel = new QLabel(frame);
    m_workflowStatusLabel->setObjectName(QStringLiteral("WorkflowStatusStrip"));
    m_workflowStatusLabel->setProperty("class", "workflowStatus");
    m_workflowStatusLabel->setWordWrap(true);
    m_workflowCurrentStepLabel = new QLabel(frame);
    m_workflowCurrentStepLabel->setObjectName(QStringLiteral("WorkflowCurrentStep"));
    m_workflowCurrentStepLabel->setProperty("class", "workflowStepChip");
    m_workflowCurrentStepLabel->setAlignment(Qt::AlignCenter);
    m_workflowCurrentStepLabel->setFixedWidth(38);
    statusRow->addWidget(m_workflowStatusLabel, 1);
    statusRow->addWidget(m_workflowCurrentStepLabel);
    layout->addLayout(statusRow);

    m_workflowPages = new QStackedWidget(frame);
    m_workflowPages->setObjectName(QStringLiteral("WorkflowPageStack"));
    for (int step = 1; step <= 13; ++step) {
        m_workflowPages->addWidget(makeWorkflowPage(step));
    }
    layout->addWidget(m_workflowPages, 1);

    auto* navRow = new QHBoxLayout;
    m_workflowBackButton = new QPushButton(QStringLiteral("上一步"), frame);
    m_workflowBackButton->setObjectName(QStringLiteral("WorkflowBackButton"));
    m_workflowBackButton->setProperty("class", "secondary");
    m_workflowNextButton = new QPushButton(QStringLiteral("下一步"), frame);
    m_workflowNextButton->setObjectName(QStringLiteral("WorkflowNextButton"));
    m_workflowNextButton->setProperty("class", "primary");
    navRow->addWidget(m_workflowBackButton);
    navRow->addStretch();
    navRow->addWidget(m_workflowNextButton);
    layout->addLayout(navRow);

    connect(m_workflowBackButton, &QPushButton::clicked, this, [this] {
        setWorkflowStep(m_workflowStep == 13 ? 12 : qMax(1, m_workflowStep - 1));
    });
    connect(m_workflowNextButton, &QPushButton::clicked, this, [this] {
        if (m_workflowStep < 12) setWorkflowStep(m_workflowStep + 1);
    });
    refreshWorkflow();

    return frame;
}

QWidget* MainWindow::makeWorkflowPage(int step)
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("WorkflowPage%1").arg(step, 2, 10, QLatin1Char('0')));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(12);

    auto* label = new QLabel(page);
    label->setObjectName(QStringLiteral("WorkflowBodyLabel%1").arg(step, 2, 10, QLatin1Char('0')));
    label->setWordWrap(true);
    label->setProperty("class", "workflowBody");
    layout->addWidget(label);

    const auto addCard = [page, layout](const QString& title, const QString& detail) {
        auto* card = makePanel("WorkflowCard");
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        auto* cardTitle = new QLabel(title, card);
        cardTitle->setProperty("class", "workflowCardTitle");
        auto* cardDetail = new QLabel(detail, card);
        cardDetail->setWordWrap(true);
        cardDetail->setObjectName("AppSubtitle");
        cardLayout->addWidget(cardTitle);
        cardLayout->addWidget(cardDetail);
        layout->addWidget(card);
        return card;
    };
    const auto addMockImage = [page, layout](const QString& objectName, const QString& caption) {
        auto* image = new MockImagingCanvas(objectName, caption, page);
        layout->addWidget(image, 1);
        return image;
    };

    switch (step) {
    case 1:
        label->setText(QStringLiteral("入口检查 · 当前为安全演示环境"));
        addCard(QStringLiteral("SDK / 设备 / 存储"),
                QStringLiteral("SDK 未加载｜设备未连接｜输出位置待确认。真实入口固定 HOLD；本流程仅提供 Mock 设计示例。"));
        addCard(QStringLiteral("下一步"), QStringLiteral("选择场景、对象与推荐模板。"));
        break;
    case 2:
        label->setText(QStringLiteral("场景、对象与模板推荐"));
        addCard(QStringLiteral("当前推荐"), QStringLiteral("默认内部结构成像：标准模体 / 组织样品 / 根茎样品。可在左栏筛选；所有推荐均为设计示例。"));
        break;
    case 3: {
        label->setText(QStringLiteral("模板确认 · 默认协议链"));
        m_protocolChainLabel = new QLabel(QStringLiteral("LOC → FSE A（默认 Mock 协议链）"), page);
        m_protocolChainLabel->setObjectName(QStringLiteral("ProtocolChainLabel"));
        m_protocolChainLabel->setProperty("class", "workflowProtocol");
        m_protocolChainLabel->setWordWrap(true);
        layout->addWidget(m_protocolChainLabel);
        m_addComparisonButton = new QPushButton(QStringLiteral("添加 FSE B 对照"), page);
        m_addComparisonButton->setObjectName(QStringLiteral("AddComparisonButton"));
        m_addComparisonButton->setProperty("class", "secondary");
        layout->addWidget(m_addComparisonButton, 0, Qt::AlignLeft);
        connect(m_addComparisonButton, &QPushButton::clicked, this, [this] {
            m_comparisonEnabled = true;
            refreshWorkflow();
        });
        addCard(QStringLiteral("确认内容"), QStringLiteral("只确认 Mock 模板与预期输出；不会写入 SDK 参数。"));
        break;
    }
    case 4:
        label->setText(QStringLiteral("样品登记与预检"));
        addCard(QStringLiteral("样品"), QStringLiteral("待登记：编号、批次、研究者。来源：用户输入，当前未录入。"));
        addCard(QStringLiteral("线圈 / 摆位 / 存储 / 安全"), QStringLiteral("均为待确认；不以界面颜色声明真实就绪。"));
        break;
    case 5: {
        auto* showL3 = new QPushButton(QStringLiteral("展开 L3 折叠参数"), page);
        showL3->setObjectName(QStringLiteral("ShowL3Button"));
        showL3->setProperty("class", "secondary");
        auto* l3Detail = new QLabel(QStringLiteral("L3：回波链、层厚与带宽为 Mock 参数示例；L4 硬件字段保持隐藏。"), page);
        l3Detail->setObjectName(QStringLiteral("L3DetailsLabel"));
        l3Detail->setWordWrap(true);
        l3Detail->setVisible(false);
        connect(showL3, &QPushButton::clicked, this, [showL3, l3Detail] {
            const bool show = !l3Detail->isVisible();
            l3Detail->setVisible(show);
            showL3->setText(show ? QStringLiteral("收起 L3 参数") : QStringLiteral("展开 L3 折叠参数"));
        });
        label->setText(QStringLiteral("协议链与扫描参数"));
        addCard(QStringLiteral("L2 · LOC"), QStringLiteral("TR 800 ms｜TE 12.9 ms｜FOV 50×50 mm｜矩阵 64×64（Mock 参数示例）"));
        addCard(QStringLiteral("L2 · FSE A"), QStringLiteral("TR 3000 ms｜TE 12.9 ms｜FOV 50×50 mm｜矩阵 128×128（Mock 参数示例）"));
        layout->addWidget(showL3, 0, Qt::AlignLeft);
        layout->addWidget(l3Detail);
        break;
    }
    case 6:
        label->setText(QStringLiteral("LOC 定位 · Mock 进度"));
        addMockImage(QStringLiteral("MockLocImage"), QStringLiteral("LOC 定位"));
        addCard(QStringLiteral("Mock LOC 图"), QStringLiteral("设计示例，非同次采集图像。进度：100% Mock 完成；下一步进入定位规划。"));
        break;
    case 7: {
        label->setText(QStringLiteral("定位规划 · 三标准方位"));
        m_localizationPlanner = new LocalizationPlannerView(page);
        layout->addWidget(m_localizationPlanner, 1);
        auto* controls = new QHBoxLayout;
        auto* swap = new QPushButton(QStringLiteral("交换 Read / Phase"), page);
        swap->setObjectName(QStringLiteral("ReadPhaseSwapButton"));
        swap->setProperty("class", "secondary");
        auto* more = new QPushButton(QStringLiteral("更多方位"), page);
        more->setProperty("class", "secondary");
        controls->addWidget(swap);
        controls->addWidget(more);
        controls->addStretch();
        layout->addLayout(controls);
        connect(swap, &QPushButton::clicked, this, [this] {
            static_cast<LocalizationPlannerView*>(m_localizationPlanner)->swapReadPhase();
        });
        connect(more, &QPushButton::clicked, this, [more] {
            more->setText(QStringLiteral("自定义斜切：仅 Mock 规划"));
        });
        break;
    }
    case 8: {
        label->setText(QStringLiteral("运行前确认与参数快照"));
        addCard(QStringLiteral("真实 Run"), QStringLiteral("永久 HOLD：本页面不加载 SDK、不连接设备、不调用 Run 或 Abort。"));
        m_realRunButton = new QPushButton(QStringLiteral("真实 Run（HOLD）"), page);
        m_realRunButton->setObjectName(QStringLiteral("WorkflowRealRunButton"));
        m_realRunButton->setProperty("class", "danger");
        m_realRunButton->setEnabled(false);
        m_mockAcquireButton = new QPushButton(QStringLiteral("进入 FSE A Mock 采集"), page);
        m_mockAcquireButton->setObjectName(QStringLiteral("MockAcquireButton"));
        m_mockAcquireButton->setProperty("class", "primary");
        layout->addWidget(m_realRunButton);
        layout->addWidget(m_mockAcquireButton);
        connect(m_mockAcquireButton, &QPushButton::clicked, this, [this] { setWorkflowStep(9); });
        break;
    }
    case 9: {
        label->setText(QStringLiteral("FSE A Mock 采集"));
        addMockImage(QStringLiteral("MockAcquisitionImage"), QStringLiteral("FSE A 采集进度"));
        auto* progress = new QProgressBar(page);
        progress->setObjectName(QStringLiteral("MockAcquisitionProgress"));
        progress->setRange(0, 100);
        progress->setValue(100);
        progress->setFormat(QStringLiteral("Mock 采集 100% · 未触发设备"));
        layout->addWidget(progress);
        addCard(QStringLiteral("Mock 采集进度"), QStringLiteral("FSE A · 100% · 仅为设计示例；未触发设备、SDK 或扫描。"));
        break;
    }
    case 10: {
        label->setText(QStringLiteral("原生输出与标准重建 · Mock"));
        addCard(QStringLiteral("来源绑定"), QStringLiteral("Mock 输出包 / 协议快照 / 任务说明。未声明为已验证 k-space。"));
        auto* parsing = new QProgressBar(page);
        parsing->setObjectName(QStringLiteral("MockParsingProgress"));
        parsing->setValue(100);
        parsing->setFormat(QStringLiteral("解析 100% · Mock"));
        auto* reconstruction = new QProgressBar(page);
        reconstruction->setObjectName(QStringLiteral("MockReconstructionProgress"));
        reconstruction->setValue(100);
        reconstruction->setFormat(QStringLiteral("标准重建 100% · Mock"));
        layout->addWidget(parsing);
        layout->addWidget(reconstruction);
        break;
    }
    case 11:
        label->setText(QStringLiteral("最终 Mock 图与 QC"));
        addMockImage(QStringLiteral("MockResultImage"), QStringLiteral("最终重建结果"));
        addCard(QStringLiteral("最终图"), QStringLiteral("Mock 设计示例，不代表真实设备成像或同次 RAW。"));
        addCard(QStringLiteral("QC"), QStringLiteral("SNR、均匀性和覆盖均为 Mock 示例；科研样品结论待研究者确认。"));
        break;
    case 12: {
        label->setText(QStringLiteral("结果包"));
        addCard(QStringLiteral("包含内容"), QStringLiteral("原始数据引用、标准结果、QC、协议与参数快照、来源记录、任务说明（全部标为 Mock/设计示例）。"));
        m_openHistoryButton = new QPushButton(QStringLiteral("查看只读历史记录"), page);
        m_openHistoryButton->setObjectName(QStringLiteral("OpenHistoryButton"));
        m_openHistoryButton->setProperty("class", "secondary");
        layout->addWidget(m_openHistoryButton, 0, Qt::AlignLeft);
        connect(m_openHistoryButton, &QPushButton::clicked, this, [this] { setWorkflowStep(13); });
        break;
    }
    case 13: {
        auto* filter = new QLineEdit(page);
        filter->setObjectName(QStringLiteral("HistoryFilter"));
        filter->setPlaceholderText(QStringLiteral("筛选历史（只读 Mock 记录）"));
        auto* table = new QTableWidget(2, 4, page);
        table->setObjectName(QStringLiteral("HistoryReadOnlyTable"));
        table->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("模板"), QStringLiteral("结果"), QStringLiteral("来源")});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Mock-01")));
        table->setItem(0, 1, new QTableWidgetItem(QStringLiteral("LOC → FSE A")));
        table->setItem(0, 2, new QTableWidgetItem(QStringLiteral("设计示例")));
        table->setItem(0, 3, new QTableWidgetItem(QStringLiteral("Mock 来源记录")));
        table->setItem(1, 0, new QTableWidgetItem(QStringLiteral("Mock-02")));
        table->setItem(1, 1, new QTableWidgetItem(QStringLiteral("研究样品")));
        table->setItem(1, 2, new QTableWidgetItem(QStringLiteral("待研究者确认")));
        table->setItem(1, 3, new QTableWidgetItem(QStringLiteral("Mock 来源记录")));
        table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(filter);
        layout->addWidget(table, 1);
        label->setText(QStringLiteral("历史记录 · 只读"));
        addCard(QStringLiteral("筛选 / 表格 / 打开结果"), QStringLiteral("历史记录仅用于浏览来源记录与结果包；不调用设备、不写入数据。"));
        m_backToResultsButton = new QPushButton(QStringLiteral("返回结果包"), page);
        m_backToResultsButton->setObjectName(QStringLiteral("BackToResultsButton"));
        m_backToResultsButton->setProperty("class", "secondary");
        layout->addWidget(m_backToResultsButton, 0, Qt::AlignLeft);
        connect(m_backToResultsButton, &QPushButton::clicked, this, [this] { setWorkflowStep(12); });
        break;
    }
    default:
        break;
    }
    layout->addStretch();
    return page;
}

void MainWindow::setWorkflowStep(int step)
{
    m_workflowStep = qBound(1, step, 13);
    if (m_workflowPages) m_workflowPages->setCurrentIndex(m_workflowStep - 1);
    refreshWorkflow();
}

void MainWindow::refreshWorkflow()
{
    static const QStringList titles = {
        QStringLiteral("入口"), QStringLiteral("场景对象"), QStringLiteral("模板确认"),
        QStringLiteral("登记预检"), QStringLiteral("协议参数"), QStringLiteral("LOC"),
        QStringLiteral("定位规划"), QStringLiteral("运行前确认"), QStringLiteral("FSE A Mock"),
        QStringLiteral("输出重建"), QStringLiteral("QC"), QStringLiteral("结果包"), QStringLiteral("历史记录")
    };
    if (m_workflowCurrentStepLabel) {
        m_workflowCurrentStepLabel->setText(QStringLiteral("%1").arg(m_workflowStep, 2, 10, QLatin1Char('0')));
    }
    if (m_workflowStatusLabel) {
        const QString completed = m_workflowStep == 1 ? QStringLiteral("—") : QStringLiteral("01–%1").arg(m_workflowStep - 1, 2, 10, QLatin1Char('0'));
        const QString next = m_workflowStep == 13 ? QStringLiteral("返回结果包") : (m_workflowStep == 12 ? QStringLiteral("结果包完成 / 可进入历史") : titles.at(m_workflowStep));
        m_workflowStatusLabel->setText(QStringLiteral("已完成｜当前｜下一步   %1 ｜ %2｜ %3（Mock/设计示例）")
                                           .arg(completed, titles.at(m_workflowStep - 1), next));
    }
    if (m_workflowBackButton) m_workflowBackButton->setEnabled(m_workflowStep > 1);
    if (m_workflowNextButton) m_workflowNextButton->setEnabled(m_workflowStep < 12 && m_workflowStep != 8);
    if (m_protocolChainLabel) {
        m_protocolChainLabel->setText(m_comparisonEnabled
                                          ? QStringLiteral("LOC → FSE A → FSE B 对照（用户已主动添加，Mock）")
                                          : QStringLiteral("LOC → FSE A（默认 Mock 协议链）"));
    }
    if (m_workflowOutputSummary) {
        m_workflowOutputSummary->setText(QStringLiteral("当前步骤 %1 · 所有图像、数值和输出均为 Mock/设计示例").arg(m_workflowStep, 2, 10, QLatin1Char('0')));
    }
}

QWidget* MainWindow::buildRightPane()
{
    auto* frame = makePanel("Panel");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("当前状态 / QC / 输出"), frame);
    title->setObjectName("SectionTitle");
    layout->addWidget(title);

    m_workflowOutputSummary = new QLabel(frame);
    m_workflowOutputSummary->setObjectName(QStringLiteral("WorkflowOutputSummary"));
    m_workflowOutputSummary->setWordWrap(true);
    m_workflowOutputSummary->setProperty("class", "workflowSummary");
    layout->addWidget(m_workflowOutputSummary);

    auto* metricGrid = new QGridLayout;
    metricGrid->setSpacing(10);
    metricGrid->addWidget(makeMetricCard(QStringLiteral("Mock SNR"), m_snrValue), 0, 0);
    metricGrid->addWidget(makeMetricCard(QStringLiteral("Mock 均匀性"), m_uniformityValue), 0, 1);
    metricGrid->addWidget(makeMetricCard(QStringLiteral("覆盖"), m_peakValue), 1, 0);
    metricGrid->addWidget(makeMetricCard(QStringLiteral("研究者确认"), m_areaValue), 1, 1);
    m_snrValue->setObjectName(QStringLiteral("QualitySnrValue"));
    m_uniformityValue->setObjectName(QStringLiteral("QualityUniformityValue"));
    m_peakValue->setObjectName(QStringLiteral("QualitySizeValue"));
    m_areaValue->setObjectName(QStringLiteral("QualityStabilityValue"));
    layout->addLayout(metricGrid);

    auto* statusCard = makePanel("WorkflowCard");
    auto* statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(14, 14, 14, 14);
    auto* stateTitle = new QLabel(QStringLiteral("设备与输出状态"), statusCard);
    stateTitle->setProperty("class", "workflowCardTitle");
    auto* state = new QLabel(QStringLiteral("真实设备：HOLD\nSDK：未加载\n输出：Mock 设计示例\n来源绑定：仅在结果包中只读显示"), statusCard);
    state->setWordWrap(true);
    state->setObjectName("AppSubtitle");
    statusLayout->addWidget(stateTitle);
    statusLayout->addWidget(state);
    layout->addWidget(statusCard);
    layout->addStretch();

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

static QWidget* createSequenceTimeline(QWidget* parent);
static QWidget* createLocalizationCoverageDiagram(QWidget* parent);

QWidget* MainWindow::makeProtocolTimelineViewport()
{
    auto* frame = makePanel("DarkPanel");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(0);

    auto* tag = new QLabel(QStringLiteral("获取图像/协议"), frame);
    tag->setProperty("class", "overlayTag");
    tag->setAlignment(Qt::AlignCenter);
    tag->setFixedWidth(120);

    m_sequenceProtocolSummary = new QLabel(QStringLiteral("已选：PTScan 基线"), frame);
    m_sequenceProtocolSummary->setObjectName(QStringLiteral("SequenceProtocolSummaryLabel"));
    m_sequenceProtocolSummary->setWordWrap(true);
    m_sequenceProtocolSummary->setStyleSheet("color: #bcc5d0;");

    auto* timeline = createSequenceTimeline(frame);

    m_sequenceTimingSummary = new QLabel(QStringLiteral("TR / TE / 采集窗口仅显示当前模板已证实字段"), frame);
    m_sequenceTimingSummary->setWordWrap(true);
    m_sequenceTimingSummary->setStyleSheet("color: #bcc5d0;");

    auto* evidence = new QLabel(QStringLiteral("参数推导示意，非设备实测波形"), frame);
    evidence->setObjectName(QStringLiteral("SequenceTimelineEvidenceLabel"));
    evidence->setStyleSheet("color: #e9a84a;");

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(evidence);
    bottom->addStretch();

    layout->addWidget(tag, 0, Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(m_sequenceProtocolSummary);
    layout->addWidget(timeline, 1);
    layout->addWidget(m_sequenceTimingSummary);
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

QWidget* MainWindow::makePrecheckViewport()
{
    auto* frame = makePanel("DarkPanel");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    auto* tag = new QLabel(QStringLiteral("准备与预检"), frame);
    tag->setProperty("class", "overlayTag");
    tag->setAlignment(Qt::AlignCenter);
    tag->setMinimumWidth(140);

    m_precheckStatusLabel = new QLabel(QStringLiteral("真实预检：待执行（未声明通过）"), frame);
    m_precheckStatusLabel->setObjectName(QStringLiteral("PrecheckStatusLabel"));
    m_precheckStatusLabel->setWordWrap(true);
    m_precheckStatusLabel->setStyleSheet(QStringLiteral("color: #e9a84a; background: #10151c; padding: 12px;"));

    auto* board = makePanel("PrecheckResultBoard");
    auto* boardLayout = new QGridLayout(board);
    boardLayout->setContentsMargins(8, 8, 8, 8);
    boardLayout->setHorizontalSpacing(8);
    boardLayout->setVerticalSpacing(8);
    const auto addStatusCard = [&](int row, int column, const QString& title, const QString& initialStatus,
                                   const QString& objectName, QLabel*& value) {
        auto* card = makePanel("PrecheckStatusCard");
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(10, 8, 10, 8);
        cardLayout->setSpacing(4);
        auto* cardTitle = new QLabel(title, card);
        cardTitle->setStyleSheet(QStringLiteral("color: #9ba8b8;"));
        value = new QLabel(initialStatus, card);
        value->setObjectName(objectName);
        value->setStyleSheet(QStringLiteral("color: #e9a84a; font-weight: 600;"));
        value->setWordWrap(true);
        cardLayout->addWidget(cardTitle);
        cardLayout->addWidget(value);
        boardLayout->addWidget(card, row, column);
    };
    addStatusCard(0, 0, QStringLiteral("样品"), QStringLiteral("待确认 / 未录入"),
                  QStringLiteral("PrecheckSampleStatus"), m_precheckSampleStatus);
    addStatusCard(0, 1, QStringLiteral("线圈"), QStringLiteral("待确认 / 未读取"),
                  QStringLiteral("PrecheckCoilStatus"), m_precheckCoilStatus);
    addStatusCard(1, 0, QStringLiteral("存储"), QStringLiteral("待确认 / 未验证"),
                  QStringLiteral("PrecheckStorageStatus"), m_precheckStorageStatus);
    addStatusCard(1, 1, QStringLiteral("设备连接"), QStringLiteral("待确认 / 未预检"),
                  QStringLiteral("PrecheckDeviceStatus"), m_precheckDeviceStatus);

    layout->addWidget(tag, 0, Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(m_precheckStatusLabel);
    layout->addWidget(board, 1);
    return frame;
}

QWidget* MainWindow::makeLocalizationViewport()
{
    auto* frame = makePanel("DarkPanel");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    auto* tag = new QLabel(QStringLiteral("定位与采集"), frame);
    tag->setProperty("class", "overlayTag");
    tag->setAlignment(Qt::AlignCenter);
    tag->setMinimumWidth(140);

    m_localizationImageView = new QLabel(QStringLiteral("未取得LOC定位图\n当前界面未收到同次 LOC 产物"), frame);
    m_localizationImageView->setObjectName(QStringLiteral("LocalizationImageView"));
    m_localizationImageView->setAlignment(Qt::AlignCenter);
    m_localizationImageView->setWordWrap(true);
    m_localizationImageView->setMinimumSize(220, 170);
    m_localizationImageView->setStyleSheet(QStringLiteral("color: #8e99a8; background: #10151c;"));

    auto* coverageDiagram = createLocalizationCoverageDiagram(frame);

    m_localizationCoverageLabel = new QLabel(
        QStringLiteral("规划覆盖，非采集图像：待同次 LOC 定位像后叠加"), frame);
    m_localizationCoverageLabel->setObjectName(QStringLiteral("LocalizationCoverageLabel"));
    m_localizationCoverageLabel->setWordWrap(true);
    m_localizationCoverageLabel->setStyleSheet(QStringLiteral("color: #bcc5d0;"));

    layout->addWidget(tag, 0, Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(m_localizationImageView, 1);
    layout->addWidget(coverageDiagram);
    layout->addWidget(m_localizationCoverageLabel);
    return frame;
}

QWidget* MainWindow::makeReconstructionViewport()
{
    auto* frame = makePanel("DarkPanel");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    auto* tag = new QLabel(QStringLiteral("处理与重建"), frame);
    tag->setProperty("class", "overlayTag");
    tag->setAlignment(Qt::AlignCenter);
    tag->setMinimumWidth(140);

    auto* views = new QTabWidget(frame);
    views->setObjectName(QStringLiteral("ReconstructionViews"));
    auto* finalTab = new QWidget(views);
    auto* finalLayout = new QVBoxLayout(finalTab);
    finalLayout->setContentsMargins(0, 0, 0, 0);
    m_finalImageView = new QLabel(QStringLiteral("等待自动化基线最终重建图"), finalTab);
    m_finalImageView->setObjectName(QStringLiteral("FinalImageView"));
    m_finalImageView->setAlignment(Qt::AlignCenter);
    m_finalImageView->setMinimumSize(220, 170);
    m_finalImageView->setStyleSheet(QStringLiteral("color: #8e99a8; background: #10151c;"));
    finalLayout->addWidget(m_finalImageView);
    views->addTab(finalTab, QStringLiteral("最终重建图"));

    auto* kspaceTab = new QWidget(views);
    auto* kspaceLayout = new QVBoxLayout(kspaceTab);
    kspaceLayout->setContentsMargins(0, 0, 0, 0);
    m_kspaceImageView = new QLabel(QStringLiteral("等待同次 K-space 产物"), kspaceTab);
    m_kspaceImageView->setObjectName(QStringLiteral("KspaceImageView"));
    m_kspaceImageView->setAlignment(Qt::AlignCenter);
    m_kspaceImageView->setMinimumSize(220, 170);
    m_kspaceImageView->setStyleSheet(QStringLiteral("color: #8e99a8; background: #10151c;"));
    kspaceLayout->addWidget(m_kspaceImageView);
    views->addTab(kspaceTab, QStringLiteral("K-space 子视图"));

    m_reconstructionEvidenceLabel = new QLabel(
        QStringLiteral("来源：等待自动化任务产物；未声明同次已证实"), frame);
    m_reconstructionEvidenceLabel->setObjectName(QStringLiteral("ReconstructionEvidenceLabel"));
    m_reconstructionEvidenceLabel->setWordWrap(true);
    m_reconstructionEvidenceLabel->setStyleSheet(QStringLiteral("color: #bcc5d0;"));
    layout->addWidget(tag, 0, Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(views, 1);
    layout->addWidget(m_reconstructionEvidenceLabel);
    return frame;
}

class SequenceTimelineView final : public QWidget {
public:
    explicit SequenceTimelineView(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("SequenceTimelineView"));
        setMinimumHeight(126);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(QStringLiteral("#10151c")));
        painter.setRenderHint(QPainter::Antialiasing);
        const QStringList tracks = {QStringLiteral("RF"), QStringLiteral("Gx"), QStringLiteral("Gy"), QStringLiteral("Gz"), QStringLiteral("ADC")};
        const int labelWidth = 34;
        const int top = 16;
        const int spacing = qMax(18, (height() - 28) / tracks.size());
        const int x0 = labelWidth + 10;
        const int x1 = width() - 14;
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
        for (int index = 0; index < tracks.size(); ++index) {
            const int y = top + index * spacing;
            painter.setPen(QColor(QStringLiteral("#9ba8b8")));
            painter.drawText(2, y + 4, tracks.at(index));
            painter.setPen(QPen(QColor(QStringLiteral("#4b596b")), 1));
            painter.drawLine(x0, y, x1, y);
        }
        const int pulseX = x0 + (x1 - x0) / 7;
        const int echoX = x0 + (x1 - x0) * 3 / 5;
        const int readoutX = echoX + 18;
        const int rfY = top;
        const int gxY = top + spacing;
        const int gyY = top + spacing * 2;
        const int gzY = top + spacing * 3;
        const int adcY = top + spacing * 4;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#e9a84a")));
        painter.drawRect(pulseX, rfY - 9, 10, 18);
        painter.drawRect(echoX, rfY - 7, 8, 14);
        painter.setPen(QPen(QColor(QStringLiteral("#62c1d8")), 2));
        painter.drawLine(pulseX - 10, gxY, pulseX, gxY - 8);
        painter.drawLine(pulseX, gxY - 8, pulseX + 12, gxY);
        painter.drawLine(readoutX, gxY, readoutX + 12, gxY - 10);
        painter.drawLine(readoutX + 12, gxY - 10, readoutX + 74, gxY - 10);
        painter.drawLine(readoutX + 74, gxY - 10, readoutX + 88, gxY);
        painter.setPen(QPen(QColor(QStringLiteral("#7bc47f")), 2));
        painter.drawLine(pulseX + 18, gyY, pulseX + 24, gyY - 8);
        painter.drawLine(pulseX + 24, gyY - 8, pulseX + 30, gyY);
        painter.drawLine(echoX - 16, gyY, echoX - 8, gyY + 7);
        painter.drawLine(echoX - 8, gyY + 7, echoX, gyY);
        painter.setPen(QPen(QColor(QStringLiteral("#b98ae3")), 2));
        painter.drawLine(pulseX - 6, gzY, pulseX + 1, gzY - 10);
        painter.drawLine(pulseX + 1, gzY - 10, pulseX + 12, gzY);
        painter.drawLine(echoX - 14, gzY, echoX - 7, gzY + 7);
        painter.drawLine(echoX - 7, gzY + 7, echoX, gzY);
        painter.setPen(QPen(QColor(QStringLiteral("#f06e6e")), 2, Qt::DashLine));
        painter.drawRect(readoutX + 14, adcY - 8, 58, 16);
        painter.setPen(QColor(QStringLiteral("#9ba8b8")));
        painter.drawText(readoutX + 16, adcY + 4, QStringLiteral("采集窗口"));
    }
};

static QWidget* createSequenceTimeline(QWidget* parent)
{
    return new SequenceTimelineView(parent);
}

class LocalizationCoverageView final : public QWidget {
public:
    explicit LocalizationCoverageView(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("LocalizationCoverageDiagram"));
        setMinimumHeight(74);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(QStringLiteral("#10151c")));
        painter.setRenderHint(QPainter::Antialiasing);

        const QRect fovRect(42, 12, qMax(120, width() - 84), qMax(42, height() - 24));
        painter.setPen(QPen(QColor(QStringLiteral("#62c1d8")), 1));
        painter.drawRect(fovRect);
        painter.setPen(QPen(QColor(QStringLiteral("#4b596b")), 1, Qt::DashLine));
        painter.drawLine(fovRect.center().x(), fovRect.top(), fovRect.center().x(), fovRect.bottom());
        painter.drawLine(fovRect.left(), fovRect.center().y(), fovRect.right(), fovRect.center().y());

        painter.setPen(QPen(QColor(QStringLiteral("#b98ae3")), 1));
        for (int slice = 1; slice < 9; ++slice) {
            const int y = fovRect.top() + slice * fovRect.height() / 9;
            painter.drawLine(fovRect.left() + 4, y, fovRect.right() - 4, y);
        }
        painter.setPen(QColor(QStringLiteral("#9ba8b8")));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
        painter.drawText(4, fovRect.center().y() + 4, QStringLiteral("FOV"));
        painter.drawText(fovRect.right() + 6, fovRect.center().y() + 4, QStringLiteral("9 层"));
    }
};

static QWidget* createLocalizationCoverageDiagram(QWidget* parent)
{
    return new LocalizationCoverageView(parent);
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
    appendLog(QStringLiteral("真实设备连接保持 HOLD；当前界面仅允许 Mock 工作流。"));
    return;
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
    appendLog(QStringLiteral("真实 SDK 加载保持 HOLD；当前界面不打开文件选择器。"));
    return;
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
    appendLog(QStringLiteral("真实预检保持 HOLD；第 04 步仅显示 Mock 待确认状态。"));
    return;
    m_precheckRequested = true;
    m_bridge->precheck();
}

void MainWindow::handleDryRun()
{
    appendLog(QStringLiteral("Mock 参数快照已保留；未写入 SDK。"));
}

void MainWindow::handleStart()
{
    appendLog(QStringLiteral("真实 Run 永久 HOLD；请使用第 08 步 Mock 采集。"));
}

void MainWindow::handlePause()
{
    appendLog(QStringLiteral("Mock 工作流不调用真实暂停。"));
}

void MainWindow::handleResume()
{
    appendLog(QStringLiteral("Mock 工作流不调用真实继续。"));
}

void MainWindow::handleAbort()
{
    appendLog(QStringLiteral("真实 Abort 永久 HOLD；当前无设备操作。"));
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
    Q_UNUSED(state);
    if (m_loadSdkButton) m_loadSdkButton->setEnabled(false);
    if (m_connectButton) m_connectButton->setEnabled(false);
    if (m_startButton) m_startButton->setEnabled(false);
    if (m_pauseButton) m_pauseButton->setEnabled(false);
    if (m_abortButton) m_abortButton->setEnabled(false);
}

void MainWindow::updatePrecheckStatus(const MriSdkStatus& status)
{
    if (!m_precheckRequested || !m_precheckStatusLabel) {
        return;
    }
    m_precheckStatusLabel->setText(
        QStringLiteral("真实预检：连接码 %1；温度 %2 C；ScanStatus %3")
            .arg(status.connection)
            .arg(status.temperature, 0, 'f', 1)
            .arg(status.scan));
    if (m_precheckDeviceStatus) {
        m_precheckDeviceStatus->setText(
            QStringLiteral("实际返回 / 连接码 %1").arg(status.connection));
    }
}

void MainWindow::updateControlMode()
{
    if (m_controlModeCombo) m_controlModeCombo->setEnabled(false);
    if (m_loadSdkButton) m_loadSdkButton->setEnabled(false);
    if (m_connectButton) m_connectButton->setEnabled(false);
    if (m_startButton) m_startButton->setEnabled(false);
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
    if (m_reconstructionEvidenceLabel) {
        m_reconstructionEvidenceLabel->setText(
            QStringLiteral("来源：自动化入口返回任务 %1；关联由入口声明，未额外判定同次")
                .arg(artifacts.taskId));
    }

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
    if (m_sequenceProtocolSummary) {
        m_sequenceProtocolSummary->setText(QStringLiteral("已选：PTScan 基线 / ") + scene.sequence);
    }
    if (m_sequenceTimingSummary) {
        m_sequenceTimingSummary->setText(scene.parameterDetails);
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
            QStringLiteral("获取图像/协议：%1\n准备与预检：%2\n定位与采集：%3\n处理与重建：%4")
                .arg(scene.acquisitionProtocol, scene.preparation, scene.positioning, scene.reconstruction));
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
    const QString values[4] = {
        scene.acquisitionProtocol,
        scene.preparation,
        scene.positioning,
        scene.reconstruction
    };
    for (int i = 0; i < 4; ++i) {
        if (m_operationDetails[i]) {
            m_operationDetails[i]->setText(values[i]);
        }
    }
}
