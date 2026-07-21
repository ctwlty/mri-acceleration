#include "MriSdkLoader.h"

#include <QFileInfo>
#include <QDir>
#include <QtMath>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MriSdkLoader::MriSdkLoader()
{
}

MriSdkLoader::~MriSdkLoader()
{
    shutdown();
    unload();
}

MriSdkResult MriSdkLoader::load(const QString& dllPath)
{
    unload();
    m_error.clear();
    m_dllPath = dllPath;
    if (dllPath.isEmpty()) {
        setError(QStringLiteral("未选择 mridll.dll"));
        m_sessionState = MriSdkSessionState::Fault;
        return MriSdkResult::failure(QStringLiteral("load"), QStringLiteral("LoadLibrary"), -1, m_error);
    }

#ifdef Q_OS_WIN
    const QString absolutePath = QFileInfo(dllPath).absoluteFilePath();
    m_handle = reinterpret_cast<void*>(LoadLibraryW(reinterpret_cast<const wchar_t*>(absolutePath.utf16())));
    if (!m_handle) {
        const int errorCode = static_cast<int>(GetLastError());
        setError(QStringLiteral("加载 mridll.dll 失败：%1（Windows 错误 %2）").arg(absolutePath).arg(errorCode));
        m_sessionState = MriSdkSessionState::Fault;
        return MriSdkResult::failure(QStringLiteral("load"), QStringLiteral("LoadLibrary"), errorCode, m_error);
    }
    if (!bindAll()) {
        const QString bindError = m_error;
        unload();
        setError(bindError);
        m_sessionState = MriSdkSessionState::Fault;
        return MriSdkResult::failure(QStringLiteral("bind"), QStringLiteral("GetProcAddress"), -1, m_error);
    }
    m_mode = Mode::Real;
    m_sessionState = MriSdkSessionState::Loaded;
    return MriSdkResult::success(QStringLiteral("load"));
#else
    setError(QStringLiteral("当前平台不支持 Windows DLL 加载"));
    m_sessionState = MriSdkSessionState::Fault;
    return MriSdkResult::failure(QStringLiteral("load"), QStringLiteral("LoadLibrary"), -1, m_error);
#endif
}

void MriSdkLoader::unload()
{
    shutdown();
#ifdef Q_OS_WIN
    if (m_handle) {
        FreeLibrary(static_cast<HMODULE>(m_handle));
    }
#endif
    m_handle = nullptr;
    m_init = nullptr;
    m_configFile = nullptr;
    m_setOutputPath = nullptr;
    m_setParameterFile = nullptr;
    m_setSaveMode = nullptr;
    m_setSystemSel = nullptr;
    m_setChannelValid = nullptr;
    m_setParameter = nullptr;
    m_setTxCenterFre = nullptr;
    m_setChannelValue = nullptr;
    m_saveParameterFile = nullptr;
    m_setOutputPrefix = nullptr;
    m_setAllPreempValue = nullptr;
    m_setAllGraAnalogDelay = nullptr;
    m_setSingleGraGmax = nullptr;
    m_setPreempCross = nullptr;
    m_setPreempValue = nullptr;
    m_run = nullptr;
    m_abort = nullptr;
    m_closeSys = nullptr;
    m_scanStatus = nullptr;
    m_scanCompleted = nullptr;
    m_getTotalScanNo = nullptr;
    m_getCurrentScanNo = nullptr;
    m_getTemperature = nullptr;
    m_getConnectStatus = nullptr;
    m_systemOpen = false;
    m_abortIssued = false;
    m_mode = Mode::Demo;
    m_sessionState = MriSdkSessionState::Unloaded;
}

bool MriSdkLoader::isLoaded() const
{
    return m_mode == Mode::Real && m_handle;
}

MriSdkSessionState MriSdkLoader::sessionState() const
{
    return m_sessionState;
}

MriSdkLoader::Mode MriSdkLoader::mode() const
{
    return m_mode;
}

QString MriSdkLoader::lastError() const
{
    return m_error;
}

QString MriSdkLoader::dllPath() const
{
    return m_dllPath;
}

