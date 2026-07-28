#include "MainWindow.h"

#include "DeviceActionAvailability.h"
#include "ImageQualityEvaluator.h"

#include <QCryptographicHash>
#include <QButtonGroup>
#include <QCheckBox>
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
#include <QRadioButton>
#include <QRegularExpression>
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
#include <QTimer>
#include <QValidator>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
enum class ProtocolFieldKind {
    Fov,
    Matrix,
    SliceThickness,
    SliceGap,
    Nex
};

bool parseDecimalPair(const QString& text, double& first, double& second, bool requireMillimetres)
{
    const QString suffix = requireMillimetres ? QStringLiteral("\\s*mm") : QString();
    const QRegularExpression expression(
        QStringLiteral("^\\s*(\\d+(?:\\.\\d+)?)\\s*[×xX]\\s*"
                       "(\\d+(?:\\.\\d+)?)%1\\s*$").arg(suffix));
    const QRegularExpressionMatch match = expression.match(text);
    if (!match.hasMatch()) {
        return false;
    }
    first = match.captured(1).toDouble();
    second = match.captured(2).toDouble();
    return true;
}

bool parseDecimalMillimetres(const QString& text, double& value)
{
    const QRegularExpression expression(
        QStringLiteral("^\\s*(\\d+(?:\\.\\d+)?)\\s*mm\\s*$"));
    const QRegularExpressionMatch match = expression.match(text);
    if (!match.hasMatch()) {
        return false;
    }
    value = match.captured(1).toDouble();
    return true;
}

class ProtocolFieldValidator final : public QValidator {
public:
    explicit ProtocolFieldValidator(ProtocolFieldKind kind, QObject* parent = nullptr)
        : QValidator(parent), m_kind(kind)
    {
    }

    State validate(QString& input, int&) const override
    {
        if (input.trimmed().isEmpty()) {
            return Intermediate;
        }
        switch (m_kind) {
        case ProtocolFieldKind::Fov: {
            double width = 0.0;
            double height = 0.0;
            return parseDecimalPair(input, width, height, true)
                    && width >= 10.0 && width <= 100.0
                    && height >= 10.0 && height <= 100.0
                ? Acceptable : Invalid;
        }
        case ProtocolFieldKind::Matrix: {
            double width = 0.0;
            double height = 0.0;
            return parseDecimalPair(input, width, height, false)
                    && std::floor(width) == width && std::floor(height) == height
                    && width >= 16.0 && width <= 512.0
                    && height >= 16.0 && height <= 512.0
                ? Acceptable : Invalid;
        }
        case ProtocolFieldKind::SliceThickness: {
            double value = 0.0;
            return parseDecimalMillimetres(input, value)
                    && value >= 0.1 && value <= 20.0
                ? Acceptable : Invalid;
        }
        case ProtocolFieldKind::SliceGap: {
            double value = 0.0;
            return parseDecimalMillimetres(input, value)
                    && value >= 0.0 && value <= 20.0
                ? Acceptable : Invalid;
        }
        case ProtocolFieldKind::Nex: {
            bool ok = false;
            const int value = input.trimmed().toInt(&ok);
            return ok && value >= 1 && value <= 16 ? Acceptable : Invalid;
        }
        }
        return Invalid;
    }

private:
    ProtocolFieldKind m_kind;
};
}

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
    resize(1570, 953);
    setMinimumSize(1280, 760);

    m_mockAcquisitionTimer = new QTimer(this);
    m_mockAcquisitionTimer->setSingleShot(true);
    connect(m_mockAcquisitionTimer, &QTimer::timeout, this, [this] {
        m_mockAcquisitionRemainingMs = 3200;
        if (m_workflowStep == 9 && m_mockRunActive) {
            m_mockRunActive = false;
            m_mockExecutionCompleted = true;
            setWorkflowStep(10);
        }
    });

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    auto* splitter = new QSplitter(Qt::Horizontal, root);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(buildLeftPane());
    splitter->addWidget(buildCenterPane());
    splitter->addWidget(buildRightPane());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({330, 880, 330});
    rootLayout->addWidget(splitter, 1);
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
    {
        const QSignalBlocker primaryBlocker(m_primarySceneCombo);
        const QSignalBlocker targetBlocker(m_targetCombo);
        const QSignalBlocker templateBlocker(m_sceneList);
        m_primarySceneCombo->setCurrentIndex(-1);
        m_targetCombo->setCurrentIndex(-1);
        m_sceneList->clear();
        auto* emptyRecommendation = new QListWidgetItem(
            QStringLiteral("选择场景与检测对象后生成推荐模板"), m_sceneList);
        emptyRecommendation->setFlags(Qt::NoItemFlags);
        emptyRecommendation->setTextAlignment(Qt::AlignCenter);
        emptyRecommendation->setSizeHint(QSize(0, 104));
        emptyRecommendation->setData(Qt::UserRole, -1);
        if (m_useSelectedTemplateButton) {
            m_useSelectedTemplateButton->setEnabled(false);
        }
    }
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
        setProperty("selectedOrientation", QStringLiteral("横断"));
    }

    void swapReadPhase()
    {
        m_axesSwapped = !m_axesSwapped;
        setProperty("readPhaseSwapped", m_axesSwapped);
        update();
    }

    void setOrientation(const QString& orientation)
    {
        m_orientation = orientation;
        setProperty("selectedOrientation", orientation);
        update();
    }

    void autoPlan()
    {
        m_boxX = 0.12;
        m_boxY = 0.10;
        m_boxWidth = 0.76;
        m_boxHeight = 0.76;
        m_centerX = 0.50;
        m_centerY = 0.50;
        m_slice = 0.50;
        setProperty("planningCoverageModified", true);
        update();
    }

    void resetPlanning()
    {
        m_boxX = 0.16;
        m_boxY = 0.20;
        m_boxWidth = 0.66;
        m_boxHeight = 0.56;
        m_centerX = 0.50;
        m_centerY = 0.50;
        m_slice = 0.50;
        m_orientation = QStringLiteral("横断");
        m_axesSwapped = false;
        setProperty("selectedOrientation", m_orientation);
        setProperty("readPhaseSwapped", false);
        setProperty("planningCoverageModified", false);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(QStringLiteral("#05070a")));
        const QPixmap reference(QStringLiteral(":/mock-localization.png"));
        if (!reference.isNull()) {
            const QSize target = reference.size().scaled(size() - QSize(12, 12), Qt::KeepAspectRatio);
            const QRect targetRect((width() - target.width()) / 2, (height() - target.height()) / 2,
                                   target.width(), target.height());
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.drawPixmap(targetRect, reference);
            {
                const QRectF coverage(targetRect.left() + targetRect.width() * m_boxX,
                                      targetRect.top() + targetRect.height() * m_boxY,
                                      targetRect.width() * m_boxWidth,
                                      targetRect.height() * m_boxHeight);
                painter.setPen(QPen(QColor(QStringLiteral("#00cfd1")), 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(coverage);
                const QPointF center(coverage.left() + coverage.width() * m_centerX,
                                     coverage.top() + coverage.height() * m_centerY);
                painter.drawLine(QPointF(coverage.left(), center.y()), QPointF(coverage.right(), center.y()));
                painter.drawLine(QPointF(center.x(), coverage.top()), QPointF(center.x(), coverage.bottom()));
                painter.setPen(QPen(QColor(QStringLiteral("#168cff")), 1, Qt::DashLine));
                for (int line = 1; line < 8; ++line) {
                    const qreal y = coverage.top() + coverage.height() * line / 8.0;
                    painter.drawLine(QPointF(coverage.left(), y), QPointF(coverage.right(), y));
                }
            }
            if (m_orientation != QStringLiteral("横断") || m_axesSwapped) {
                const QString axes = m_axesSwapped
                    ? QStringLiteral("Phase / Read")
                    : QStringLiteral("Read / Phase");
                const QString caption = QStringLiteral("Mock 当前方位：%1　%2")
                                            .arg(m_orientation, axes);
                painter.setPen(QColor(QStringLiteral("#eaf3ff")));
                painter.setBrush(QColor(5, 18, 35, 205));
                const QRect badge(targetRect.left() + 14, targetRect.top() + 14, 230, 30);
                painter.drawRoundedRect(badge, 4, 4);
                painter.drawText(badge.adjusted(10, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, caption);
            }
            return;
        }
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
    QString m_orientation = QStringLiteral("横断");
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

class ReferenceImageView final : public QWidget {
public:
    ReferenceImageView(const QString& resourcePath, const QString& objectName, QWidget* parent = nullptr,
                       int minimumHeight = 260)
        : QWidget(parent), m_pixmap(resourcePath)
    {
        setObjectName(objectName);
        setMinimumHeight(minimumHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(QStringLiteral("#07090c")));
        if (m_pixmap.isNull()) {
            painter.setPen(Qt::white);
            painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Mock 图像资产不可用"));
            return;
        }
        const QSize target = m_pixmap.size().scaled(size() - QSize(16, 16), Qt::KeepAspectRatio);
        const QRect destination((width() - target.width()) / 2, (height() - target.height()) / 2,
                                target.width(), target.height());
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawPixmap(destination, m_pixmap);
    }

private:
    QPixmap m_pixmap;
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
    auto* outer = new QWidget;
    outer->setObjectName(QStringLiteral("LeftColumn"));
    outer->setMinimumWidth(290);
    outer->setMaximumWidth(335);
    auto* outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addSpacing(92);

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
    m_sceneList->setMinimumHeight(116);
    m_sceneList->setMaximumHeight(150);
    m_sceneList->setSpacing(8);
    m_sceneList->setWordWrap(true);
    m_sceneList->setUniformItemSizes(false);
    layout->addWidget(m_sceneList);

    m_useSelectedTemplateButton =
        new QPushButton(QStringLiteral("使用所选模板 → 确认任务"), frame);
    m_useSelectedTemplateButton->setObjectName(QStringLiteral("UseSelectedTemplateButton"));
    m_useSelectedTemplateButton->setProperty("class", "primary");
    m_useSelectedTemplateButton->setEnabled(false);
    m_useSelectedTemplateButton->setMinimumHeight(38);
    layout->addWidget(m_useSelectedTemplateButton);

    auto* modePanel = makePanel("SelectorFilterPanel");
    auto* modeLayout = new QGridLayout(modePanel);
    modeLayout->setContentsMargins(12, 10, 12, 10);
    auto* modeLabel = new QLabel(QStringLiteral("控制方式"), modePanel);
    modeLabel->setObjectName("MutedLabel");
    m_controlModeCombo = new QComboBox(modePanel);
    m_controlModeCombo->setObjectName("ControlModeCombo");
    m_controlModeCombo->addItem(QStringLiteral("自动化基线（Mock）"), QStringLiteral("mock"));
    m_controlModeCombo->setEnabled(false);
    m_controlModeCombo->setToolTip(QStringLiteral("v0.1 只允许 MOCK 数据源"));
    m_controlModeCombo->setAccessibleDescription(
        QStringLiteral("v0.1 只允许 MOCK；HistoricalRaw 与 Live 均为 BLOCKED"));
    m_automationStatusLabel =
        new QLabel(QStringLiteral("等待现场确认 · 未通过真实预检"), modePanel);
    m_automationStatusLabel->setObjectName("AutomationStatusLabel");
    m_automationStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    modeLayout->addWidget(modeLabel, 0, 0);
    modeLayout->addWidget(m_controlModeCombo, 0, 1);
    modeLayout->addWidget(m_automationStatusLabel, 1, 0, 1, 2);
    layout->addWidget(modePanel);

    auto* buttons = new QGridLayout;
    buttons->setHorizontalSpacing(10);
    buttons->setVerticalSpacing(10);

    m_loadSdkButton = new QPushButton(QStringLiteral("加载 SDK（本轮禁用）"), frame);
    m_loadSdkButton->setObjectName(QStringLiteral("LoadSdkButton"));
    m_loadSdkButton->setProperty("class", "secondary");
    m_loadSdkButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    m_connectButton = new QPushButton(QStringLiteral("一键建链（本轮禁用）"), frame);
    m_connectButton->setObjectName(QStringLiteral("ConnectDeviceButton"));
    m_connectButton->setProperty("class", "primary");
    m_connectButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    auto* precheckBtn = new QPushButton(QStringLiteral("真实预检（未执行）"), frame);
    precheckBtn->setObjectName(QStringLiteral("RealPrecheckButton"));
    precheckBtn->setProperty("class", "secondary");
    auto* dryRunBtn = new QPushButton(QStringLiteral("DRY_RUN"), frame);
    dryRunBtn->setObjectName(QStringLiteral("DryRunButton"));
    dryRunBtn->setProperty("class", "secondary");
    m_leftMockStartButton = new QPushButton(QStringLiteral("开始采集（Mock）"), frame);
    m_leftMockStartButton->setObjectName(QStringLiteral("LeftMockStartButton"));
    m_leftMockStartButton->setProperty("class", "success");
    m_leftMockStartButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_startButton = new QPushButton(QStringLiteral("真实 Run（等待现场确认）"), frame);
    m_startButton->setObjectName("RealRunButton");
    m_startButton->setProperty("class", "secondary");
    m_pauseButton = new QPushButton(QStringLiteral("暂停（Mock）"), frame);
    m_pauseButton->setObjectName(QStringLiteral("MockPauseButton"));
    m_pauseButton->setProperty("class", "warning");
    m_pauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    m_pauseButton->setToolTip(QStringLiteral("当前 SDK 未提供暂停/继续接口"));
    m_pauseButton->setEnabled(false);
    m_abortButton = new QPushButton(QStringLiteral("真实 Abort（HOLD）"), frame);
    m_abortButton->setObjectName(QStringLiteral("RealAbortButton"));
    m_abortButton->setVisible(false);
    m_abortButton->setEnabled(false);
    m_leftMockStopButton = new QPushButton(QStringLiteral("急停（Mock-only）"), frame);
    m_leftMockStopButton->setObjectName(QStringLiteral("LeftMockStopButton"));
    m_leftMockStopButton->setProperty("class", "danger");
    m_leftMockStopButton->setIcon(style()->standardIcon(QStyle::SP_BrowserStop));

    buttons->addWidget(m_loadSdkButton, 0, 0);
    buttons->addWidget(m_connectButton, 0, 1);
    buttons->addWidget(precheckBtn, 1, 0);
    buttons->addWidget(dryRunBtn, 1, 1);
    buttons->addWidget(m_leftMockStartButton, 2, 0);
    buttons->addWidget(m_pauseButton, 2, 1);
    buttons->addWidget(m_startButton, 3, 0, 1, 2);
    buttons->addWidget(m_leftMockStopButton, 4, 0, 1, 2);
    layout->addLayout(buttons);

    m_loadSdkButton->setEnabled(false);
    m_connectButton->setEnabled(false);
    precheckBtn->setEnabled(false);
    m_startButton->setEnabled(false);
    m_leftMockStopButton->setEnabled(false);
    const QString liveBlockedReason =
        QStringLiteral("LIVE: BLOCKED；缺少 SDK 身份、设备 IDLE、横断位映射、"
                       "冻结快照、隔离输出、唯一 Run owner 和单次人工授权");
    for (QPushButton* button : {m_loadSdkButton, m_connectButton, precheckBtn, m_startButton}) {
        button->setToolTip(liveBlockedReason);
        button->setAccessibleDescription(liveBlockedReason);
    }

    layout->addStretch();

    connect(m_primarySceneCombo, &QComboBox::currentIndexChanged, this, &MainWindow::handlePrimarySceneChanged);
    connect(m_targetCombo, &QComboBox::currentIndexChanged, this, &MainWindow::handleTargetChanged);
    connect(m_templateSearchEdit, &QLineEdit::textChanged, this, &MainWindow::handleTemplateSearchChanged);
    connect(m_sceneList, &QListWidget::currentRowChanged, this, &MainWindow::handleSceneChanged);
    connect(m_useSelectedTemplateButton, &QPushButton::clicked, this, [this] {
        if (!m_sceneList || !m_sceneList->currentItem()
            || m_sceneList->currentItem()->data(Qt::UserRole).toInt() != 0) {
            return;
        }
        applyScene(currentScene());
        setWorkflowStep(3);
    });
    connect(m_controlModeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::updateControlMode);
    connect(m_loadSdkButton, &QPushButton::clicked, this, &MainWindow::handleLoadSdk);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::handleConnect);
    connect(precheckBtn, &QPushButton::clicked, this, &MainWindow::handlePrecheck);
    connect(dryRunBtn, &QPushButton::clicked, this, &MainWindow::handleDryRun);
    connect(m_leftMockStartButton, &QPushButton::clicked, this, [this] {
        if (m_workflowStep == 8 && m_mockAcquireButton
            && m_mockAcquireButton->isEnabled()) {
            m_mockAcquireButton->click();
        }
    });
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::handleStart);
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::handlePause);
    connect(m_abortButton, &QPushButton::clicked, this, &MainWindow::handleAbort);
    connect(m_leftMockStopButton, &QPushButton::clicked, this, [this] {
        if (!m_mockRunActive) return;
        m_mockRunActive = false;
        m_mockExecutionCompleted = false;
        setWorkflowStep(8);
    });

    outerLayout->addWidget(frame, 1);
    return outer;
}

