#include "src/game/capture/Dx11HookCapture.h"

#include <windows.h>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "src/core/AppPaths.h"
#include "src/core/Logger.h"
#include "src/core/SettingManager.h"
#include "src/game/capture/Dx11CaptureShared.h"

namespace capture {

namespace {

// 读取共享内存中当前帧的序号（共享内存不可用时返回 0）
uint32_t readDx11SharedSequence()
{
    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, DX11_SHARED_NAME);
    if (!mapping) return 0;

    uint32_t seq = 0;
    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(Dx11CaptureShared));
    if (view) {
        const Dx11CaptureShared* hdr = static_cast<const Dx11CaptureShared*>(view);
        if (hdr->magic == DX11_SHARED_MAGIC && hdr->version == DX11_SHARED_VERSION) {
            seq = hdr->sequence;
        }
        UnmapViewOfFile(view);
    }
    CloseHandle(mapping);
    return seq;
}

// 从 DLL 写入的共享内存中直接读取最新一帧（BGRA），转换为 BGR 的 cv::Mat。
// 无需 PNG 落盘/解码中转。
bool readDx11SharedCapture(cv::Mat& outImg)
{
    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, DX11_SHARED_NAME);
    if (!mapping) {
        qWarning() << "无法打开截图共享内存 (错误码:" << GetLastError() << ")";
        return false;
    }

    bool ok = false;
    uint32_t width = 0, height = 0, dataSize = 0;

    // 先映射头部获取尺寸
    void* headView = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(Dx11CaptureShared));
    if (headView) {
        const Dx11CaptureShared* hdr = static_cast<const Dx11CaptureShared*>(headView);
        if (hdr->magic == DX11_SHARED_MAGIC && hdr->version == DX11_SHARED_VERSION &&
            hdr->status == 0 && hdr->channels == 4 &&
            hdr->width > 0 && hdr->height > 0 &&
            hdr->dataSize == hdr->width * hdr->height * 4) {
            width = hdr->width;
            height = hdr->height;
            dataSize = hdr->dataSize;
        } else {
            qWarning() << "截图共享内存头部无效 (magic/version/status 不匹配)";
        }
        UnmapViewOfFile(headView);
    }

    if (width > 0) {
        void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0,
                                   sizeof(Dx11CaptureShared) + dataSize);
        if (view) {
            const uchar* pixels = static_cast<const uchar*>(view) + sizeof(Dx11CaptureShared);
            const cv::Mat bgra(static_cast<int>(height), static_cast<int>(width),
                               CV_8UC4, const_cast<uchar*>(pixels));
            // cvtColor 会分配新内存，outImg 不再引用共享内存
            cv::cvtColor(bgra, outImg, cv::COLOR_BGRA2BGR);
            ok = !outImg.empty();
            UnmapViewOfFile(view);
        } else {
            qWarning() << "映射截图共享内存失败 (错误码:" << GetLastError() << ")";
        }
    }

    CloseHandle(mapping);
    return ok;
}

// 检查 remote_capture_call.exe 与 hook DLL 是否就绪
bool checkHookToolsExist()
{
    const QString remoteCaptureExe = AppPaths::instance().remoteCaptureExePath();
    const QString dllPath = AppPaths::instance().dx11HookDllPath();

    if (!QFile::exists(remoteCaptureExe)) {
        qWarning() << "remote_capture_call.exe 不存在:" << remoteCaptureExe;
        return false;
    }

    if (!QFile::exists(dllPath)) {
        qWarning() << "DLL 文件不存在:" << dllPath;
        return false;
    }
    return true;
}

} // namespace