MriSdkResult MriSdkLoader::initialize(const MriSdkConfig& config)
{
    if (!isLoaded()) {
        setError(QStringLiteral("SDK 尚未成功加载"));
        m_sessionState = MriSdkSessionState::Fault;
        return MriSdkResult::failure(QStringLiteral("initialize"), QStringLiteral("precondition"), -1, m_error);
    }

    const QFileInfo initFile(config.initPath);
    if (!initFile.isFile()) {
        setError(QStringLiteral("初始化文件不存在：%1").arg(config.initPath));
        m_sessionState = MriSdkSessionState::Fault;
        return MriSdkResult::failure(QStringLiteral("initialize"), QStringLiteral("Init"), -1, m_error);
    }
    const QFileInfo parameterFile(config.parameterPath);
    if (!parameterFile.isFile()) {
        setError(QStringLiteral("参数文件不存在：%1").arg(config.parameterPath));
        m_sessionState = MriSdkSessionState::Fault;
        return MriSdkResult::failure(QStringLiteral("initialize"), QStringLiteral("SetParameterFile"), -1, m_error);
    }
    if (!QDir(config.outputPath).exists()) {
        setError(QStringLiteral("输出目录不存在：%1").arg(config.outputPath));
        m_sessionState = MriSdkSessionState::Fault;
        return MriSdkResult::failure(QStringLiteral("initialize"), QStringLiteral("SetOutputPath"), -1, m_error);
    }

    m_sessionState = MriSdkSessionState::Initializing;
    m_error.clear();
    m_config = config;

    auto failure = [this](const QString& functionName, int code) {
        setError(QStringLiteral("%1 失败，返回码 %2").arg(functionName).arg(code));
        if (m_systemOpen && m_closeSys) {
            m_closeSys();
            m_systemOpen = false;
        }
        m_sessionState = MriSdkSessionState::Fault;
        return MriSdkResult::failure(QStringLiteral("initialize"), functionName, code, m_error);
    };
    auto checked = [&failure](const QString& functionName, int code) -> MriSdkResult {
        return code == 0 ? MriSdkResult::success(QStringLiteral("initialize")) : failure(functionName, code);
    };

    // The vendor DLL derives sibling hardware files from these strings and only
    // recognizes native Windows separators. QFileInfo normally returns '/'.
    const QByteArray initPath = QDir::toNativeSeparators(
        QFileInfo(config.initPath).absoluteFilePath()).toLocal8Bit();
    const QByteArray outputPath = QDir::toNativeSeparators(
        QDir(config.outputPath).absolutePath()).toLocal8Bit();
    const QByteArray parameterPath = QDir::toNativeSeparators(
        QFileInfo(config.parameterPath).absoluteFilePath()).toLocal8Bit();

    int code = m_init(initPath.constData());
    if (code != 0) {
        return failure(QStringLiteral("Init"), code);
    }
    m_systemOpen = true;

    MriSdkResult step = checked(QStringLiteral("ConfigFile"), m_configFile(initPath.constData()));
    if (!step.ok) return step;
    step = checked(QStringLiteral("SetOutputPath"), m_setOutputPath(outputPath.constData()));
    if (!step.ok) return step;
    step = checked(QStringLiteral("SetChannelValid"), m_setChannelValid("1"));
    if (!step.ok) return step;
    step = checked(QStringLiteral("SetOutputPrefix"), m_setOutputPrefix(config.outputPrefix.constData()));
    if (!step.ok) return step;
    m_setSaveMode(1);
    step = checked(QStringLiteral("SetParameterFile"), m_setParameterFile(parameterPath.constData(), false));
    if (!step.ok) return step;
    m_setSystemSel(config.systemSelection);
    step = checked(QStringLiteral("SetAllPreempValue"), m_setAllPreempValue());
    if (!step.ok) return step;
    step = checked(QStringLiteral("SetAllGraAnalogDelay"), m_setAllGraAnalogDelay());
    if (!step.ok) return step;
    step = checked(QStringLiteral("SetSingleGraGmax"), m_setSingleGraGmax(0, 2240.0f));
    if (!step.ok) return step;
    step = checked(QStringLiteral("SetSingleGraGmax"), m_setSingleGraGmax(1, 2080.0f));
    if (!step.ok) return step;
    step = checked(QStringLiteral("SetSingleGraGmax"), m_setSingleGraGmax(2, 2980.0f));
    if (!step.ok) return step;
    code = m_setPreempCross(1);
    if (code != 0 && code != 1) return failure(QStringLiteral("SetPreempCross"), code);
    step = checked(QStringLiteral("SetPreempValue"), m_setPreempValue(0, 6, 200.0f));
    if (!step.ok) return step;
    step = checked(QStringLiteral("SetPreempValue"), m_setPreempValue(0, 7, 500.0f));
    if (!step.ok) return step;
    step = checked(QStringLiteral("SetPreempValue"), m_setPreempValue(0, 8, 800.0f));
    if (!step.ok) return step;
    step = checked(QStringLiteral("SetPreempValue"), m_setPreempValue(0, 9, 1000.0f));
    if (!step.ok) return step;

    m_abortIssued = false;
    m_sessionState = MriSdkSessionState::Ready;
    return MriSdkResult::success(QStringLiteral("initialize"));
}