QWidget* MainWindow::buildCenterPane()
{
    auto* frame = new QWidget;
    frame->setObjectName(QStringLiteral("CenterColumn"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 28, 0, 0);
    layout->setSpacing(10);

    auto* statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(0, 0, 0, 0);
    m_workflowStatusLabel = new QLabel(frame);
    m_workflowStatusLabel->setObjectName(QStringLiteral("WorkflowStatusStrip"));
    m_workflowStatusLabel->setProperty("class", "workflowStatus");
    m_workflowStatusLabel->setWordWrap(false);
    m_workflowStatusLabel->setAlignment(Qt::AlignCenter);
    m_workflowStatusLabel->setMinimumWidth(700);
    m_workflowStatusLabel->setMaximumWidth(820);
    m_workflowStatusLabel->setFixedHeight(48);
    m_workflowCurrentStepLabel = new QLabel(frame);
    m_workflowCurrentStepLabel->setObjectName(QStringLiteral("WorkflowCurrentStep"));
    m_workflowCurrentStepLabel->setProperty("class", "workflowStepChip");
    m_workflowCurrentStepLabel->setAlignment(Qt::AlignCenter);
    m_workflowCurrentStepLabel->setFixedWidth(38);
    m_workflowCurrentStepLabel->setVisible(false);
    statusRow->addStretch(1);
    statusRow->addWidget(m_workflowStatusLabel);
    statusRow->addWidget(m_workflowCurrentStepLabel);
    statusRow->addStretch(1);
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
    m_workflowBackButton->setVisible(false);
    m_workflowBackButton->setMinimumWidth(170);
    m_workflowBackButton->setMinimumHeight(38);
    m_workflowNextButton = new QPushButton(QStringLiteral("下一步"), frame);
    m_workflowNextButton->setObjectName(QStringLiteral("WorkflowNextButton"));
    m_workflowNextButton->setProperty("class", "primary");
    m_workflowNextButton->setVisible(false);
    m_workflowNextButton->setMinimumWidth(230);
    m_workflowNextButton->setMinimumHeight(38);
    navRow->addWidget(m_workflowBackButton);
    navRow->addStretch();
    navRow->addWidget(m_workflowNextButton);
    layout->addLayout(navRow);

    connect(m_workflowBackButton, &QPushButton::clicked, this, [this] {
        const int targetStep =
            m_workflowStep == 13 ? 12
            : m_workflowStep == 10 ? 8
            : m_workflowStep == 11 ? 7
                                   : qMax(1, m_workflowStep - 1);
        setWorkflowStep(targetStep);
    });
    connect(m_workflowNextButton, &QPushButton::clicked, this, [this] {
        const QStringList canonicalActions = {
            QStringLiteral("BeginResearchButton"),
            QStringLiteral("ShowRecommendedTemplateButton"),
            QStringLiteral("AcceptTemplateButton"),
            QStringLiteral("SavePreparationButton"),
            QStringLiteral("ContinueProtocolButton"),
            QStringLiteral("OpenLocalizationPlanningButton"),
            QStringLiteral("ConfirmLocalizationButton"),
            QStringLiteral("MockAcquireButton"),
            QString(),
            QStringLiteral("CompleteMockProcessingButton"),
            QStringLiteral("ConfirmResultButton"),
            QStringLiteral("OpenHistoryButton")
        };
        if (m_workflowStep < 1 || m_workflowStep > canonicalActions.size()) return;
        const QString objectName = canonicalActions.at(m_workflowStep - 1);
        if (objectName.isEmpty()) return;
        if (auto* canonical = findChild<QPushButton*>(objectName);
            canonical && canonical->isEnabled()) {
            canonical->click();
        }
    });
    refreshWorkflow();

    return frame;
}

static QFrame* makeGalleryCard(const QString& title, const QString& detail, QWidget* parent = nullptr)
{
    auto* card = makePanel(QStringLiteral("WorkflowCard"));
    if (parent) card->setParent(parent);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(6);
    auto* titleLabel = new QLabel(title, card);
    titleLabel->setProperty("class", "workflowCardTitle");
    auto* detailLabel = new QLabel(detail, card);
    detailLabel->setProperty("class", "workflowCardDetail");
    detailLabel->setWordWrap(true);
    detailLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(titleLabel);
    layout->addWidget(detailLabel);
    card->setMinimumHeight(64);
    return card;
}

static QFrame* makeGalleryThumbnail(const QString& resource, const QString& objectName,
                                    const QString& caption, bool selected, QWidget* parent)
{
    auto* thumbnail = makePanel(QStringLiteral("WorkflowCard"));
    thumbnail->setParent(parent);
    thumbnail->setObjectName(objectName);
    thumbnail->setProperty("selected", selected);
    thumbnail->setFixedWidth(104);
    auto* thumbnailLayout = new QVBoxLayout(thumbnail);
    thumbnailLayout->setContentsMargins(5, 5, 5, 5);
    thumbnailLayout->setSpacing(3);
    auto* imageLayer = new QWidget(thumbnail);
    auto* imageLayerLayout = new QGridLayout(imageLayer);
    imageLayerLayout->setContentsMargins(0, 0, 0, 0);
    auto* image = new ReferenceImageView(
        resource, QStringLiteral("%1Image").arg(objectName), imageLayer, 70);
    imageLayerLayout->addWidget(image, 0, 0);
    QString badgeText;
    QString badgeClass;
    if (caption.contains(QStringLiteral("完成"))) {
        badgeText = QStringLiteral("✓");
        badgeClass = QStringLiteral("galleryBadgeSuccess");
    } else if (caption.contains(QStringLiteral("68%"))) {
        badgeText = QStringLiteral("68%");
        badgeClass = QStringLiteral("galleryBadgeProgress");
    } else if (caption.contains(QStringLiteral("等待"))) {
        badgeText = QStringLiteral("待");
        badgeClass = QStringLiteral("galleryBadgeWaiting");
    }
    if (!badgeText.isEmpty()) {
        auto* badge = new QLabel(badgeText, imageLayer);
        badge->setProperty("class", badgeClass);
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(badgeText == QStringLiteral("68%") ? QSize(42, 42) : QSize(32, 32));
        imageLayerLayout->addWidget(badge, 0, 0, Qt::AlignRight | Qt::AlignBottom);
    }
    thumbnailLayout->addWidget(imageLayer, 1);
    auto* label = new QLabel(caption, thumbnail);
    label->setAlignment(Qt::AlignCenter);
    label->setProperty("class", "evidenceLabel");
    thumbnailLayout->addWidget(label);
    return thumbnail;
}

static QLabel* makeGallerySectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("class", "workflowSectionTitle");
    label->setWordWrap(true);
    return label;
}

