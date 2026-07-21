#include "MriSdkLoader.h"

#include <QFileInfo>
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

bool MriSdkLoader::load(const QString& dllPath)
{
    unload();
    m_error.clear();
    m_dllPath = dllPath;
    if (dllPath.isEmpty()) {
        m_mode = Mode::Demo;
        applyDemoState();
        return true;
    }

#ifdef Q_OS_WIN
    const QByteArray utf8 = QFileInfo(dllPath).absoluteFilePath().toLocal8Bit();
    m_handle = reinterpret_cast<void*>(LoadLibraryA(utf8.constData()));
    if (!m_handle) {
        setError(QStringLiteral("加载 mridll.dll 失败：%1").arg(dllPath));
        m_mode = Mode::Demo;
        applyDemoState();
        return true;
    }
    if (!bindAll()) {
        unload();
        m_mode = Mode::Demo;
        applyDemoState();
        return true;
    }
    m_mode = Mode::Real;
    return true;
#else
    setError(QStringLiteral("当前平台不支持 Windows DLL 加载"));
    m_mode = Mode::Demo;
    applyDemoState();
    return false;
#endif
}

void MriSdkLoader::unload()
{
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
    m_run = nullptr;
    m_abort = nullptr;
    m_closeSys = nullptr;
    m_scanStatus = nullptr;
    m_scanCompleted = nullptr;
    m_getTotalScanNo = nullptr;
    m_getCurrentScanNo = nullptr;
    m_getTemperature = nullptr;
    m_getConnectStatus = nullptr;
}

bool MriSdkLoader::isLoaded() const
{
    return m_mode == Mode::Real && m_handle;
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

bool MriSdkLoader::initialize(const QString& initPath, const QString& outputPath, const QString& parPath, bool saveMode)
{
    if (m_mode == Mode::Demo) {
        m_demoScanStatus = 0;
        m_demoScanCompleted = 0;
        m_demoCurrentScanNo = 0;
        m_demoTotalScanNo = 8;
        return true;
    }

    if (!m_init || !m_configFile || !m_setOutputPath || !m_setParameterFile || !m_setSaveMode) {
        setError(QStringLiteral("SDK 函数未完成绑定"));
        return false;
    }

    if (m_init(initPath.toLocal8Bit().constData()) != 0) {
        setError(QStringLiteral("Init 失败"));
        return false;
    }
    if (m_configFile(initPath.toLocal8Bit().constData()) != 0) {
        setError(QStringLiteral("ConfigFile 失败"));
        return false;
    }
    if (m_setOutputPath(outputPath.toLocal8Bit().constData()) != 0) {
        setError(QStringLiteral("SetOutputPath 失败"));
        return false;
    }
    if (m_setParameterFile(parPath.toLocal8Bit().constData(), false) != 0) {
        setError(QStringLiteral("SetParameterFile 失败"));
        return false;
    }
    m_setSaveMode(saveMode ? 1 : 0);
    return true;
}

void MriSdkLoader::shutdown()
{
    if (m_mode == Mode::Real && m_closeSys) {
        m_abort();
        m_closeSys();
    }
    m_demoScanStatus = 0;
    m_demoScanCompleted = 0;
    m_demoCurrentScanNo = 0;
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
    return m_run ? m_run() : -1;
}

void MriSdkLoader::abort()
{
    if (m_mode == Mode::Demo) {
        m_demoScanStatus = 0;
        m_demoScanCompleted = 0;
        m_demoCurrentScanNo = 0;
        return;
    }
    if (m_abort) {
        m_abort();
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