MriSdkResult MriSdkLoader::prepareScan()
{
    if (m_sessionState != MriSdkSessionState::Ready) {
        setError(QStringLiteral("设备未处于可扫描状态"));
        return MriSdkResult::failure(QStringLiteral("prepare"), QStringLiteral("precondition"), -1, m_error);
    }

    const QByteArray parameterPath = QDir::toNativeSeparators(
        QFileInfo(m_config.parameterPath).absoluteFilePath()).toLocal8Bit();
    int code = m_setParameterFile(parameterPath.constData(), false);
    if (code != 0) {
        setError(QStringLiteral("SetParameterFile 失败，返回码 %1").arg(code));
        m_sessionState = MriSdkSessionState::Fault;
        return MriSdkResult::failure(QStringLiteral("prepare"), QStringLiteral("SetParameterFile"), code, m_error);
    }
    code = m_setChannelValid("1");
    if (code != 0) {
        setError(QStringLiteral("SetChannelValid 失败，返回码 %1").arg(code));
        m_sessionState = MriSdkSessionState::Fault;
        return MriSdkResult::failure(QStringLiteral("prepare"), QStringLiteral("SetChannelValid"), code, m_error);
    }
    return MriSdkResult::success(QStringLiteral("prepare"));
}

bool MriSdkLoader::initialize(const QString& initPath, const QString& outputPath, const QString& parPath, bool saveMode)
{
    Q_UNUSED(saveMode);
    MriSdkConfig config;
    config.initPath = initPath;
    config.outputPath = outputPath;
    config.parameterPath = parPath;
    return initialize(config).ok;
}

void MriSdkLoader::shutdown()
{
    if (m_mode == Mode::Real && m_systemOpen && m_closeSys) {
        if ((m_sessionState == MriSdkSessionState::Scanning
                || m_sessionState == MriSdkSessionState::Stopping)
            && m_abort && !m_abortIssued) {
            m_abort();
            m_abortIssued = true;
        }
        m_closeSys();
        m_systemOpen = false;
        m_sessionState = MriSdkSessionState::Closed;
    }
    m_demoScanStatus = 0;
    m_demoScanCompleted = 0;
    m_demoCurrentScanNo = 0;
}

MriSdkStatus MriSdkLoader::status() const
{
    MriSdkStatus value;
    if (!isLoaded()) {
        return value;
    }
    value.connection = m_getConnectStatus ? m_getConnectStatus(0) : 0;
    value.temperature = m_getTemperature ? m_getTemperature() : 0.0;
    value.scan = m_scanStatus ? m_scanStatus() : (m_scanCompleted ? m_scanCompleted() : 0);
    value.currentScan = m_getCurrentScanNo ? m_getCurrentScanNo() : 0;
    value.totalScans = m_getTotalScanNo ? m_getTotalScanNo() : 0;
    return value;
}

int MriSdkLoader::connectStatus(int boxType) const
{
    if (m_mode == Mode::Demo) {
        return 1;
    }
    return m_getConnectStatus ? m_getConnectStatus(boxType) : 0;
}

double MriSdkLoader::temperature() const
{
    if (m_mode == Mode::Demo) {
        return 31.4;
    }
    return m_getTemperature ? m_getTemperature() : 0.0;
}

int MriSdkLoader::scanStatus() const
{
    if (m_mode == Mode::Demo) {
        return m_demoScanStatus;
    }
    return m_scanStatus ? m_scanStatus() : 0;
}

int MriSdkLoader::scanCompleted() const
{
    if (m_mode == Mode::Demo) {
        return m_demoScanCompleted;
    }
    return m_scanCompleted ? m_scanCompleted() : 0;
}

int MriSdkLoader::totalScanNo() const
{
    if (m_mode == Mode::Demo) {
        return m_demoTotalScanNo;
    }
    return m_getTotalScanNo ? m_getTotalScanNo() : 0;
}

int MriSdkLoader::currentScanNo() const
{
    if (m_mode == Mode::Demo) {
        return m_demoCurrentScanNo;
    }
    return m_getCurrentScanNo ? m_getCurrentScanNo() : 0;
}

int MriSdkLoader::prepareForScene(const SceneTemplate& scene)
{
    Q_UNUSED(scene);
    if (m_mode == Mode::Demo) {
        return 0;
    }
    if (!m_setSystemSel || !m_setChannelValid || !m_setParameter || !m_setTxCenterFre || !m_setChannelValue || !m_saveParameterFile) {
        setError(QStringLiteral("SDK 函数未完成绑定"));
        return -1;
    }

    m_setSystemSel(3);
    m_setChannelValid("255");
    m_setParameter("viewBlock", 1.0);
    m_setParameter("TR", 500.0);
    for (int i = 0; i < 8; ++i) {
        m_setTxCenterFre(0, 4, i, 50.0);
    }
    m_setChannelValue(0, 1000.0f);
    m_setChannelValue(1, 1000.0f);
    m_setChannelValue(2, 1000.0f);
    m_saveParameterFile("scenario_nmr.par");
    return 0;
}