QWidget* MainWindow::makeWorkflowPage(int step)
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("WorkflowPage%1").arg(step, 2, 10, QLatin1Char('0')));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(10);

    const auto addTitle = [page, layout](const QString& text) {
        auto* title = makeGallerySectionTitle(text, page);
        title->setObjectName(QStringLiteral("WorkflowBodyLabel"));
        layout->addWidget(title);
        return title;
    };
    const auto addImageEvidence = [page, layout](const QString& resource, const QString& objectName,
                                                 const QString& evidence) {
        auto* image = new ReferenceImageView(resource, objectName, page);
        layout->addWidget(image, 1);
        auto* note = new QLabel(evidence, page);
        note->setObjectName(QStringLiteral("MockImageEvidenceLabel"));
        note->setProperty("class", "evidenceLabel");
        note->setWordWrap(true);
        layout->addWidget(note);
        return image;
    };

    switch (step) {
    case 1: {
        layout->addSpacing(165);
        auto* hero = new QLabel(QStringLiteral("开始一次水模横断位扫描"), page);
        hero->setProperty("class", "workflowHero");
        hero->setAlignment(Qt::AlignCenter);
        layout->addWidget(hero);
        auto* copy = new QLabel(
            QStringLiteral("以当前水模为对象，按横断位 LOC → 单次 PTScan 采集 → 原程序既有重建与结果显示推进。"),
            page);
        copy->setProperty("class", "workflowLead");
        copy->setAlignment(Qt::AlignCenter);
        copy->setWordWrap(true);
        layout->addWidget(copy);
        auto* begin = new QPushButton(QStringLiteral("开始选择任务"), page);
        begin->setObjectName(QStringLiteral("BeginResearchButton"));
        begin->setProperty("class", "primary");
        begin->setMinimumWidth(190);
        connect(begin, &QPushButton::clicked, this, [this] { setWorkflowStep(2); });
        layout->addWidget(begin, 0, Qt::AlignHCenter);
        layout->addSpacing(150);
        auto* readiness = makePanel(QStringLiteral("WorkflowCard"));
        readiness->setObjectName(QStringLiteral("EntryReadinessSummary"));
        readiness->setMinimumHeight(210);
        auto* readinessLayout = new QGridLayout(readiness);
        readinessLayout->setContentsMargins(18, 14, 18, 14);
        readinessLayout->setHorizontalSpacing(34);
        readinessLayout->setVerticalSpacing(12);
        const QStringList readinessNames = {
            QStringLiteral("SDK"), QStringLiteral("设备连接"),
            QStringLiteral("存储空间"), QStringLiteral("真实 Run")
        };
        const QStringList readinessValues = {
            QStringLiteral("未加载"), QStringLiteral("未连接"),
            QStringLiteral("可用 · Mock"), QStringLiteral("HOLD")
        };
        for (int row = 0; row < readinessNames.size(); ++row) {
            auto* name = new QLabel(readinessNames.at(row), readiness);
            name->setProperty("class", "workflowCardTitle");
            auto* value = new QLabel(readinessValues.at(row), readiness);
            value->setProperty("class", row == 3 ? "warningText" : "workflowCardDetail");
            readinessLayout->addWidget(name, row, 0);
            readinessLayout->addWidget(value, row, 1);
        }
        readinessLayout->setColumnStretch(1, 1);
        layout->addWidget(readiness);
        layout->addStretch();
        break;
    }
    case 2: {
        layout->addSpacing(85);
        addTitle(QStringLiteral("根据科研目标推荐任务模板"));
        auto* lead = new QLabel(
            QStringLiteral("基于当前选择的一级场景与检测对象，推荐最适合的任务模板作为默认流程。"), page);
        lead->setProperty("class", "workflowLead");
        lead->setWordWrap(true);
        layout->addWidget(lead);

        const auto makeTemplateChoice =
            [page](const QString& objectName, const QString& title, const QString& detail,
                   const QString& modules, bool selected) {
                auto* card = makePanel(QStringLiteral("WorkflowCard"));
                card->setParent(page);
                card->setObjectName(objectName);
                card->setProperty("selected", selected);
                auto* cardLayout = new QHBoxLayout(card);
                cardLayout->setContentsMargins(18, 16, 18, 16);
                cardLayout->setSpacing(14);
                auto* choice = new QRadioButton(card);
                choice->setObjectName(QStringLiteral("%1Radio").arg(objectName));
                choice->setChecked(selected);
                choice->setProperty("class", "templateChoice");
                cardLayout->addWidget(choice, 0, Qt::AlignTop);
                auto* copy = new QVBoxLayout;
                copy->setSpacing(8);
                auto* heading = new QLabel(title, card);
                heading->setProperty("class", "templateChoiceTitle");
                auto* description = new QLabel(detail, card);
                description->setProperty("class", "templateChoiceDetail");
                description->setWordWrap(true);
                auto* flow = new QLabel(modules, card);
                flow->setProperty("class", "templateChoiceFlow");
                flow->setWordWrap(true);
                copy->addWidget(heading);
                copy->addWidget(description);
                copy->addStretch();
                copy->addWidget(flow);
                cardLayout->addLayout(copy, 1);
                return qMakePair(card, choice);
            };
        auto primaryChoice = makeTemplateChoice(
            QStringLiteral("PrimaryTemplateRecommendation"),
            QStringLiteral("水模横断位成像模板"),
            QStringLiteral("水模横断位定位、单次采集、既有重建与基础图像质控\n"
                           "真实采集　·　等待现场确认"),
            QStringLiteral("采集协议　｜　准备与预检　｜　定位与采集　｜　处理与重建"),
            true);
        auto* primaryRecommendation = primaryChoice.first;
        primaryRecommendation->setObjectName(QStringLiteral("PrimaryTemplateRecommendation"));
        primaryRecommendation->setFixedHeight(178);
        layout->addWidget(primaryRecommendation);

        auto repeatChoice = makeTemplateChoice(
            QStringLiteral("RepeatTemplateRecommendation"),
            QStringLiteral("对照重复扫描"),
            QStringLiteral("按需增加，用于重复稳定性对照；不属于默认主流程。"),
            QStringLiteral("仅在用户主动添加对照时加入"),
            false);
        auto* repeatRecommendation = repeatChoice.first;
        repeatRecommendation->setObjectName(QStringLiteral("RepeatTemplateRecommendation"));
        repeatRecommendation->setFixedHeight(116);
        layout->addWidget(repeatRecommendation);
        auto* choiceGroup = new QButtonGroup(page);
        choiceGroup->setExclusive(true);
        choiceGroup->addButton(primaryChoice.second);
        choiceGroup->addButton(repeatChoice.second);
        connect(primaryChoice.second, &QRadioButton::toggled, this,
                [primaryRecommendation, repeatRecommendation](bool checked) {
                    primaryRecommendation->setProperty("selected", checked);
                    repeatRecommendation->setProperty("selected", !checked);
                    primaryRecommendation->style()->unpolish(primaryRecommendation);
                    primaryRecommendation->style()->polish(primaryRecommendation);
                    repeatRecommendation->style()->unpolish(repeatRecommendation);
                    repeatRecommendation->style()->polish(repeatRecommendation);
                });
        connect(repeatChoice.second, &QRadioButton::toggled, this,
                [this](bool checked) {
                    m_comparisonEnabled = checked;
                    refreshWorkflow();
                });
        layout->addStretch();

        auto* actions = new QHBoxLayout;
        auto* back = new QPushButton(QStringLiteral("返回"), page);
        back->setObjectName(QStringLiteral("SceneSelectionBackButton"));
        back->setProperty("class", "secondary");
        connect(back, &QPushButton::clicked, this, [this] { setWorkflowStep(1); });
        auto* generate = new QPushButton(QStringLiteral("查看推荐模板"), page);
        generate->setObjectName(QStringLiteral("ShowRecommendedTemplateButton"));
        generate->setProperty("class", "primary");
        connect(generate, &QPushButton::clicked, this, [this] {
            if (!m_sceneList || !m_sceneList->currentItem()
                || m_sceneList->currentItem()->data(Qt::UserRole).toInt() != 0) {
                return;
            }
            applyScene(currentScene());
            setWorkflowStep(3);
        });
        actions->addStretch();
        actions->addWidget(back);
        actions->addWidget(generate);
        layout->addLayout(actions);
        break;
    }
    case 3: {
        layout->addSpacing(65);
        addTitle(QStringLiteral("确认推荐任务模板"));
        auto* recommendation = makePanel(QStringLiteral("WorkflowCard"));
        recommendation->setParent(page);
        recommendation->setObjectName(QStringLiteral("TemplateDetailsCard"));
        recommendation->setProperty("class", "selectedTemplate");
        recommendation->setProperty("selected", true);
        recommendation->setMinimumHeight(365);
        auto* recommendationLayout = new QVBoxLayout(recommendation);
        recommendationLayout->setContentsMargins(18, 16, 18, 16);
        recommendationLayout->setSpacing(10);
        auto* templateTitle = new QLabel(QStringLiteral("水模横断位成像模板"), recommendation);
        templateTitle->setProperty("class", "templateChoiceTitle");
        auto* metadata = new QLabel(
            QStringLiteral("TPL-PHANTOM-AXIAL · v1.0　｜　系统模板 · 只读　｜　开发预设 · 待设备适配"),
            recommendation);
        metadata->setProperty("class", "templateMetadata");
        recommendationLayout->addWidget(templateTitle);
        recommendationLayout->addWidget(metadata);
        auto* capabilityGrid = new QGridLayout;
        capabilityGrid->setHorizontalSpacing(18);
        capabilityGrid->setVerticalSpacing(8);
        const QList<QPair<QString, QString>> capabilities = {
            {QStringLiteral("目标输出"), QStringLiteral("水模横断位图像")},
            {QStringLiteral("默认主采集"), QStringLiteral("PTScan 基线（单次）")},
            {QStringLiteral("第二组采集"), QStringLiteral("按需增加，不默认执行")},
            {QStringLiteral("重建方式"), QStringLiteral("标准重建与基础处理")},
            {QStringLiteral("质控"), QStringLiteral("SNR、均匀性、畸变/尺寸、分辨率、伪影、重复稳定性")}
        };
        for (int row = 0; row < capabilities.size(); ++row) {
            auto* name = new QLabel(capabilities.at(row).first, recommendation);
            name->setProperty("class", "capabilityName");
            auto* value = new QLabel(capabilities.at(row).second, recommendation);
            value->setProperty("class", "capabilityValue");
            value->setWordWrap(true);
            capabilityGrid->addWidget(name, row, 0, Qt::AlignTop);
            capabilityGrid->addWidget(value, row, 1);
        }
        capabilityGrid->setColumnStretch(1, 1);
        recommendationLayout->addLayout(capabilityGrid, 1);
        layout->addWidget(recommendation);
        m_protocolChainLabel =
            new QLabel(QStringLiteral("横断位 LOC → PTScan（单次基线；当前 HOLD）"), page);
        m_protocolChainLabel->setObjectName(QStringLiteral("ProtocolChainLabel"));
        m_protocolChainLabel->setProperty("class", "workflowProtocol");
        layout->addWidget(m_protocolChainLabel);
        m_addComparisonButton = new QPushButton(QStringLiteral("添加 FSE B 对照协议"), page);
        m_addComparisonButton->setObjectName(QStringLiteral("AddComparisonButton"));
        m_addComparisonButton->setProperty("class", "secondary");
        connect(m_addComparisonButton, &QPushButton::clicked, this, [this] {
            m_comparisonEnabled = true;
            refreshWorkflow();
        });
        layout->addWidget(m_addComparisonButton, 0, Qt::AlignLeft);
        auto* boundary = new QLabel(
            QStringLiteral("系统模板为只读，无法覆盖；后续仅开放白名单科研参数进行编辑。"), page);
        boundary->setProperty("class", "evidenceLabel");
        boundary->setWordWrap(true);
        layout->addWidget(boundary);
        layout->addStretch();

        auto* actions = new QHBoxLayout;
        auto* back = new QPushButton(QStringLiteral("返回推荐列表"), page);
        back->setObjectName(QStringLiteral("TemplateBackButton"));
        back->setProperty("class", "secondary");
        connect(back, &QPushButton::clicked, this, [this] { setWorkflowStep(2); });
        auto* accept = new QPushButton(QStringLiteral("采用模板并继续"), page);
        accept->setObjectName(QStringLiteral("AcceptTemplateButton"));
        accept->setProperty("class", "primary");
        connect(accept, &QPushButton::clicked, this, [this] { setWorkflowStep(4); });
        actions->addStretch();
        actions->addWidget(back);
        actions->addWidget(accept);
        layout->addLayout(actions);
        break;
    }
    case 4: {
        addTitle(QStringLiteral("样品登记与准备预检"));
        auto* sampleCard = makePanel(QStringLiteral("WorkflowCard"));
        sampleCard->setObjectName(QStringLiteral("SampleInfoCard"));
        sampleCard->setParent(page);
        sampleCard->setFixedHeight(240);
        auto* sampleLayout = new QVBoxLayout(sampleCard);
        sampleLayout->setContentsMargins(16, 14, 16, 14);
        sampleLayout->setSpacing(8);
        auto* sampleTitle = new QLabel(QStringLiteral("A. 样品信息"), sampleCard);
        sampleTitle->setProperty("class", "workflowSectionTitle");
        sampleLayout->addWidget(sampleTitle);
        auto* sampleGrid = new QGridLayout;
        sampleGrid->setHorizontalSpacing(24);
        sampleGrid->setVerticalSpacing(7);
        const QList<QPair<QString, QString>> sampleRows = {
            {QStringLiteral("样品编号"), QStringLiteral("WATER-PHANTOM-001")},
            {QStringLiteral("样品名称"), QStringLiteral("水模")},
            {QStringLiteral("样品类型"), QStringLiteral("标准水模")},
            {QStringLiteral("装样方式"), QStringLiteral("中心固定 · 需现场确认")},
            {QStringLiteral("样品温度"), QStringLiteral("待读取")},
            {QStringLiteral("备注"), QStringLiteral("目标方向：横断位")}
        };
        for (int row = 0; row < sampleRows.size(); ++row) {
            auto* name = new QLabel(sampleRows.at(row).first, sampleCard);
            name->setProperty("class", "precheckRowName");
            auto* value = new QLabel(sampleRows.at(row).second, sampleCard);
            if (row == 1) {
                value->setObjectName(QStringLiteral("SampleProfileLabel"));
            }
            value->setProperty("class", "precheckRowValue");
            sampleGrid->addWidget(name, row, 0);
            sampleGrid->addWidget(value, row, 1);
        }
        sampleGrid->setColumnStretch(1, 1);
        sampleLayout->addLayout(sampleGrid, 1);
        layout->addWidget(sampleCard);

        auto* precheckCard = makePanel(QStringLiteral("WorkflowCard"));
        precheckCard->setObjectName(QStringLiteral("PreparationPrecheckCard"));
        precheckCard->setParent(page);
        precheckCard->setFixedHeight(245);
        auto* precheckLayout = new QVBoxLayout(precheckCard);
        precheckLayout->setContentsMargins(16, 14, 16, 14);
        precheckLayout->setSpacing(8);
        auto* precheckTitle = new QLabel(QStringLiteral("B. 扫描前检查"), precheckCard);
        precheckTitle->setProperty("class", "workflowSectionTitle");
        precheckLayout->addWidget(precheckTitle);
        auto* precheckGrid = new QGridLayout;
        precheckGrid->setHorizontalSpacing(18);
        precheckGrid->setVerticalSpacing(8);
        const QList<QPair<QString, QString>> precheckRows = {
            {QStringLiteral("水模与线圈空间"), QStringLiteral("待现场确认")},
            {QStringLiteral("水模中心与固定"), QStringLiteral("待现场确认")},
            {QStringLiteral("接收线圈"), QStringLiteral("待现场确认")},
            {QStringLiteral("输出目录可写"), QStringLiteral("未执行检查")},
            {QStringLiteral("设备连接 / 温度 / ScanStatus"), QStringLiteral("未执行真实预检")}
        };
        for (int row = 0; row < precheckRows.size(); ++row) {
            auto* name = new QLabel(precheckRows.at(row).first, precheckCard);
            name->setProperty("class", "precheckWarningName");
            auto* state = new QLabel(precheckRows.at(row).second, precheckCard);
            state->setProperty("class", "precheckWarningState");
            state->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            precheckGrid->addWidget(name, row, 0);
            precheckGrid->addWidget(state, row, 1);
        }
        precheckGrid->setColumnStretch(0, 1);
        precheckLayout->addLayout(precheckGrid, 1);
        layout->addWidget(precheckCard);

        auto* preparationNote = new QLabel(
            QStringLiteral("水模需落在 φ50 mm、长度 70 mm 线圈空间内并现场确认；连接、温度、ScanStatus 与输出目录尚未真实预检。"),
            page);
        preparationNote->setProperty("class", "evidenceLabel");
        preparationNote->setWordWrap(false);
        preparationNote->setFixedHeight(42);
        layout->addWidget(preparationNote);
        auto* actions = new QHBoxLayout;
        auto* back = new QPushButton(QStringLiteral("返回模板"), page);
        back->setObjectName(QStringLiteral("PreparationBackButton"));
        back->setProperty("class", "secondary");
        connect(back, &QPushButton::clicked, this, [this] { setWorkflowStep(3); });
        auto* save = new QPushButton(QStringLiteral("确认 Mock 预设并继续"), page);
        save->setObjectName(QStringLiteral("SavePreparationButton"));
        save->setProperty("class", "primary");
        connect(save, &QPushButton::clicked, this, [this] {
            m_preparationConfirmed = true;
            if (m_automationStatusLabel) {
                m_automationStatusLabel->setText(
                    QStringLiteral("Mock 水模预设已确认 · 仅内存 · 未写文件/SDK"));
            }
            setWorkflowStep(5);
        });
        actions->addStretch();
        actions->addWidget(back);
        actions->addWidget(save);
        layout->addLayout(actions);
        break;
    }
    case 5: {
        addTitle(QStringLiteral("扫描方案与参数确认"));
        auto* planSummary = makePanel(QStringLiteral("WorkflowCard"));
        planSummary->setObjectName(QStringLiteral("ScanPlanSummaryCard"));
        planSummary->setParent(page);
        planSummary->setFixedHeight(92);
        auto* planSummaryLayout = new QGridLayout(planSummary);
        planSummaryLayout->setContentsMargins(14, 10, 14, 10);
        planSummaryLayout->setHorizontalSpacing(16);
        planSummaryLayout->setVerticalSpacing(7);
        const QList<QPair<QString, QString>> planRows = {
            {QStringLiteral("系统模板"), QStringLiteral("水模横断位成像模板 · TPL-PHANTOM-AXIAL v1.0")},
            {QStringLiteral("当前方案"), QStringLiteral("水模横断位 · PTScan 基线 · 单次")}
        };
        for (int row = 0; row < planRows.size(); ++row) {
            auto* name = new QLabel(planRows.at(row).first, planSummary);
            name->setProperty("class", "capabilityName");
            auto* value = new QLabel(planRows.at(row).second, planSummary);
            value->setProperty("class", "capabilityValue");
            planSummaryLayout->addWidget(name, row, 0);
            planSummaryLayout->addWidget(value, row, 1);
        }
        planSummaryLayout->setColumnStretch(1, 1);
        layout->addWidget(planSummary);

        m_scanPlanChainLabel =
            new QLabel(QStringLiteral("协议链　横断位 LOC → PTScan（单次基线）"), page);
        m_scanPlanChainLabel->setProperty("class", "workflowProtocol");
        m_scanPlanChainLabel->setFixedHeight(48);
        layout->addWidget(m_scanPlanChainLabel);

        auto* level2Title = new QLabel(QStringLiteral("科研参数（L2，默认开放）"), page);
        level2Title->setProperty("class", "workflowCardTitle");
        layout->addWidget(level2Title);

        auto* level2Row = new QHBoxLayout;
        level2Row->setSpacing(10);
        auto* table = new QTableWidget(5, 4, page);
        table->setObjectName(QStringLiteral("ProtocolLevel2Table"));
        table->setHorizontalHeaderLabels({QStringLiteral("参数"), QStringLiteral("模板值"),
                                          QStringLiteral("当前值（可编辑）"), QStringLiteral("状态")});
        const QStringList parameters = {QStringLiteral("FOV"), QStringLiteral("矩阵"),
                                        QStringLiteral("层厚"), QStringLiteral("层间距"),
                                        QStringLiteral("NEX")};
        const QStringList values = {QStringLiteral("50×50 mm"), QStringLiteral("128×128"),
                                    QStringLiteral("3.5 mm"), QStringLiteral("1.25 mm"),
                                    QStringLiteral("1")};
        const QList<ProtocolFieldKind> fieldKinds = {
            ProtocolFieldKind::Fov, ProtocolFieldKind::Matrix,
            ProtocolFieldKind::SliceThickness, ProtocolFieldKind::SliceGap,
            ProtocolFieldKind::Nex
        };
        QList<QLineEdit*> currentEditors;
        for (int row = 0; row < parameters.size(); ++row) {
            auto* name = new QTableWidgetItem(parameters.at(row));
            auto* templateValue = new QTableWidgetItem(values.at(row));
            auto* state = new QTableWidgetItem(QStringLiteral("✓ 有效"));
            name->setFlags(name->flags() & ~Qt::ItemIsEditable);
            templateValue->setFlags(templateValue->flags() & ~Qt::ItemIsEditable);
            state->setFlags(state->flags() & ~Qt::ItemIsEditable);
            table->setItem(row, 0, name);
            table->setItem(row, 1, templateValue);
            auto* editor = new QLineEdit(values.at(row), table);
            editor->setObjectName(QStringLiteral("ProtocolL2Current%1").arg(row));
            editor->setProperty("class", "tableEditor");
            editor->setClearButtonEnabled(false);
            editor->setToolTip(QStringLiteral("L2 白名单参数，可编辑；仅 Mock 方案"));
            editor->setValidator(new ProtocolFieldValidator(fieldKinds.at(row), editor));
            table->setCellWidget(row, 2, editor);
            currentEditors.append(editor);
            table->setItem(row, 3, state);
            table->setRowHeight(row, 42);
        }
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        table->setMinimumHeight(260);
        table->setMaximumHeight(260);
        level2Row->addWidget(table, 3, Qt::AlignTop);

        auto* calculationCard = makePanel(QStringLiteral("WorkflowCard"));
        calculationCard->setObjectName(QStringLiteral("ProtocolAutoResultCard"));
        auto* calculationLayout = new QVBoxLayout(calculationCard);
        calculationLayout->setContentsMargins(14, 14, 14, 14);
        calculationLayout->setSpacing(8);
        auto* calculationTitle = new QLabel(QStringLiteral("系统自动结果（实时计算）"), calculationCard);
        calculationTitle->setProperty("class", "workflowCardTitle");
        auto* calculation = new QLabel(
            QStringLiteral("实际分辨率　0.39×0.39×3.5 mm\n"
                           "层数　　　　11 层\n"
                           "覆盖范围　　51 mm\n"
                           "预计采集　　3分20秒\n"
                           "SNR 趋势　　中\n\n"
                           "由 L2 参数实时计算，不可编辑（Mock）"),
            calculationCard);
        calculation->setObjectName(QStringLiteral("ProtocolAutoResultValue"));
        calculation->setProperty("class", "workflowCardDetail");
        calculation->setWordWrap(true);
        calculationLayout->addWidget(calculationTitle);
        calculationLayout->addWidget(calculation);
        calculationLayout->addStretch();
        calculationCard->setMinimumHeight(260);
        calculationCard->setMaximumHeight(260);
        level2Row->addWidget(calculationCard, 2, Qt::AlignTop);
        layout->addLayout(level2Row);

        auto* l3Detail = new QLabel(
            QStringLiteral("TR / TE / ETL / 接收带宽（Mock 专家参数）。L4 工程与 SDK 参数保持隐藏。"),
            page);
        l3Detail->setObjectName(QStringLiteral("L3DetailsLabel"));
        l3Detail->setProperty("class", "workflowCardDetail");
        l3Detail->setWordWrap(true);
        l3Detail->setVisible(false);
        auto* showL3 =
            new QPushButton(QStringLiteral("专家参数（L3）｜仅影响当前 Mock 候选协议　展开 >"), page);
        showL3->setObjectName(QStringLiteral("ShowL3Button"));
        showL3->setProperty("class", "secondary");
        connect(showL3, &QPushButton::clicked, this, [showL3, l3Detail] {
            const bool visible = !l3Detail->isVisible();
            l3Detail->setVisible(visible);
            showL3->setText(visible
                                ? QStringLiteral("专家参数（L3）｜仅影响当前 Mock 候选协议　收起")
                                : QStringLiteral("专家参数（L3）｜仅影响当前 Mock 候选协议　展开 >"));
        });
        layout->addWidget(showL3);
        layout->addWidget(l3Detail);

        layout->addStretch();
        auto* actions = new QHBoxLayout;
        auto* useOnce = new QPushButton(QStringLiteral("仅本次使用"), page);
        useOnce->setObjectName(QStringLiteral("ProtocolUseOnceButton"));
        useOnce->setProperty("class", "secondary");
        auto* saveVersion = new QPushButton(QStringLiteral("另存为新版本"), page);
        saveVersion->setObjectName(QStringLiteral("ProtocolSaveVersionButton"));
        saveVersion->setProperty("class", "secondary");
        auto* continueButton = new QPushButton(QStringLiteral("确认方案并继续"), page);
        continueButton->setObjectName(QStringLiteral("ContinueProtocolButton"));
        continueButton->setProperty("class", "primary");
        saveVersion->setEnabled(false);
        const QString versionReason = QStringLiteral("模板持久化未纳入 v0.1");
        saveVersion->setToolTip(versionReason);
        saveVersion->setAccessibleDescription(versionReason);

        const auto updateProtocolState =
            [this, currentEditors, table, calculation, useOnce, continueButton] {
            static const QStringList errorTexts = {
                QStringLiteral("错误：FOV 需为 10–100 mm 的宽×高"),
                QStringLiteral("错误：矩阵需为 16–512 的整数宽×高"),
                QStringLiteral("错误：层厚需为 0.1–20 mm"),
                QStringLiteral("错误：层间距需为 0–20 mm"),
                QStringLiteral("错误：NEX 需为 1–16 的整数")
            };
            bool allValid = true;
            for (int row = 0; row < currentEditors.size(); ++row) {
                const bool valid = currentEditors.at(row)->hasAcceptableInput();
                allValid = allValid && valid;
                table->item(row, 3)->setText(
                    valid ? QStringLiteral("✓ 有效") : errorTexts.at(row));
            }

            if (!allValid) {
                calculation->setText(
                    QStringLiteral("无法计算：请修正 L2 参数错误。\n"
                                   "当前未生成分辨率、覆盖或采集时间结论。"));
            } else {
                double fovX = 0.0;
                double fovY = 0.0;
                double matrixX = 0.0;
                double matrixY = 0.0;
                double thickness = 0.0;
                double gap = 0.0;
                parseDecimalPair(currentEditors.at(0)->text(), fovX, fovY, true);
                parseDecimalPair(currentEditors.at(1)->text(), matrixX, matrixY, false);
                parseDecimalMillimetres(currentEditors.at(2)->text(), thickness);
                parseDecimalMillimetres(currentEditors.at(3)->text(), gap);
                const int nex = currentEditors.at(4)->text().trimmed().toInt();
                const double coverage = thickness * 11.0 + gap * 10.0;
                calculation->setText(
                    QStringLiteral("实际分辨率　%1×%2×%3 mm\n"
                                   "层数　　　　11 层（Mock 预设）\n"
                                   "覆盖范围　　%4 mm\n"
                                   "NEX　　　　 %5\n"
                                   "预计采集　　未计算（无批准公式）\n"
                                   "SNR 趋势　　未评估\n\n"
                                   "由当前 L2 输入计算；未写入 SDK。")
                        .arg(fovX / matrixX, 0, 'f', 2)
                        .arg(fovY / matrixY, 0, 'f', 2)
                        .arg(thickness, 0, 'f', 1)
                        .arg(coverage, 0, 'f', 1)
                        .arg(nex));
            }

            useOnce->setEnabled(allValid);
            useOnce->setToolTip(
                allValid ? QStringLiteral("确认当前 L2 仅用于本次 Mock")
                         : QStringLiteral("请先修正所有 L2 参数"));
            continueButton->setEnabled(allValid && m_protocolUseOnceConfirmed);
            continueButton->setToolTip(
                !allValid ? QStringLiteral("请先修正所有 L2 参数")
                : !m_protocolUseOnceConfirmed
                    ? QStringLiteral("请先点击“仅本次使用”冻结用途")
                    : QString());
            refreshWorkflow();
        };

        connect(useOnce, &QPushButton::clicked, this, [this, updateProtocolState] {
            m_protocolUseOnceConfirmed = true;
            if (m_automationStatusLabel) {
                m_automationStatusLabel->setText(
                    QStringLiteral("仅本次使用已确认 · Mock 参数草稿 · 未写入 SDK"));
            }
            updateProtocolState();
        });
        for (QLineEdit* editor : currentEditors) {
            connect(editor, &QLineEdit::textChanged, this,
                    [this, updateProtocolState](const QString&) {
                m_protocolUseOnceConfirmed = false;
                updateProtocolState();
            });
        }
        connect(continueButton, &QPushButton::clicked, this, [this] { setWorkflowStep(6); });
        actions->addWidget(useOnce);
        actions->addWidget(saveVersion);
        actions->addStretch();
        actions->addWidget(continueButton);
        layout->addLayout(actions);
        updateProtocolState();
        break;
    }
    case 6: {
        auto* locRow = new QHBoxLayout;
        locRow->setSpacing(10);
        auto* locRail = new QVBoxLayout;
        locRail->setSpacing(8);
        locRail->addWidget(makeGalleryThumbnail(QStringLiteral(":/mock-loc-axial.png"),
                                                QStringLiteral("MockLocThumbnailAxial"),
                                                QStringLiteral("横断 · Mock参考"), true, page), 1);
        locRail->addWidget(makeGalleryThumbnail(QStringLiteral(":/mock-loc-coronal.png"),
                                                QStringLiteral("MockLocThumbnailCoronal"),
                                                QStringLiteral("冠状 · Mock参考"), false, page), 1);
        locRail->addWidget(makeGalleryThumbnail(QStringLiteral(":/mock-loc-sagittal.png"),
                                                QStringLiteral("MockLocThumbnailSagittal"),
                                                QStringLiteral("矢状 · Mock参考"), false, page), 1);
        locRow->addLayout(locRail);
        auto* locReference =
            new ReferenceImageView(QStringLiteral(":/mock-localization.png"),
                                   QStringLiteral("MockLocImage"), page);
        locReference->setProperty(
            "evidenceKind", QStringLiteral("MOCK_PLANNING_REFERENCE"));
        locReference->setAccessibleDescription(
            QStringLiteral("MOCK LOC 规划参考；不包含采集状态、进度或设备输出"));
        locRow->addWidget(locReference, 1);
        layout->addLayout(locRow, 1);
        auto* evidence = new QLabel(
            QStringLiteral("Mock LOC 规划参考 · 尚未开始本次 Mock · "
                           "非设备采集图像，不关联真实 RAW"), page);
        evidence->setObjectName(QStringLiteral("MockImageEvidenceLabel"));
        evidence->setProperty("class", "evidenceLabel");
        evidence->setWordWrap(true);
        layout->addWidget(evidence);
        auto* planning = new QPushButton(QStringLiteral("进入切片规划"), page);
        planning->setObjectName(QStringLiteral("OpenLocalizationPlanningButton"));
        planning->setProperty("class", "primary");
        connect(planning, &QPushButton::clicked, this, [this] { setWorkflowStep(7); });
        layout->addWidget(planning, 0, Qt::AlignRight);
        break;
    }
    case 7: {
        auto* imagingPanel = new QWidget(page);
        imagingPanel->setObjectName(QStringLiteral("LocalizationImagingPanel"));
        imagingPanel->setMinimumHeight(520);
        imagingPanel->setMaximumHeight(570);
        auto* imagingRow = new QHBoxLayout(imagingPanel);
        imagingRow->setContentsMargins(0, 0, 0, 0);
        imagingRow->setSpacing(10);
        auto* thumbnailRail = new QVBoxLayout;
        thumbnailRail->setSpacing(8);
        const QStringList thumbnailNames = {
            QStringLiteral("LocalizationThumbnailAxial"),
            QStringLiteral("LocalizationThumbnailCoronal"),
            QStringLiteral("LocalizationThumbnailSagittal")
        };
        const QStringList thumbnailResources = {
            QStringLiteral(":/mock-loc-axial.png"),
            QStringLiteral(":/mock-loc-coronal.png"),
            QStringLiteral(":/mock-loc-sagittal.png")
        };
        const QStringList thumbnailLabels = {
            QStringLiteral("横断 · 当前"),
            QStringLiteral("冠状"),
            QStringLiteral("矢状")
        };
        for (int index = 0; index < thumbnailNames.size(); ++index) {
            auto* thumbnail = makePanel(QStringLiteral("WorkflowCard"));
            thumbnail->setObjectName(thumbnailNames.at(index));
            thumbnail->setProperty("selected", index == 0);
            thumbnail->setFixedWidth(104);
            auto* thumbnailLayout = new QVBoxLayout(thumbnail);
            thumbnailLayout->setContentsMargins(5, 5, 5, 5);
            thumbnailLayout->setSpacing(3);
            thumbnailLayout->addWidget(new ReferenceImageView(
                thumbnailResources.at(index),
                QStringLiteral("%1Image").arg(thumbnailNames.at(index)),
                thumbnail, 70), 1);
            auto* thumbnailLabel = new QLabel(thumbnailLabels.at(index), thumbnail);
            thumbnailLabel->setAlignment(Qt::AlignCenter);
            thumbnailLabel->setProperty("class", "evidenceLabel");
            thumbnailLayout->addWidget(thumbnailLabel);
            thumbnailRail->addWidget(thumbnail, 1);
        }
        imagingRow->addLayout(thumbnailRail);
        m_localizationPlanner = new LocalizationPlannerView(page);
        imagingRow->addWidget(m_localizationPlanner, 1);
        layout->addWidget(imagingPanel, 1);

        auto* actionFeedback = new QLabel(
            QStringLiteral("当前方位：横断 · Read / Phase · 推荐覆盖（Mock）"), page);
        actionFeedback->setObjectName(QStringLiteral("LocalizationActionFeedback"));
        actionFeedback->setProperty("class", "evidenceLabel");
        actionFeedback->setWordWrap(true);
        layout->addWidget(actionFeedback);

        auto* controls = new QHBoxLayout;
        const QStringList orientations = {QStringLiteral("横断"), QStringLiteral("冠状"),
                                          QStringLiteral("矢状")};
        const QStringList orientationObjectNames = {
            QStringLiteral("OrientationAxialButton"),
            QStringLiteral("OrientationCoronalButton"),
            QStringLiteral("OrientationSagittalButton")
        };
        for (int index = 0; index < orientations.size(); ++index) {
            const QString orientation = orientations.at(index);
            auto* button = new QPushButton(orientation, page);
            button->setObjectName(orientationObjectNames.at(index));
            button->setProperty("class", orientation == QStringLiteral("横断") ? "primary" : "secondary");
            connect(button, &QPushButton::clicked, this,
                    [this, page, button, orientation, index, actionFeedback] {
                static_cast<LocalizationPlannerView*>(m_localizationPlanner)->setOrientation(orientation);
                const QStringList objectNames = {
                    QStringLiteral("OrientationAxialButton"),
                    QStringLiteral("OrientationCoronalButton"),
                    QStringLiteral("OrientationSagittalButton")
                };
                for (const QString& objectName : objectNames) {
                    if (auto* candidate = page->findChild<QPushButton*>(objectName)) {
                        candidate->setProperty("class", candidate == button ? "primary" : "secondary");
                        candidate->style()->unpolish(candidate);
                        candidate->style()->polish(candidate);
                    }
                }
                const QStringList thumbnailObjectNames = {
                    QStringLiteral("LocalizationThumbnailAxial"),
                    QStringLiteral("LocalizationThumbnailCoronal"),
                    QStringLiteral("LocalizationThumbnailSagittal")
                };
                for (int thumbnailIndex = 0; thumbnailIndex < thumbnailObjectNames.size();
                     ++thumbnailIndex) {
                    if (auto* thumbnail = page->findChild<QWidget*>(
                            thumbnailObjectNames.at(thumbnailIndex))) {
                        thumbnail->setProperty("selected", thumbnailIndex == index);
                        thumbnail->style()->unpolish(thumbnail);
                        thumbnail->style()->polish(thumbnail);
                    }
                }
                const bool axial = orientation == QStringLiteral("横断");
                if (auto* confirm = page->findChild<QPushButton*>(
                        QStringLiteral("ConfirmLocalizationButton"))) {
                    confirm->setEnabled(axial);
                    confirm->setToolTip(
                        axial ? QString()
                              : QStringLiteral("v0.1 主路径要求恢复横断位后再确认"));
                }
                actionFeedback->setText(
                    axial
                        ? QStringLiteral("当前方位：横断 · Read / Phase · 可继续（Mock）")
                        : QStringLiteral("当前方位：%1 · v0.1 主路径要求横断位，暂不可继续")
                              .arg(orientation));
                refreshWorkflow();
            });
            controls->addWidget(button);
        }
        auto* swap = new QPushButton(QStringLiteral("交换 Read / Phase"), page);
        swap->setObjectName(QStringLiteral("ReadPhaseSwapButton"));
        swap->setProperty("class", "secondary");
        connect(swap, &QPushButton::clicked, this, [this, actionFeedback] {
            auto* planner = static_cast<LocalizationPlannerView*>(m_localizationPlanner);
            planner->swapReadPhase();
            actionFeedback->setText(
                planner->property("readPhaseSwapped").toBool()
                    ? QStringLiteral("当前方位：横断 · Phase / Read（已交换，Mock）")
                    : QStringLiteral("当前方位：横断 · Read / Phase（Mock）"));
        });
        controls->addWidget(swap);
        auto* autoAdjust = new QPushButton(QStringLiteral("自动调整"), page);
        autoAdjust->setObjectName(QStringLiteral("AutoPlanningButton"));
        autoAdjust->setProperty("class", "secondary");
        connect(autoAdjust, &QPushButton::clicked, this, [this, actionFeedback] {
            static_cast<LocalizationPlannerView*>(m_localizationPlanner)->autoPlan();
            actionFeedback->setText(QStringLiteral("横断位覆盖已自动调整 · Mock 规划"));
        });
        controls->addWidget(autoAdjust);
        auto* reset = new QPushButton(QStringLiteral("恢复推荐"), page);
        reset->setObjectName(QStringLiteral("ResetPlanningButton"));
        reset->setProperty("class", "secondary");
        connect(reset, &QPushButton::clicked, this, [this, page, actionFeedback] {
            static_cast<LocalizationPlannerView*>(m_localizationPlanner)->resetPlanning();
            for (const QString& objectName : {
                     QStringLiteral("OrientationAxialButton"),
                     QStringLiteral("OrientationCoronalButton"),
                     QStringLiteral("OrientationSagittalButton")}) {
                if (auto* candidate = page->findChild<QPushButton*>(objectName)) {
                    candidate->setProperty(
                        "class", objectName == QStringLiteral("OrientationAxialButton")
                                     ? "primary" : "secondary");
                    candidate->style()->unpolish(candidate);
                    candidate->style()->polish(candidate);
                }
            }
            if (auto* confirm = page->findChild<QPushButton*>(
                    QStringLiteral("ConfirmLocalizationButton"))) {
                confirm->setEnabled(true);
                confirm->setToolTip(QString());
            }
            actionFeedback->setText(
                QStringLiteral("已恢复推荐：横断 · Read / Phase · 默认覆盖（Mock）"));
            refreshWorkflow();
        });
        controls->addWidget(reset);
        auto* more = new QPushButton(QStringLiteral("更多方位"), page);
        more->setObjectName(QStringLiteral("MoreOrientationButton"));
        more->setProperty("class", "secondary");
        connect(more, &QPushButton::clicked, this, [more, actionFeedback] {
            more->setText(QStringLiteral("自定义斜切（Mock）"));
            more->setEnabled(false);
            more->setToolTip(QStringLiteral("已显示：自定义斜切仅用于 Mock 规划"));
            actionFeedback->setText(
                QStringLiteral("自定义斜切仅为 Mock 规划提示；v0.1 主路径仍要求横断位"));
        });
        controls->addWidget(more);
        controls->addStretch();
        layout->addLayout(controls);
        auto* targetRow = new QHBoxLayout;
        auto* targetLabel = new QLabel(QStringLiteral("成像目标"), page);
        targetLabel->setProperty("class", "capabilityName");
        auto* targetChoice = new QComboBox(page);
        targetChoice->setObjectName(QStringLiteral("ImagingTargetCombo"));
        targetChoice->addItems({QStringLiteral("均衡"), QStringLiteral("结构细节"),
                                QStringLiteral("覆盖优先")});
        auto* modifyTarget = new QPushButton(QStringLiteral("修改"), page);
        modifyTarget->setObjectName(QStringLiteral("ModifyImagingTargetButton"));
        modifyTarget->setProperty("class", "secondary");
        connect(modifyTarget, &QPushButton::clicked, this,
                [this, targetChoice, actionFeedback] {
            if (m_automationStatusLabel) {
                m_automationStatusLabel->setText(
                    QStringLiteral("成像目标已应用：%1 · Mock 规划")
                        .arg(targetChoice->currentText()));
            }
            actionFeedback->setText(
                QStringLiteral("成像目标已应用：%1 · Mock 规划")
                    .arg(targetChoice->currentText()));
        });
        targetRow->addWidget(targetLabel);
        targetRow->addWidget(targetChoice);
        targetRow->addWidget(modifyTarget);
        targetRow->addStretch();
        layout->addLayout(targetRow);
        auto* summary = new QLabel(
            QStringLiteral("分辨率 0.39×0.39×3.5 mm　|　11 层　|　覆盖 51 mm　|　"
                           "预计 3分20秒　|　SNR 中"),
            page);
        summary->setProperty("class", "evidenceLabel");
        layout->addWidget(summary);
        auto* planningActions = new QHBoxLayout;
        auto* researchParameters = new QPushButton(QStringLiteral("科研参数 >"), page);
        researchParameters->setObjectName(QStringLiteral("ResearchParametersButton"));
        researchParameters->setProperty("class", "secondary");
        connect(researchParameters, &QPushButton::clicked, this, [this] {
            if (m_automationStatusLabel) {
                m_automationStatusLabel->setText(
                    QStringLiteral("科研参数 L3 已展开 · 未写入 SDK"));
            }
            if (auto* l3 = findChild<QLabel*>(QStringLiteral("L3DetailsLabel"))) {
                l3->setVisible(true);
            }
            if (auto* showL3 = findChild<QPushButton*>(QStringLiteral("ShowL3Button"))) {
                showL3->setText(
                    QStringLiteral("专家参数（L3）｜仅影响当前 Mock 候选协议　收起"));
            }
            m_protocolUseOnceConfirmed = false;
            setWorkflowStep(5);
        });
        auto* confirm = new QPushButton(QStringLiteral("确认定位"), page);
        confirm->setObjectName(QStringLiteral("ConfirmLocalizationButton"));
        confirm->setProperty("class", "primary");
        connect(confirm, &QPushButton::clicked, this, [this] { setWorkflowStep(8); });
        planningActions->addStretch();
        planningActions->addWidget(researchParameters);
        planningActions->addWidget(confirm);
        layout->addLayout(planningActions);
        break;
    }
    case 8: {
        auto* runConfirmationTitle =
            addTitle(QStringLiteral("运行前确认与参数快照"));
        runConfirmationTitle->setFixedHeight(90);
        auto* confirmationTable = new QTableWidget(4, 2, page);
        confirmationTable->setObjectName(QStringLiteral("RunConfirmationTable"));
        confirmationTable->setHorizontalHeaderLabels(
            {QStringLiteral("确认项"), QStringLiteral("当前快照")});
        const QList<QPair<QString, QString>> confirmations = {
            {QStringLiteral("1　样品"),
             QStringLiteral("WATER-PHANTOM-001 · 固定 Mock 水模预设")},
            {QStringLiteral("2　任务与方案"),
             QStringLiteral("水模横断位 · PTScan Mock 基线 · 单次模拟执行")},
            {QStringLiteral("3　采集步骤"),
             QStringLiteral("LOC Mock参考 → 参数快照 → Mock执行 → Mock处理与QC")},
            {QStringLiteral("4　定位与主要参数"),
             QStringLiteral("横断位 · FOV 50×50 mm · 128×128 · 层厚 3.5 mm · "
                             "层间距 1.25 mm · 11 层 · NEX 1")}
        };
        for (int row = 0; row < confirmations.size(); ++row) {
            auto* name = new QTableWidgetItem(confirmations.at(row).first);
            auto* value = new QTableWidgetItem(confirmations.at(row).second);
            name->setFlags(name->flags() & ~Qt::ItemIsEditable);
            value->setFlags(value->flags() & ~Qt::ItemIsEditable);
            confirmationTable->setItem(row, 0, name);
            confirmationTable->setItem(row, 1, value);
            confirmationTable->setRowHeight(row, 72);
        }
        confirmationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        confirmationTable->setSelectionMode(QAbstractItemView::NoSelection);
        confirmationTable->setFocusPolicy(Qt::NoFocus);
        confirmationTable->verticalHeader()->setVisible(false);
        confirmationTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        confirmationTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        confirmationTable->setMinimumHeight(335);
        confirmationTable->setMaximumHeight(335);
        layout->addWidget(confirmationTable);

        auto* snapshot = new QLabel(
            QStringLiteral("SNAPSHOT-PENDING　｜　水模 · 横断位 · PTScan Mock　｜　开始时冻结唯一身份"),
            page);
        snapshot->setObjectName(QStringLiteral("RealAcquisitionPlanSummary"));
        snapshot->setProperty("class", "evidenceLabel");
        snapshot->setWordWrap(false);
        snapshot->setFixedHeight(42);
        layout->addWidget(snapshot);
        auto* confirmationsDone = new QLabel(
            QStringLiteral("运行前确认项"), page);
        confirmationsDone->setObjectName(QStringLiteral("RunConfirmationChecks"));
        confirmationsDone->setProperty("class", "workflowCardTitle");
        confirmationsDone->setFixedHeight(30);
        layout->addWidget(confirmationsDone);
        auto* checksCard = makePanel(QStringLiteral("WorkflowCard"));
        checksCard->setObjectName(QStringLiteral("RunConfirmationCheckCard"));
        checksCard->setParent(page);
        checksCard->setFixedHeight(64);
        auto* checks = new QHBoxLayout(checksCard);
        checks->setContentsMargins(14, 8, 14, 8);
        checks->setSpacing(16);
        const QStringList checkLabels = {
            QStringLiteral("确认使用固定 Mock 水模预设"),
            QStringLiteral("确认横断位规划仅用于 Mock"),
            QStringLiteral("确认不会加载 SDK、连接设备或调用 Run/Abort")
        };
        QList<QCheckBox*> runConfirmationChecks;
        for (int index = 0; index < checkLabels.size(); ++index) {
            auto* check = new QCheckBox(checkLabels.at(index), checksCard);
            check->setObjectName(QStringLiteral("RunConfirmationCheck%1").arg(index + 1));
            check->setChecked(false);
            checks->addWidget(check);
            runConfirmationChecks.append(check);
        }
        checks->addStretch();
        layout->addWidget(checksCard);
        auto* gateState = new QLabel(
            QStringLiteral("真实采集：等待现场确认 · 未通过真实预检 · 本轮不会调用 Run/Abort"),
            page);
        gateState->setObjectName(QStringLiteral("RealAcquisitionGateState"));
        gateState->setProperty("class", "warningNote");
        gateState->setWordWrap(true);
        gateState->setFixedHeight(72);
        layout->addWidget(gateState);
        m_realRunButton = new QPushButton(QStringLiteral("真实 Run（等待现场确认）"), page);
        m_realRunButton->setObjectName(QStringLiteral("WorkflowRealRunButton"));
        m_realRunButton->setEnabled(false);
        const QString liveBlockedReason =
            QStringLiteral("LIVE: BLOCKED；缺少 SDK 身份、设备 IDLE、横断位映射、"
                           "冻结快照、隔离输出、唯一 Run owner 和单次人工授权");
        m_realRunButton->setToolTip(liveBlockedReason);
        m_realRunButton->setAccessibleDescription(liveBlockedReason);
        m_mockAcquireButton =
            new QPushButton(QStringLiteral("确认并进入 PTScan Mock 采集"), page);
        m_mockAcquireButton->setObjectName(QStringLiteral("MockAcquireButton"));
        m_mockAcquireButton->setProperty("class", "primary");
        m_mockAcquireButton->setEnabled(false);
        const auto updateMockAcquisitionGate = [this, runConfirmationChecks] {
            const bool allConfirmed = std::all_of(
                runConfirmationChecks.cbegin(), runConfirmationChecks.cend(),
                [](const QCheckBox* check) { return check->isChecked(); });
            const bool baselineTemplateSelected =
                m_sceneList && m_sceneList->currentItem()
                && m_sceneList->currentItem()->data(Qt::UserRole).toInt() == 0;
            const bool canStartMock = allConfirmed && baselineTemplateSelected;
            m_mockAcquireButton->setEnabled(canStartMock);
            if (m_leftMockStartButton && m_workflowStep == 8) {
                m_leftMockStartButton->setEnabled(canStartMock);
            }
            refreshWorkflow();
        };
        for (QCheckBox* check : runConfirmationChecks) {
            connect(check, &QCheckBox::toggled, this,
                    [updateMockAcquisitionGate](bool) { updateMockAcquisitionGate(); });
        }
        connect(m_mockAcquireButton, &QPushButton::clicked, this, [this] {
            m_mockRunActive = true;
            m_mockExecutionCompleted = false;
            setWorkflowStep(9);
            m_mockAcquisitionRemainingMs = 3200;
            if (m_pauseButton) {
                m_pauseButton->setProperty("mockPaused", false);
                m_pauseButton->setText(QStringLiteral("暂停（Mock）"));
            }
            m_mockAcquisitionTimer->start(m_mockAcquisitionRemainingMs);
        });
        auto* actions = new QHBoxLayout;
        auto* back = new QPushButton(QStringLiteral("返回调整定位"), page);
        back->setObjectName(QStringLiteral("RunConfirmationBackButton"));
        back->setProperty("class", "secondary");
        connect(back, &QPushButton::clicked, this, [this] { setWorkflowStep(7); });
        actions->addWidget(m_realRunButton);
        actions->addWidget(back);
        actions->addStretch();
        actions->addWidget(m_mockAcquireButton);
        layout->addLayout(actions);
        break;
    }
    case 9: {
        auto* acquisitionImage =
            addImageEvidence(QStringLiteral(":/mock-fse-acquisition.png"),
                             QStringLiteral("MockAcquisitionImage"),
                             QStringLiteral("尚未开始本次 Mock；QA 只读预览不构成运行证据。"));
        acquisitionImage->setVisible(false);
        break;
    }
    case 10: {
        auto* preview = addImageEvidence(
            QStringLiteral(":/mock-reconstruction.png"),
            QStringLiteral("MockReconstructionImage"),
            QStringLiteral("尚未执行 Mock 处理；无 RAW、解析、重建或 QC 成功证据。"));
        preview->setMaximumHeight(380);
        preview->setVisible(false);

        auto* processingSteps = new QTableWidget(5, 2, page);
        processingSteps->setObjectName(QStringLiteral("MockProcessingSteps"));
        processingSteps->setHorizontalHeaderLabels(
            {QStringLiteral("处理步骤"), QStringLiteral("当前状态")});
        processingSteps->verticalHeader()->setVisible(false);
        processingSteps->setEditTriggers(QAbstractItemView::NoEditTriggers);
        processingSteps->setSelectionMode(QAbstractItemView::NoSelection);
        processingSteps->setFocusPolicy(Qt::NoFocus);
        processingSteps->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        processingSteps->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        processingSteps->setMinimumHeight(190);
        processingSteps->setMaximumHeight(210);
        const QList<QPair<QString, QString>> processingRows = {
            {QStringLiteral("1　谱仪原生采集输出"),
             QStringLiteral("尚未执行 · MOCK 不生成真实 RAW")},
            {QStringLiteral("2　来源绑定"),
             QStringLiteral("尚未执行 · 等待合法 Mock run")},
            {QStringLiteral("3　Mock 输入解析"), QStringLiteral("尚未执行")},
            {QStringLiteral("4　标准 Mock 结果"), QStringLiteral("尚未执行")},
            {QStringLiteral("5　图像 QC"), QStringLiteral("尚未执行")}
        };
        for (int row = 0; row < processingRows.size(); ++row) {
            processingSteps->setItem(row, 0, new QTableWidgetItem(processingRows.at(row).first));
            processingSteps->setItem(row, 1, new QTableWidgetItem(processingRows.at(row).second));
        }
        layout->addWidget(processingSteps);

        auto* warning = new QLabel(
            QStringLiteral("RAW 数据合同尚待设备实测验证；当前不开放任何 k-space 视图。"),
            page);
        warning->setObjectName(QStringLiteral("RawContractWarningLabel"));
        warning->setProperty("class", "warningNote");
        warning->setWordWrap(true);
        layout->addWidget(warning);
        auto* reconstructionPath = new QLabel(
            QStringLiteral("v0.1 处理边界：只接受本次合法 MOCK 输入并生成标准 Mock 结果；"
                           "不调用 eggcontrollerV2、真实 RAW、SDK 或设备能力。"),
            page);
        reconstructionPath->setObjectName(QStringLiteral("ExistingReconstructionPathSummary"));
        reconstructionPath->setProperty("class", "evidenceLabel");
        reconstructionPath->setWordWrap(true);
        layout->addWidget(reconstructionPath);
        auto* result = new QPushButton(QStringLiteral("Mock 处理完成并查看结果"), page);
        result->setObjectName(QStringLiteral("CompleteMockProcessingButton"));
        result->setProperty("class", "primary");
        result->setEnabled(false);
        result->setToolTip(QStringLiteral("等待合法 Mock 执行与处理完成"));
        result->setAccessibleDescription(QStringLiteral("等待合法 Mock 执行与处理完成"));
        connect(result, &QPushButton::clicked, this, [this] { setWorkflowStep(11); });
        auto* retry = new QPushButton(QStringLiteral("重试 Mock 处理"), page);
        retry->setObjectName(QStringLiteral("RetryMockProcessingButton"));
        retry->setProperty("class", "secondary");
        retry->setEnabled(false);
        retry->setToolTip(QStringLiteral("当前没有可重试的 Mock 处理失败"));
        retry->setAccessibleDescription(retry->toolTip());
        auto* actions = new QHBoxLayout;
        actions->addWidget(retry);
        actions->addStretch();
        actions->addWidget(result);
        layout->addLayout(actions);
        break;
    }
    case 11: {
        auto* resultRow = new QHBoxLayout;
        resultRow->setSpacing(10);
        auto* resultRail = new QVBoxLayout;
        resultRail->setSpacing(8);
        auto* locThumbnail =
            makeGalleryThumbnail(QStringLiteral(":/mock-loc-axial.png"),
                                 QStringLiteral("ResultLocThumbnail"),
                                 QStringLiteral("定位图 LOC"), false, page);
        auto* fseThumbnail =
            makeGalleryThumbnail(QStringLiteral(":/mock-phantom.png"),
                                 QStringLiteral("ResultFseThumbnail"),
                                 QStringLiteral("PTScan · 当前"), true, page);
        locThumbnail->setVisible(false);
        fseThumbnail->setVisible(false);
        resultRail->addWidget(locThumbnail, 1);
        resultRail->addWidget(fseThumbnail, 1);
        resultRail->addStretch();
        resultRow->addLayout(resultRail);
        auto* mockResultImage =
            new ReferenceImageView(QStringLiteral(":/mock-phantom.png"),
                                   QStringLiteral("MockResultImage"), page);
        mockResultImage->setVisible(false);
        resultRow->addWidget(mockResultImage, 1);
        layout->addLayout(resultRow, 1);
        auto* evidence = new QLabel(
            QStringLiteral("尚无本次 Mock 重建图或 QC；请先完成合法执行与处理。"), page);
        evidence->setObjectName(QStringLiteral("MockImageEvidenceLabel"));
        evidence->setProperty("class", "evidenceLabel");
        evidence->setWordWrap(true);
        layout->addWidget(evidence);
        auto* controls = new QHBoxLayout;
        controls->addWidget(new QLabel(QStringLiteral("窗宽 1200　窗位 60%　缩放 100%"), page));
        controls->addStretch();
        auto* returnToLocalization = new QPushButton(QStringLiteral("返回定位 / 重新采集"), page);
        returnToLocalization->setObjectName(QStringLiteral("ReturnToLocalizationButton"));
        returnToLocalization->setProperty("class", "secondary");
        connect(returnToLocalization, &QPushButton::clicked, this,
                [this] { setWorkflowStep(7); });
        auto* confirm = new QPushButton(QStringLiteral("确认结果"), page);
        confirm->setObjectName(QStringLiteral("ConfirmResultButton"));
        confirm->setProperty("class", "primary");
        confirm->setEnabled(false);
        confirm->setToolTip(QStringLiteral("等待本次 Mock 重建与 QC 成功"));
        confirm->setAccessibleDescription(confirm->toolTip());
        connect(confirm, &QPushButton::clicked, this, [this] { setWorkflowStep(12); });
        auto* retryQc = new QPushButton(QStringLiteral("重试 Mock QC"), page);
        retryQc->setObjectName(QStringLiteral("RetryMockQcButton"));
        retryQc->setProperty("class", "secondary");
        retryQc->setEnabled(false);
        retryQc->setToolTip(QStringLiteral("当前没有可重试的 Mock QC 失败"));
        retryQc->setAccessibleDescription(retryQc->toolTip());
        controls->addWidget(returnToLocalization);
        controls->addWidget(retryQc);
        controls->addWidget(confirm);
        layout->addLayout(controls);
        break;
    }
    case 12: {
        addTitle(QStringLiteral("结果包保存与任务结束"));
        auto* content = new QHBoxLayout;
        auto* image = new ReferenceImageView(QStringLiteral(":/mock-phantom.png"),
                                             QStringLiteral("ResultPackageImage"), page);
        image->setVisible(false);
        content->addWidget(image, 3);
        auto* packageLayout = new QVBoxLayout;
        const QStringList packageItems = {
            QStringLiteral("原始数据"), QStringLiteral("标准结果"),
            QStringLiteral("QC记录"), QStringLiteral("协议与参数快照"),
            QStringLiteral("来源记录"), QStringLiteral("任务说明")
        };
        for (const QString& item : packageItems) {
            auto* card = makeGalleryCard(item, QStringLiteral("尚未生成 · 等待封存"), page);
            card->setObjectName(QStringLiteral("ResultPackageItem"));
            card->setProperty("resultItemName", item);
            packageLayout->addWidget(card);
        }
        auto* metadata = new QLabel(
            QStringLiteral("尚无已封存 Mock 结果包\n"
                           "完成合法 Mock 重建与 QC 后方可保存。"),
            page);
        metadata->setObjectName(QStringLiteral("ResultPackageMetadata"));
        metadata->setProperty("class", "evidenceLabel");
        metadata->setWordWrap(true);
        packageLayout->addWidget(metadata);
        content->addLayout(packageLayout, 2);
        layout->addLayout(content, 1);
        auto* actions = new QHBoxLayout;
        auto* save = new QPushButton(QStringLiteral("保存 Mock 结果包"), page);
        save->setObjectName(QStringLiteral("SaveResultPackageButton"));
        save->setProperty("class", "primary");
        auto* openLocation = new QPushButton(QStringLiteral("打开结果位置"), page);
        openLocation->setObjectName(QStringLiteral("OpenResultLocationButton"));
        openLocation->setProperty("class", "secondary");
        auto* external = new QPushButton(QStringLiteral("交给外部数据分析软件"), page);
        external->setObjectName(QStringLiteral("ExternalAnalysisButton"));
        external->setProperty("class", "secondary");
        m_openHistoryButton = new QPushButton(QStringLiteral("打开历史记录"), page);
        m_openHistoryButton->setObjectName(QStringLiteral("OpenHistoryButton"));
        m_openHistoryButton->setProperty("class", "secondary");
        auto* copyPath = new QPushButton(QStringLiteral("复制结果路径"), page);
        copyPath->setObjectName(QStringLiteral("CopyResultPathButton"));
        copyPath->setProperty("class", "secondary");
        auto* saveState = new QLabel(
            QStringLiteral("尚无已封存 Mock 结果包 · 所有结果动作保持禁用"), page);
        saveState->setObjectName(QStringLiteral("ResultPackageSaveState"));
        saveState->setProperty("class", "evidenceLabel");
        save->setEnabled(false);
        save->setToolTip(QStringLiteral("等待本次 Mock 重建与 QC 成功"));
        openLocation->setEnabled(false);
        openLocation->setToolTip(QStringLiteral("尚无已封存 Mock 结果目录"));
        copyPath->setEnabled(false);
        copyPath->setToolTip(QStringLiteral("尚无已封存 Mock 结果路径"));
        external->setEnabled(false);
        external->setToolTip(QStringLiteral("未配置外部分析"));
        external->setAccessibleDescription(external->toolTip());
        m_openHistoryButton->setEnabled(false);
        m_openHistoryButton->setToolTip(QStringLiteral("尚无实际封存的 Mock 结果包"));
        actions->addWidget(save);
        actions->addWidget(openLocation);
        actions->addWidget(copyPath);
        actions->addWidget(external);
        actions->addWidget(m_openHistoryButton);
        actions->addStretch();
        layout->addLayout(actions);
        layout->addWidget(saveState);
        break;
    }
    case 13: {
        auto* historyHeader = new QHBoxLayout;
        auto* historyTitle = makeGallerySectionTitle(QStringLiteral("历史记录"), page);
        historyTitle->setObjectName(QStringLiteral("WorkflowBodyLabel"));
        historyHeader->addWidget(historyTitle);
        historyHeader->addStretch();
        m_backToResultsButton = new QPushButton(QStringLiteral("← 返回当前结果"), page);
        m_backToResultsButton->setObjectName(QStringLiteral("BackToResultsButton"));
        m_backToResultsButton->setProperty("class", "secondary");
        connect(m_backToResultsButton, &QPushButton::clicked, this, [this] { setWorkflowStep(12); });
        historyHeader->addWidget(m_backToResultsButton);
        layout->addLayout(historyHeader);

        auto* filters = new QHBoxLayout;
        auto* sampleFilter = new QComboBox(page);
        sampleFilter->setObjectName(QStringLiteral("HistorySampleFilter"));
        sampleFilter->addItem(QStringLiteral("全部样品"));
        auto* templateFilter = new QComboBox(page);
        templateFilter->setObjectName(QStringLiteral("HistoryTemplateFilter"));
        templateFilter->addItem(QStringLiteral("全部模板"));
        auto* dateFilter = new QComboBox(page);
        dateFilter->setObjectName(QStringLiteral("HistoryDateFilter"));
        dateFilter->addItem(QStringLiteral("全部时间"));
        auto* filter = new QLineEdit(page);
        filter->setObjectName(QStringLiteral("HistoryFilter"));
        filter->setPlaceholderText(QStringLiteral("搜索运行 ID 或样品 ID"));
        const QString emptyHistoryReason = QStringLiteral("尚无已封存 Mock 结果包");
        for (QWidget* control : {static_cast<QWidget*>(sampleFilter),
                                 static_cast<QWidget*>(templateFilter),
                                 static_cast<QWidget*>(dateFilter),
                                 static_cast<QWidget*>(filter)}) {
            control->setEnabled(false);
            control->setToolTip(emptyHistoryReason);
            control->setAccessibleDescription(emptyHistoryReason);
        }
        filters->addWidget(sampleFilter);
        filters->addWidget(templateFilter);
        filters->addWidget(dateFilter);
        filters->addWidget(filter, 1);
        layout->addLayout(filters);
        auto* table = new QTableWidget(0, 7, page);
        table->setObjectName(QStringLiteral("HistoryReadOnlyTable"));
        table->setHorizontalHeaderLabels({QStringLiteral("运行ID"), QStringLiteral("时间"),
                                          QStringLiteral("样品ID"), QStringLiteral("任务模板"),
                                          QStringLiteral("方案版本"), QStringLiteral("结果包"),
                                          QStringLiteral("QC状态")});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setEnabled(false);
        table->setToolTip(emptyHistoryReason);
        table->setAccessibleDescription(emptyHistoryReason);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        layout->addWidget(table, 1);

        const auto applyHistoryFilter = [table, sampleFilter, templateFilter, dateFilter, filter] {
            const QString sample = sampleFilter->currentText();
            const QString selectedTemplate = templateFilter->currentText();
            const QString date = dateFilter->currentText();
            const QString query = filter->text().trimmed();
            for (int row = 0; row < table->rowCount(); ++row) {
                const bool sampleMatches = sample == QStringLiteral("全部样品")
                    || table->item(row, 2)->text() == sample;
                const bool templateMatches = selectedTemplate == QStringLiteral("全部模板")
                    || table->item(row, 3)->text() == selectedTemplate;
                const bool dateMatches = date == QStringLiteral("全部时间")
                    || table->item(row, 1)->text().startsWith(date);
                const QString rowText = QStringLiteral("%1 %2")
                    .arg(table->item(row, 0)->text(), table->item(row, 2)->text());
                const bool queryMatches = query.isEmpty() || rowText.contains(query, Qt::CaseInsensitive);
                table->setRowHidden(row, !(sampleMatches && templateMatches && dateMatches && queryMatches));
            }
        };
        connect(sampleFilter, &QComboBox::currentTextChanged, this,
                [applyHistoryFilter](const QString&) { applyHistoryFilter(); });
        connect(templateFilter, &QComboBox::currentTextChanged, this,
                [applyHistoryFilter](const QString&) { applyHistoryFilter(); });
        connect(dateFilter, &QComboBox::currentTextChanged, this,
                [applyHistoryFilter](const QString&) { applyHistoryFilter(); });
        connect(filter, &QLineEdit::textChanged, this,
                [applyHistoryFilter](const QString&) { applyHistoryFilter(); });

        connect(table, &QTableWidget::currentCellChanged, this,
                [this, table](int currentRow, int, int, int) {
            if (currentRow < 0 || !m_historySelectionSummary) return;
            m_historySelectionSummary->setText(
                QStringLiteral("运行 ID　RUN-MOCK-%1\n"
                               "样品 ID　%2\n"
                               "结果包　　%3\n"
                               "QC　　　　%4\n"
                               "来源记录　完整 · Mock")
                    .arg(currentRow + 1, 3, 10, QLatin1Char('0'))
                     .arg(table->item(currentRow, 2)->text(),
                          table->item(currentRow, 5)->text(),
                          table->item(currentRow, 6)->text()));
        });

        auto* actions = new QHBoxLayout;
        auto* open = new QPushButton(QStringLiteral("打开所选结果（只读）"), page);
        open->setObjectName(QStringLiteral("HistoryOpenButton"));
        open->setProperty("class", "primary");
        auto* compare = new QPushButton(QStringLiteral("设为对比参考"), page);
        compare->setObjectName(QStringLiteral("HistoryCompareButton"));
        compare->setProperty("class", "secondary");
        auto* source = new QPushButton(QStringLiteral("查看来源记录"), page);
        source->setObjectName(QStringLiteral("HistorySourceButton"));
        source->setProperty("class", "secondary");
        auto* historyActionState = new QLabel(QStringLiteral("历史记录只读"), page);
        historyActionState->setObjectName(QStringLiteral("HistoryActionState"));
        historyActionState->setProperty("class", "evidenceLabel");
        open->setEnabled(false);
        open->setToolTip(emptyHistoryReason);
        source->setEnabled(false);
        source->setToolTip(emptyHistoryReason);
        compare->setEnabled(false);
        compare->setToolTip(QStringLiteral("v0.1 单选只读历史不支持对比"));
        for (QPushButton* button : {open, compare, source})
            button->setAccessibleDescription(button->toolTip());
        historyActionState->setText(QStringLiteral("尚无已封存 Mock 结果包"));
        actions->addWidget(open);
        actions->addWidget(compare);
        actions->addWidget(source);
        actions->addStretch();
        layout->addLayout(actions);
        layout->addWidget(historyActionState);
        auto* readOnlyNote = new QLabel(
            QStringLiteral("历史记录只读；基于历史参数创建新方案不会覆盖原结果。"), page);
        readOnlyNote->setObjectName(QStringLiteral("HistoryReadOnlyNote"));
        readOnlyNote->setProperty("class", "warningNote");
        layout->addWidget(readOnlyNote);
        break;
    }
    default:
        break;
    }
    return page;
}