bool captureByDllInjection(const QString& targetPid, cv::Mat& winImg)
{
    QString DX11_CAPTURE_PATH = AppPaths::instance().dx11CapturePath();
    QString DX11_LOG_PATH = AppPaths::instance().dx11LogPath();
    QString DX11_HOOK_DLL_PATH = AppPaths::instance().dx11HookDllPath();
    QString DX11_HOOK_DLL_NAME = AppPaths::instance().dx11HookDllName();
    QString remoteCaptureExe = AppPaths::instance().remoteCaptureExePath();

    // 确保目录存在
    QDir logDir = QFileInfo(DX11_LOG_PATH).absoluteDir();
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    QDir captureDir = QFileInfo(DX11_CAPTURE_PATH).absoluteDir();
    if (!captureDir.exists()) {
        captureDir.mkpath(".");
    }

    if (targetPid.isEmpty()) {
        qWarning() << "无法获取目标进程ID";
        return false;
    }

    if (!checkHookToolsExist()) {
        return false;
    }

    // 是否需要把截图额外持久化为 PNG 文件（开关，默认开）。
    // 关闭时向 DLL 传入哨兵路径，DLL 只写共享内存、不落盘。
    const bool persist = SETTING_CONFIG.getPersistScreenshot();
    const QString capturePathArg = persist ? DX11_CAPTURE_PATH : QStringLiteral("__NO_FILE__");

    // 记录截图前的共享内存序号，用于确认拿到的是本次写入的新帧。
    const uint32_t prevSeq = readDx11SharedSequence();

    QProcess process;
    QStringList arguments;
    arguments << "-capture"
              << targetPid
              << DX11_HOOK_DLL_PATH
              << DX11_HOOK_DLL_NAME
              << capturePathArg;

    process.start(remoteCaptureExe, arguments);

    if (!process.waitForFinished(3000)) {
        qWarning() << "截图命令执行超时";
        process.kill();
        return false;
    }

    // 主路径：直接从共享内存读取像素数据，无需 PNG 落盘/解码中转。
    // 若渲染线程刚出新帧但序号尚未刷新，短暂轮询等待。
    for (int i = 0; i < 20; ++i) {
        if (readDx11SharedSequence() != prevSeq) break;
        QThread::msleep(2);
    }

    if (readDx11SharedCapture(winImg)) {
        Logger::log(QString("截图成功(共享内存)，图像尺寸: %1 x %2  通道数: %3")
                        .arg(winImg.cols).arg(winImg.rows).arg(winImg.channels()));
        return true;
    }

    // 回退：共享内存不可用时，若开启了持久化则尝试读取 PNG 文件。
    if (persist && QFile::exists(DX11_CAPTURE_PATH)) {
        winImg = cv::imread(DX11_CAPTURE_PATH.toStdString());
        if (!winImg.empty()) {
            Logger::log(QString("截图成功(文件回退)，图像尺寸: %1 x %2  通道数: %3")
                            .arg(winImg.cols).arg(winImg.rows).arg(winImg.channels()));
            return true;
        }
        qWarning() << "无法读取截图文件:" << DX11_CAPTURE_PATH;
    }

    Logger::log(QString("截图失败：共享内存与文件均不可用"));
    return false;
}

bool dllSetLogPath(const QString& targetPid)
{
    QString DX11_LOG_PATH = AppPaths::instance().dx11LogPath();
    QString DX11_HOOK_DLL_PATH = AppPaths::instance().dx11HookDllPath();
    QString DX11_HOOK_DLL_NAME = AppPaths::instance().dx11HookDllName();
    QString remoteCaptureExe = AppPaths::instance().remoteCaptureExePath();

    QDir logDir = QFileInfo(DX11_LOG_PATH).absoluteDir();
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    if (targetPid.isEmpty()) {
        qWarning() << "无法获取目标进程ID";
        return false;
    }

    if (!checkHookToolsExist()) {
        return false;
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);

    QStringList arguments;
    arguments << "-log"
              << targetPid
              << DX11_HOOK_DLL_PATH
              << DX11_HOOK_DLL_NAME
              << DX11_LOG_PATH;

    qDebug() << "执行修改log地址命令:" << remoteCaptureExe << arguments;

    process.start(remoteCaptureExe, arguments);

    bool finished = false;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QObject::connect(&process, &QProcess::finished, [&](int, QProcess::ExitStatus) {
        finished = true;
    });

    QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
        if (!finished) {
            process.kill();
            process.waitForFinished(1000);
        }
    });

    timeoutTimer.start(5000);

    // 等待进程完成，但允许处理其他事件
    while (!finished && process.state() == QProcess::Running) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }

    bool success = false;
    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        QByteArray output = process.readAll();
        qDebug() << "修改log地址命令输出:" << output;
        success = true;
    } else {
        QByteArray output = process.readAll();
        qWarning() << "修改log地址命令执行失败，退出码:" << process.exitCode();
        qWarning() << "输出:" << output;
    }

    return success;
}

bool dllStopHook(const QString& targetPid)
{
    QString DX11_HOOK_DLL_PATH = AppPaths::instance().dx11HookDllPath();
    QString DX11_HOOK_DLL_NAME = AppPaths::instance().dx11HookDllName();
    QString remoteCaptureExe = AppPaths::instance().remoteCaptureExePath();

    if (targetPid.isEmpty()) {
        qWarning() << "无法获取目标进程ID";
        return false;
    }

    if (!checkHookToolsExist()) {
        return false;
    }

    QProcess process;
    QStringList arguments;
    arguments << "-stop"
              << targetPid
              << DX11_HOOK_DLL_PATH
              << DX11_HOOK_DLL_NAME;

    qDebug() << "执行停止dx11_hook命令:" << remoteCaptureExe << arguments;

    process.start(remoteCaptureExe, arguments);

    if (!process.waitForFinished(10000)) {
        qWarning() << "停止dx11_hook命令执行超时";
        process.kill();
        return false;
    }

    int exitCode = process.exitCode();
    QByteArray output = process.readAllStandardOutput();
    QByteArray errorOutput = process.readAllStandardError();

    if (exitCode != 0) {
        qWarning() << "停止dx11_hook命令执行失败，退出码:" << exitCode;
        qWarning() << "错误输出:" << errorOutput;
        return false;
    }

    qDebug() << "停止dx11_hook命令输出:" << output;
    return true;
}

} // namespace capture