int MriSdkLoader::run()
{
    if (m_mode == Mode::Demo) {
        m_demoScanStatus = 1;
        m_demoScanCompleted = 1;
        m_demoCurrentScanNo = 1;
        return 0;
    }
    const int code = m_run ? m_run() : -1;
    if (code == 0) {
        m_abortIssued = false;
        m_sessionState = MriSdkSessionState::Scanning;
    }
    return code;
}

void MriSdkLoader::abort()
{
    if (m_mode == Mode::Demo) {
        m_demoScanStatus = 0;
        m_demoScanCompleted = 0;
        m_demoCurrentScanNo = 0;
        return;
    }
    if (m_abort && !m_abortIssued) {
        m_abort();
        m_abortIssued = true;
        m_sessionState = MriSdkSessionState::Stopping;
    }
}

void MriSdkLoader::markScanFinished()
{
    if (m_mode == Mode::Real && m_systemOpen) {
        m_abortIssued = false;
        m_sessionState = MriSdkSessionState::Ready;
    }
}

MriSdkLoader::FrameData MriSdkLoader::fetchDemoFrame(const SceneTemplate& scene) const
{
    FrameData frame;
    frame.kind = QStringLiteral("IMAGE");
    frame.samples.reserve(128);
    for (int i = 0; i < 128; ++i) {
        const double t = i / 12.0;
        const double value = qAbs(qSin(t) * 140.0 + qCos(t * 0.7) * 55.0 + scene.snr.toDouble());
        frame.samples.push_back(value);
    }
    return frame;
}

template <typename T>
bool MriSdkLoader::bind(const char* name, T& target)
{
#ifdef Q_OS_WIN
    target = reinterpret_cast<T>(GetProcAddress(static_cast<HMODULE>(m_handle), name));
    if (!target) {
        setError(QStringLiteral("绑定函数失败：%1").arg(QString::fromLatin1(name)));
        return false;
    }
    return true;
#else
    Q_UNUSED(name);
    Q_UNUSED(target);
    return false;
#endif
}

bool MriSdkLoader::bindAll()
{
    const bool required = bind("Init", m_init)
        && bind("ConfigFile", m_configFile)
        && bind("SetOutputPath", m_setOutputPath)
        && bind("SetParameterFile", m_setParameterFile)
        && bind("SetSaveMode", m_setSaveMode)
        && bind("SetSystemSel", m_setSystemSel)
        && bind("SetChannelValid", m_setChannelValid)
        && bind("SetParameter", m_setParameter)
        && bind("SetTxCenterFre", m_setTxCenterFre)
        && bind("SetChannelValue", m_setChannelValue)
        && bind("SaveParameterFile", m_saveParameterFile)
        && bind("SetOutputPrefix", m_setOutputPrefix)
        && bind("SetAllPreempValue", m_setAllPreempValue)
        && bind("SetAllGraAnalogDelay", m_setAllGraAnalogDelay)
        && bind("SetSingleGraGmax", m_setSingleGraGmax)
        && bind("SetPreempCross", m_setPreempCross)
        && bind("SetPreempValue", m_setPreempValue)
        && bind("Run", m_run)
        && bind("Abort", m_abort)
        && bind("CloseSys", m_closeSys)
        && bind("GetTotalScanNo", m_getTotalScanNo)
        && bind("GetCurrentScanNo", m_getCurrentScanNo)
        && bind("GetTemperature", m_getTemperature)
        && bind("GetConnectStatus", m_getConnectStatus);
    if (!required) {
        return false;
    }

#ifdef Q_OS_WIN
    m_scanStatus = reinterpret_cast<ScanStatusFunc>(GetProcAddress(static_cast<HMODULE>(m_handle), "ScanStatus"));
    m_scanCompleted = reinterpret_cast<ScanCompletedFunc>(GetProcAddress(static_cast<HMODULE>(m_handle), "ScanCompleted"));
    if (!m_scanStatus && !m_scanCompleted) {
        setError(QStringLiteral("绑定函数失败：ScanStatus / ScanCompleted"));
        return false;
    }
    return true;
#else
    return false;
#endif
}

void MriSdkLoader::setError(const QString& error)
{
    m_error = error;
}

void MriSdkLoader::applyDemoState()
{
    m_demoScanStatus = 0;
    m_demoScanCompleted = 0;
    m_demoTotalScanNo = 8;
    m_demoCurrentScanNo = 0;
}

QString MriSdkLoader::defaultInitPath() const
{
    return QStringLiteral("Iface/mriRely/hw_cfg/init.ini");
}

QString MriSdkLoader::defaultOutputPath() const
{
    return QStringLiteral("demo_output");
}

QString MriSdkLoader::defaultParPath() const
{
    return QStringLiteral("Iface/mriRely/par0423.par");
}