QWidget* MainWindow::makeLegacyWorkflowPage(int step)
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

void MainWindow::resetRunConfirmations()
{
    for (int index = 1; index <= 3; ++index) {
        if (auto* check = findChild<QCheckBox*>(
                QStringLiteral("RunConfirmationCheck%1").arg(index))) {
            check->setChecked(false);
        }
    }
    if (m_mockAcquireButton) {
        m_mockAcquireButton->setEnabled(false);
    }
    if (m_leftMockStartButton) {
        m_leftMockStartButton->setEnabled(false);
    }
}

void MainWindow::setWorkflowStep(int step)
{
    const int nextStep = qBound(1, step, 13);
    if (m_workflowStep == 9 && nextStep != 9 && m_mockAcquisitionTimer) {
        m_mockAcquisitionTimer->stop();
        m_mockAcquisitionRemainingMs = 3200;
        if (!m_mockExecutionCompleted) m_mockRunActive = false;
    }
    if (nextStep == 8 && m_workflowStep != 8) {
        resetRunConfirmations();
    }
    m_workflowStep = nextStep;
    if (m_workflowStep > 1 && m_primarySceneCombo && m_primarySceneCombo->currentIndex() < 0) {
        m_primarySceneCombo->setCurrentIndex(0);
    }
    if (m_workflowPages) m_workflowPages->setCurrentIndex(m_workflowStep - 1);
    if (m_workflowRightPages) m_workflowRightPages->setCurrentIndex(m_workflowStep - 1);
    refreshWorkflow();
}

void MainWindow::setMockWorkflowStep(int step)
{
    if (m_mockAcquisitionTimer) m_mockAcquisitionTimer->stop();
    m_mockAcquisitionRemainingMs = 3200;
    m_mockRunActive = false;
    m_mockExecutionCompleted = false;
    setWorkflowStep(step);
}

void MainWindow::refreshWorkflow()
{
    static const QStringList titles = {
        QStringLiteral("进入系统"), QStringLiteral("选择场景与对象"), QStringLiteral("确认任务模板"),
        QStringLiteral("样品登记与预检"), QStringLiteral("扫描方案"), QStringLiteral("LOC定位采集"),
        QStringLiteral("切片规划"), QStringLiteral("运行前确认"), QStringLiteral("PTScan采集"),
        QStringLiteral("RAW保存与重建"), QStringLiteral("标准结果与QC"),
        QStringLiteral("保存结果包"), QStringLiteral("历史记录")
    };
    if (m_workflowCurrentStepLabel) {
        m_workflowCurrentStepLabel->setText(QStringLiteral("%1").arg(m_workflowStep, 2, 10, QLatin1Char('0')));
    }
    if (m_workflowStatusLabel) {
        if (m_workflowStep == 13) {
            m_workflowStatusLabel->setText(
                QStringLiteral("按需工具：历史记录　｜　当前任务：水模横断位基线　｜　← 返回当前结果"));
        } else if ((m_workflowStep == 10 && !m_mockExecutionCompleted)
                   || m_workflowStep == 11 || m_workflowStep == 12) {
            m_workflowStatusLabel->setText(
                QStringLiteral("前置未完成：合法 MOCK 闭环　｜　当前：%1（只读 Empty）　｜　下一步：返回完成前置")
                    .arg(titles.at(m_workflowStep - 1)));
        } else {
            const QString completed = m_workflowStep == 1
                ? QStringLiteral("—")
                : titles.at(m_workflowStep - 2);
            const QString next = m_workflowStep == 12
                ? QStringLiteral("按需历史")
                : titles.at(m_workflowStep);
            m_workflowStatusLabel->setText(
                QStringLiteral("已完成：%1　｜　当前：%2　｜　下一步：%3")
                    .arg(completed, titles.at(m_workflowStep - 1), next));
        }
    }
    if (m_workflowBackButton) {
        const bool canGoBack = m_workflowStep > 1;
        m_workflowBackButton->setVisible(canGoBack);
        m_workflowBackButton->setEnabled(canGoBack && !m_mockRunActive);
        m_workflowBackButton->setToolTip(
            m_mockRunActive
                ? QStringLiteral("Mock 运行中请使用暂停或停止；不可静默离开")
                : QString());
        m_workflowBackButton->setAccessibleDescription(
            m_workflowBackButton->toolTip());
        if (canGoBack) {
            m_workflowBackButton->setText(m_workflowStep == 10
                                              ? QStringLiteral("返回：运行前确认")
                                              : QStringLiteral("返回：%1").arg(
                                                    titles.at(m_workflowStep - 2)));
        }
    }
    if (m_workflowNextButton) {
        const bool isMainWorkflowPage = m_workflowStep <= 12;
        const QStringList canonicalActions = {
            QStringLiteral("BeginResearchButton"),
            QStringLiteral("ShowRecommendedTemplateButton"),
            QStringLiteral("AcceptTemplateButton"),
            QStringLiteral("SavePreparationButton"),
            QStringLiteral("ContinueProtocolButton"),
            QStringLiteral("OpenLocalizationPlanningButton"),
            QStringLiteral("ConfirmLocalizationButton"),
            QStringLiteral("MockAcquireButton"),
            QString(),
            QStringLiteral("CompleteMockProcessingButton"),
            QStringLiteral("ConfirmResultButton"),
            QStringLiteral("OpenHistoryButton")
        };
        QPushButton* canonical = nullptr;
        if (m_workflowStep >= 1 && m_workflowStep <= canonicalActions.size()
            && !canonicalActions.at(m_workflowStep - 1).isEmpty()) {
            canonical =
                findChild<QPushButton*>(canonicalActions.at(m_workflowStep - 1));
        }
        m_workflowNextButton->setVisible(isMainWorkflowPage);
        m_workflowNextButton->setEnabled(
            isMainWorkflowPage && canonical && canonical->isEnabled());
        if (canonical) {
            m_workflowNextButton->setText(QStringLiteral("下一步：%1").arg(canonical->text()));
            m_workflowNextButton->setToolTip(canonical->toolTip());
            m_workflowNextButton->setAccessibleDescription(
                canonical->accessibleDescription().isEmpty()
                    ? canonical->toolTip()
                    : canonical->accessibleDescription());
        } else {
            m_workflowNextButton->setText(
                m_workflowStep == 9
                    ? QStringLiteral("等待本次 Mock 采集完成")
                    : QStringLiteral("当前步骤没有可继续动作"));
            m_workflowNextButton->setToolTip(
                m_workflowStep == 9
                    ? QStringLiteral("采集进度由本次合法 Mock 运行驱动")
                    : QStringLiteral("需先生成并封存 Mock 结果包"));
            m_workflowNextButton->setAccessibleDescription(
                m_workflowNextButton->toolTip());
        }
    }
    if (auto* showRecommended =
            findChild<QPushButton*>(QStringLiteral("ShowRecommendedTemplateButton"))) {
        const bool baselineTemplateSelected =
            m_sceneList && m_sceneList->currentItem()
            && m_sceneList->currentItem()->data(Qt::UserRole).toInt() == 0;
        showRecommended->setEnabled(baselineTemplateSelected);
        showRecommended->setToolTip(
            baselineTemplateSelected
                ? QString()
                : QStringLiteral("当前模板仅供浏览；v0.1 主流程只支持水模基线"));
        showRecommended->setAccessibleDescription(showRecommended->toolTip());
        if (m_workflowStep == 2 && m_workflowNextButton) {
            m_workflowNextButton->setEnabled(showRecommended->isEnabled());
            m_workflowNextButton->setToolTip(showRecommended->toolTip());
            m_workflowNextButton->setAccessibleDescription(
                showRecommended->accessibleDescription().isEmpty()
                    ? showRecommended->toolTip()
                    : showRecommended->accessibleDescription());
        }
    }
    if (m_useSelectedTemplateButton) {
        const bool hasTemplate =
            m_sceneList && m_sceneList->currentItem()
            && m_sceneList->currentItem()->data(Qt::UserRole).toInt() >= 0;
        const bool baselineTemplateSelected =
            hasTemplate
            && m_sceneList->currentItem()->data(Qt::UserRole).toInt() == 0;
        const bool mockAcquisitionRunning = m_mockRunActive;
        m_useSelectedTemplateButton->setEnabled(
            baselineTemplateSelected && !mockAcquisitionRunning);
        m_useSelectedTemplateButton->setText(
            mockAcquisitionRunning
                ? QStringLiteral("Mock 采集中，请先返回或停止")
            : baselineTemplateSelected
                ? QStringLiteral("使用所选模板 → 确认任务")
            : hasTemplate
                ? QStringLiteral("当前模板仅供浏览（主流程仅水模基线）")
                : QStringLiteral("请选择场景与检测对象"));
    }
    const QString lockedSelectionReason =
        QStringLiteral("Mock 运行中不可切换场景、对象或模板；请先暂停或停止");
    for (QWidget* selector : {
             static_cast<QWidget*>(m_primarySceneCombo),
             static_cast<QWidget*>(m_targetCombo),
             static_cast<QWidget*>(m_templateSearchEdit),
             static_cast<QWidget*>(m_sceneList)}) {
        if (!selector) continue;
        selector->setEnabled(!m_mockRunActive);
        selector->setToolTip(m_mockRunActive ? lockedSelectionReason : QString());
        selector->setAccessibleDescription(selector->toolTip());
    }
    if (m_addComparisonButton) {
        m_addComparisonButton->setEnabled(
            !m_comparisonEnabled && !m_mockRunActive);
        m_addComparisonButton->setText(
            m_comparisonEnabled
                ? QStringLiteral("FSE B 对照已添加")
                : QStringLiteral("添加 FSE B 对照协议"));
        m_addComparisonButton->setToolTip(
            m_comparisonEnabled
                ? QStringLiteral("对照协议已加入当前 Mock 草稿")
                : QString());
        m_addComparisonButton->setAccessibleDescription(
            m_addComparisonButton->toolTip());
    }
    if (m_leftMockStartButton) {
        const bool mockAcquisitionRunning = m_mockRunActive;
        m_leftMockStartButton->setText(mockAcquisitionRunning
                                           ? QStringLiteral("运行中（Mock）")
                                           : m_workflowStep == 8
                                               ? QStringLiteral("开始采集（Mock）")
                                               : QStringLiteral("采集入口（待确认）"));
        m_leftMockStartButton->setProperty(
            "class", mockAcquisitionRunning ? QStringLiteral("running")
                                              : QStringLiteral("success"));
        m_leftMockStartButton->setEnabled(
            m_workflowStep == 8 && m_mockAcquireButton
            && m_mockAcquireButton->isEnabled());
        m_leftMockStartButton->style()->unpolish(m_leftMockStartButton);
        m_leftMockStartButton->style()->polish(m_leftMockStartButton);
    }
    if (m_pauseButton) {
        const bool mockStep = m_mockRunActive;
        if (!mockStep) {
            m_pauseButton->setProperty("mockPaused", false);
            m_pauseButton->setText(QStringLiteral("暂停（Mock）"));
        }
        m_pauseButton->setEnabled(mockStep);
    }
    if (m_leftMockStopButton)
        m_leftMockStopButton->setEnabled(m_mockRunActive);
    if (auto* acquisitionPage =
            findChild<QWidget*>(QStringLiteral("WorkflowPage09"))) {
        if (auto* image =
                acquisitionPage->findChild<QWidget*>(QStringLiteral("MockAcquisitionImage"))) {
            image->setVisible(m_mockRunActive);
        }
        if (auto* evidence =
                acquisitionPage->findChild<QLabel*>(QStringLiteral("MockImageEvidenceLabel"))) {
            evidence->setText(
                m_mockRunActive
                    ? QStringLiteral("MOCK 采集进行中 · 未加载 SDK、未连接设备、未调用 Run/Abort")
                    : QStringLiteral("尚未开始本次 Mock；QA 只读预览不构成运行证据。"));
        }
    }
    if (auto* state =
            findChild<QLabel*>(QStringLiteral("RightPage09RunState"))) {
        state->setText(
            m_mockRunActive
                ? QStringLiteral("MOCK 运行中 · 进度由本次计时任务驱动")
                : QStringLiteral("尚未开始本次 Mock · 无采集进度或输出"));
    }
    if (m_protocolChainLabel) {
        m_protocolChainLabel->setText(
            m_comparisonEnabled
                ? QStringLiteral("横断位 LOC → PTScan → FSE B 对照（用户已主动添加，Mock）")
                : QStringLiteral("横断位 LOC → PTScan（单次基线；当前 HOLD）"));
    }
    if (m_scanPlanChainLabel) {
        m_scanPlanChainLabel->setText(
            m_comparisonEnabled
                ? QStringLiteral("协议链　横断位 LOC → PTScan → FSE B（用户主动添加的对照）")
                : QStringLiteral("协议链　横断位 LOC → PTScan（单次基线）"));
    }
    if (m_workflowOutputSummary) {
        m_workflowOutputSummary->setText(QStringLiteral("当前步骤 %1 · 所有图像、数值和输出均为 Mock/设计示例").arg(m_workflowStep, 2, 10, QLatin1Char('0')));
    }
}

QWidget* MainWindow::makeWorkflowRightPage(int step)
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("RightPage%1").arg(step, 2, 10, QLatin1Char('0')));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(10);

    static const QStringList titles = {
        QStringLiteral("设备与安全状态"), QStringLiteral("推荐依据"),
        QStringLiteral("模板边界"), QStringLiteral("预检摘要"),
        QStringLiteral("方案摘要"), QStringLiteral("采集状态"),
        QStringLiteral("图像质控与输出"), QStringLiteral("运行前状态"),
        QStringLiteral("采集状态"), QStringLiteral("处理状态"),
        QStringLiteral("图像质控与输出"), QStringLiteral("完成摘要"),
        QStringLiteral("所选记录")
    };
    auto* title = new QLabel(titles.at(step - 1), page);
    title->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(title);

    const auto addStatus = [page, layout, step](const QString& name, const QString& value,
                                                const QString& state = QStringLiteral("neutral")) {
        auto* card = makeGalleryCard(name, value, page);
        card->setProperty("state", state);
        if (step >= 6 && step <= 9) {
            card->setProperty("rightStatus", true);
            card->setMinimumHeight(84);
        }
        layout->addWidget(card);
        return card;
    };
    const auto addWarningNote = [page, layout](const QString& text) {
        auto* note = new QLabel(text, page);
        note->setProperty("class", "warningNote");
        note->setWordWrap(true);
        layout->addWidget(note);
    };

    switch (step) {
    case 1:
        addStatus(QStringLiteral("设备连接"), QStringLiteral("未连接"), QStringLiteral("pending"));
        addStatus(QStringLiteral("接收状态"), QStringLiteral("未知"), QStringLiteral("pending"));
        addStatus(QStringLiteral("温度"), QStringLiteral("未知"), QStringLiteral("pending"));
        addStatus(QStringLiteral("异常"), QStringLiteral("无异常"), QStringLiteral("success"));
        addStatus(QStringLiteral("安全提示"),
                  QStringLiteral("当前仅可浏览和配置，真实采集尚未放行。"),
                  QStringLiteral("warning"));
        break;
    case 2:
        addStatus(QStringLiteral("科研目标"), QStringLiteral("水模横断位基线成像"));
        addStatus(QStringLiteral("检测对象"), QStringLiteral("水模 · 位置待现场确认"));
        addStatus(QStringLiteral("当前建议"), QStringLiteral("横断位 LOC → 单次 PTScan"));
        addStatus(QStringLiteral("候选协议"), QStringLiteral("PTScan.par · 待核验"));
        break;
    case 3:
        addStatus(QStringLiteral("参数状态"), QStringLiteral("系统模板 · 只读"));
        addStatus(QStringLiteral("定位方向"), QStringLiteral("横断位 · 待现场复核"));
        addStatus(QStringLiteral("首轮协议"), QStringLiteral("LOC → 单次 PTScan · 待核验"));
        addStatus(QStringLiteral("真实 Run"), QStringLiteral("HOLD"), QStringLiteral("warning"));
        break;
    case 4:
        addStatus(QStringLiteral("样品信息"), QStringLiteral("水模 · 已登记"));
        addStatus(QStringLiteral("水模/线圈位置"), QStringLiteral("待现场确认"), QStringLiteral("warning"));
        addStatus(QStringLiteral("存储"), QStringLiteral("未执行真实检查"), QStringLiteral("warning"));
        addStatus(QStringLiteral("连接/温度/空闲"), QStringLiteral("未执行真实预检"), QStringLiteral("warning"));
        addWarningNote(QStringLiteral("真实采集前必须完成水模/线圈位置、横断位、连接/温度/空闲和输出目录确认。"));
        break;
    case 5:
        addStatus(QStringLiteral("预计步骤"), QStringLiteral("横断位定位 + 单次 PTScan"));
        addStatus(QStringLiteral("第二组采集"), QStringLiteral("未加入"));
        addStatus(QStringLiteral("参数来源"), QStringLiteral("开发预设 · Mock"));
        addStatus(QStringLiteral("设备适配"), QStringLiteral("待实机确认"), QStringLiteral("warning"));
        addStatus(QStringLiteral("运行快照"), QStringLiteral("尚未冻结"));
        break;
    case 6:
        addStatus(QStringLiteral("序列"), QStringLiteral("定位 LOC · Mock 规划参考"));
        addStatus(QStringLiteral("进度"), QStringLiteral("尚未开始"));
        addStatus(QStringLiteral("数据保存"), QStringLiteral("无输出"));
        addStatus(QStringLiteral("异常"), QStringLiteral("未执行，无法评估"), QStringLiteral("pending"));
        addStatus(QStringLiteral("下一步"), QStringLiteral("调整定位与切片"));
        addWarningNote(QStringLiteral("当前仅为 Mock 定位规划参考；真实设备 Run 未执行，也没有采集进度。"));
        break;
    case 7:
        addStatus(QStringLiteral("标准结果"), QStringLiteral("待生成"));
        addStatus(QStringLiteral("SNR"), QStringLiteral("待计算"));
        addStatus(QStringLiteral("均匀性"), QStringLiteral("待计算"));
        addStatus(QStringLiteral("畸变 / 尺寸"), QStringLiteral("待复核"));
        addStatus(QStringLiteral("重复稳定"), QStringLiteral("需重复扫描"));
        addStatus(QStringLiteral("结果包"), QStringLiteral("待生成"));
        {
            auto* note = new QLabel(
                QStringLiteral("完成 Mock 采集与重建后显示质控摘要，由科研用户最终确认。"), page);
            note->setProperty("class", "evidenceLabel");
            note->setWordWrap(true);
            layout->addWidget(note);
        }
        break;
    case 8:
        addStatus(QStringLiteral("水模/线圈位置"), QStringLiteral("待现场确认"), QStringLiteral("warning"));
        addStatus(QStringLiteral("横断位规划"), QStringLiteral("待现场复核"), QStringLiteral("warning"));
        addStatus(QStringLiteral("连接/温度/空闲"), QStringLiteral("未通过真实预检"), QStringLiteral("warning"));
        addStatus(QStringLiteral("输出目录"), QStringLiteral("未执行可写检查"), QStringLiteral("warning"));
        addStatus(QStringLiteral("单次真实 Run"), QStringLiteral("等待现场确认"), QStringLiteral("warning"));
        addWarningNote(QStringLiteral("当前仅可进入 Mock；不会加载 SDK、调用 Run/Abort 或生成 RAW。"));
        break;
    case 9:
        addStatus(QStringLiteral("数据源"), QStringLiteral("MOCK"));
        {
            auto* state = new QLabel(
                QStringLiteral("尚未开始本次 Mock · 无采集进度或输出"), page);
            state->setObjectName(QStringLiteral("RightPage09RunState"));
            state->setProperty("class", "warningNote");
            state->setWordWrap(true);
            layout->addWidget(state);
        }
        addStatus(QStringLiteral("真实 Run"), QStringLiteral("LIVE: BLOCKED"), QStringLiteral("warning"));
        addWarningNote(QStringLiteral("只有从运行前确认页合法启动，才会显示 MOCK 进度；QA 跳页不算运行。"));
        break;
    case 10:
        addStatus(QStringLiteral("原生输出"), QStringLiteral("尚未生成"));
        addStatus(QStringLiteral("来源记录"), QStringLiteral("尚未绑定"));
        addStatus(QStringLiteral("Mock 输入解析"), QStringLiteral("尚未执行"));
        addStatus(QStringLiteral("标准 Mock 重建"), QStringLiteral("尚未执行"));
        addStatus(QStringLiteral("Mock 图像 QC"), QStringLiteral("尚未执行"));
        addStatus(QStringLiteral("状态"), QStringLiteral("等待合法 Mock 执行完成"), QStringLiteral("pending"));
        break;
    case 11: {
        addStatus(QStringLiteral("标准 Mock 结果"), QStringLiteral("尚未生成"));
        addStatus(QStringLiteral("Mock QC"), QStringLiteral("尚未计算 · 不显示数值"));
        addStatus(QStringLiteral("科研确认"), QStringLiteral("等待结果生成"));
        addWarningNote(QStringLiteral("Empty/失败/取消状态不显示图像或 QC 数值。"));
        break;
    }
    case 12:
        addStatus(QStringLiteral("采集"), QStringLiteral("尚无成功 Mock 运行"));
        addStatus(QStringLiteral("重建"), QStringLiteral("尚无成功 Mock 重建"));
        addStatus(QStringLiteral("QC"), QStringLiteral("尚无 QC 数值"));
        addStatus(QStringLiteral("结果包"), QStringLiteral("尚未生成"));
        addStatus(QStringLiteral("外部分析"), QStringLiteral("未配置"), QStringLiteral("warning"));
        addStatus(QStringLiteral("留存策略"), QStringLiteral("生成后同名不覆盖"));
        break;
    case 13:
        m_historySelectionSummary = new QLabel(
            QStringLiteral("尚无已封存 Mock 结果包\n"
                           "历史页只读取实际生成且通过完整性检查的 manifest。"),
            page);
        m_historySelectionSummary->setObjectName(QStringLiteral("HistorySelectionSummary"));
        m_historySelectionSummary->setProperty("class", "workflowSummary");
        m_historySelectionSummary->setWordWrap(true);
        layout->addWidget(m_historySelectionSummary);
        {
            auto* preview = new ReferenceImageView(QStringLiteral(":/mock-phantom.png"),
                                                   QStringLiteral("HistoryPreviewImage"), page);
            preview->setVisible(false);
            layout->addWidget(preview, 1);
        }
        break;
    default:
        break;
    }
    layout->addStretch();
    return page;
}

QWidget* MainWindow::buildRightPane()
{
    auto* outer = new QWidget;
    outer->setObjectName(QStringLiteral("RightColumn"));
    outer->setMinimumWidth(290);
    outer->setMaximumWidth(335);
    auto* outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(10);
    outerLayout->addSpacing(28);

    auto* runHold = new QLabel(QStringLiteral("Run:　HOLD"), outer);
    runHold->setObjectName(QStringLiteral("RunHoldLabel"));
    runHold->setProperty("class", "runHold");
    runHold->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    runHold->setFixedHeight(48);
    outerLayout->addWidget(runHold);

    auto* frame = makePanel(QStringLiteral("Panel"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    m_workflowRightPages = new QStackedWidget(frame);
    m_workflowRightPages->setObjectName(QStringLiteral("WorkflowRightStack"));
    for (int step = 1; step <= 13; ++step)
        m_workflowRightPages->addWidget(makeWorkflowRightPage(step));
    layout->addWidget(m_workflowRightPages, 1);
    outerLayout->addWidget(frame, 1);
    return outer;
}

QWidget* MainWindow::buildLegacyRightPane()
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
    if (m_useSelectedTemplateButton) {
        const bool baselineTemplateSelected =
            m_sceneList->currentItem()
            && m_sceneList->currentItem()->data(Qt::UserRole).toInt() == 0;
        m_useSelectedTemplateButton->setEnabled(baselineTemplateSelected);
        m_useSelectedTemplateButton->setText(
            baselineTemplateSelected
                ? QStringLiteral("使用所选模板 → 确认任务")
                : QStringLiteral("当前模板仅供浏览（主流程仅水模基线）"));
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
    resetRunConfirmations();
    const bool baselineTemplateSelected =
        m_sceneList && m_sceneList->currentItem()
        && m_sceneList->currentItem()->data(Qt::UserRole).toInt() == 0;
    if (!baselineTemplateSelected && m_workflowStep > 2) {
        setWorkflowStep(2);
        return;
    }
    if (baselineTemplateSelected) {
        applyScene(currentScene());
    } else {
        refreshWorkflow();
    }
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
    if (m_automationStatusLabel) {
        m_automationStatusLabel->setText(
            QStringLiteral("DRY_RUN 完成 · Mock 参数快照 · 未写入 SDK"));
    }
}

void MainWindow::handleStart()
{
    appendLog(QStringLiteral("真实 Run 永久 HOLD；请使用第 08 步 Mock 采集。"));
}

void MainWindow::handlePause()
{
    if (!m_pauseButton || m_workflowStep != 9 || !m_mockRunActive
        || !m_mockAcquisitionTimer) {
        return;
    }
    const bool paused = !m_pauseButton->property("mockPaused").toBool();
    if (paused) {
        if (m_mockAcquisitionTimer->isActive()) {
            m_mockAcquisitionRemainingMs =
                qMax(1, m_mockAcquisitionTimer->remainingTime());
        }
        m_mockAcquisitionTimer->stop();
    } else {
        m_mockAcquisitionTimer->start(qMax(1, m_mockAcquisitionRemainingMs));
    }
    m_pauseButton->setProperty("mockPaused", paused);
    m_pauseButton->setText(paused ? QStringLiteral("继续（Mock）")
                                 : QStringLiteral("暂停（Mock）"));
    appendLog(paused ? QStringLiteral("Mock 已暂停；未调用真实暂停。")
                     : QStringLiteral("Mock 已继续；未调用真实继续。"));
    if (m_automationStatusLabel) {
        m_automationStatusLabel->setText(
            paused ? QStringLiteral("Mock 已暂停 · 未调用设备")
                   : QStringLiteral("Mock 已继续 · 未调用设备"));
    }
}

void MainWindow::handleResume()
{
    appendLog(QStringLiteral("Mock 工作流不调用真实继续。"));
    if (m_automationStatusLabel) {
        m_automationStatusLabel->setText(QStringLiteral("Mock 已继续 · 未调用设备"));
    }
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
    if (m_connectionBadge) {
        m_connectionBadge->setText(connection);
        m_connectionBadge->setProperty("class", badgeClassForState(connection));
    }
    if (m_transferBadge) {
        m_transferBadge->setText(transfer);
        m_transferBadge->setProperty("class", badgeClassForState(transfer));
    }
    if (m_abnormalBadge) {
        m_abnormalBadge->setText(abnormal);
        m_abnormalBadge->setProperty("class", badgeClassForState(abnormal));
    }
    if (m_footerConnectionValue) m_footerConnectionValue->setText(connection);
    if (m_footerAbnormalValue) m_footerAbnormalValue->setText(abnormal);
}

void MainWindow::updateScan(const QString& scanState, const QString& scanProgress)
{
    if (m_scanStateBadge) {
        m_scanStateBadge->setText(scanState);
        m_scanStateBadge->setProperty("class", badgeClassForState(scanState));
    }
    if (m_scanProgressBadge) {
        m_scanProgressBadge->setText(scanProgress);
        m_scanProgressBadge->setProperty("class", badgeClassForState(scanState));
    }
    if (m_footerScanValue) m_footerScanValue->setText(scanState + QStringLiteral(" / ") + scanProgress);
}

void MainWindow::updateMetrics(const QString& snr, const QString& uniformity, const QString& peak, const QString& area)
{
    if (m_snrValue) m_snrValue->setText(snr);
    if (m_uniformityValue) m_uniformityValue->setText(uniformity);
    if (m_peakValue) m_peakValue->setText(peak);
    if (m_areaValue) m_areaValue->setText(area);
}

void MainWindow::updateTemperature(const QString& temperature)
{
    if (m_temperatureBadge) m_temperatureBadge->setText(temperature);
    if (m_footerTemperatureValue) m_footerTemperatureValue->setText(temperature);
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
    if (m_mockAcquisitionTimer) m_mockAcquisitionTimer->stop();
    m_mockRunActive = false;
    m_mockExecutionCompleted = false;
    m_preparationConfirmed = false;
    m_protocolUseOnceConfirmed = false;
    resetRunConfirmations();
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
