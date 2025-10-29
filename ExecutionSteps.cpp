//
// Created by CZY on 2025/9/25.
//

#include "QThread"
#include "ExecutionSteps.h"
#include <bemapiset.h>
#include <iostream>
#include <QProcess>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <dwmapi.h>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <QDir>
#include "QTemporaryFile"
#include <QTimer>
#include <random>
#include <src/widget/main/mainwindow.h>
#include "Logger.h"
#include "SettingManager.h"
#include "ConfigManager.h"
#include "src/utils/ClassNameCache.h"
#include "src/utils/MouseSimulator.h"
#include "src/utils/YOLODetector.h"

ExecutionSteps& ExecutionSteps::getInstance() {
    static ExecutionSteps instance; // C++11 保证线程安全
    return instance;
}

void ExecutionSteps::processAndShowImage(const QString& imagePath)
{
    // 处理完成后，发射信号通知mainwindow显示图像
    emit requestShowImage(imagePath);
}

bool ExecutionSteps::checkHWNDHandle() {
    DWORD pid = findPidByProcessName(target);
    if (pid == 0) {
        Logger::log(QString("找不到桌面版游戏进程： %1").arg(target));
        return false;
    }
    hwnd = findWindowByPid(pid);

    if (!hwnd) {
        Logger::log(QString("阴阳师游戏窗口未找到"));
        return false;
    }
    return true;
}

// --- Utilities: 从进程名称查找顶级窗口 ---
DWORD ExecutionSteps::findPidByProcessName(const std::string& procName) {
    // 通过Toolhelp32Snapshot迭代进程
    PROCESSENTRY32 pe = { 0 };
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    if (Process32First(snap, &pe)) {
        do {
            // 不区分大小写比较
            std::wstring exe(pe.szExeFile);  // 用宽字符字符串
            std::wstring lowerExe = exe;
            std::transform(lowerExe.begin(), lowerExe.end(), lowerExe.begin(), ::towlower);

            std::wstring lowerWanted(procName.begin(), procName.end()); // 如果 procName 是 std::string，需要转换
            std::transform(lowerWanted.begin(), lowerWanted.end(), lowerWanted.begin(), ::towlower);

            if (lowerExe == lowerWanted) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

HWND ExecutionSteps::findWindowByPid(DWORD pid) {
    struct Ctx { DWORD pid; HWND res = nullptr; };
    Ctx ctx{pid};

    auto enumProc = [](HWND hwnd, LPARAM lparam) -> BOOL {
        Ctx* p = (Ctx*)lparam;
        DWORD wpid = 0;
        GetWindowThreadProcessId(hwnd, &wpid);
        if (wpid != p->pid) return TRUE;
        // 跳过不可见或子窗口；我们希望顶层可见
        if (!IsWindowVisible(hwnd)) return TRUE;
        // 确保它有标题
        wchar_t buf[256] = {};
        GetWindowTextW(hwnd, buf, sizeof(buf)/sizeof(wchar_t));
        if (wcslen(buf) == 0) return TRUE;
        p->res = hwnd;
        return FALSE;
    };

    EnumWindows(enumProc, (LPARAM)&ctx);
    return ctx.res;
}

QString ExecutionSteps::getTargetProcessId()
{
    // 如果 hwnd 为空，返回空字符串
    if (hwnd == nullptr) {
        qWarning() << "窗口句柄为空，无法获取进程ID";
        return QString();
    }

    // 检查窗口是否有效
    if (!IsWindow(hwnd)) {
        qWarning() << "窗口句柄无效";
        hwnd = nullptr; // 重置为nullptr
        return QString();
    }

    DWORD processId = 0;
    DWORD threadId = GetWindowThreadProcessId(hwnd, &processId);

    if (processId == 0) {
        qWarning() << "无法获取窗口进程ID，错误代码:" << GetLastError();
        return QString();
    }

    return QString::number(processId);
}

//使用PrintWindo截图
bool ExecutionSteps::getOnmyojiCaptureByPrintWindow(cv::Mat& winImg)
{
    bool ok = false;
    for (int i = 0; i < 5; ++i)
    {
        ok = captureWindowToMat(hwnd, winImg);
        if (!ok || winImg.empty()) {
            Logger::log(QString("未能捕获窗口。它是最小化的还是受保护的？1秒内重试..."));
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    if (!ok)
    {
        Logger::log(QString("5次重试未能捕获窗口。任务结束"));
    }
    return ok;
}



//使用DX11截图
bool ExecutionSteps::getOnmyojiCaptureByDllInjection(cv::Mat& winImg)
{
    bool ok = false;

    // 定义路径
    QString DX11_CAPTURE_PATH = ConfigManager::instance().dx11CapturePath();
    QString DX11_LOG_PATH = ConfigManager::instance().dx11LogPath();
    QString DX11_HOOK_DLL_PATH = ConfigManager::instance().dx11HookDllPath();
    QString DX11_HOOK_DLL_NAME = ConfigManager::instance().dx11HookDllName();
    QString remoteCaptureExe = ConfigManager::instance().remoteCaptureExePath();

    // 确保目录存在
    QDir logDir = QFileInfo(DX11_LOG_PATH).absoluteDir();
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    QDir captureDir = QFileInfo(DX11_CAPTURE_PATH).absoluteDir();
    if (!captureDir.exists()) {
        captureDir.mkpath(".");
    }

    // 获取目标进程ID（这里需要您提供获取进程ID的方法）
    // 假设您有一个获取目标进程ID的函数或变量
    QString targetPid = getTargetProcessId(); // 您需要实现这个函数

    if (targetPid.isEmpty()) {
        qWarning() << "无法获取目标进程ID";
        return false;
    }

    // 检查 remote_capture_call.exe 是否存在
    if (!QFile::exists(remoteCaptureExe)) {
        qWarning() << "remote_capture_call.exe 不存在:" << remoteCaptureExe;
        return false;
    }

    // 检查 DLL 文件是否存在
    if (!QFile::exists(DX11_HOOK_DLL_PATH)) {
        qWarning() << "DLL 文件不存在:" << DX11_HOOK_DLL_PATH;
        return false;
    }

    // 执行截图命令
    QProcess process;
    QStringList arguments;
    arguments << "-capture"
              << targetPid
              << DX11_HOOK_DLL_PATH
              << DX11_HOOK_DLL_NAME
              << DX11_CAPTURE_PATH;

    // qDebug() << "执行截图命令:" << remoteCaptureExe << arguments;

    process.start(remoteCaptureExe, arguments);

    // 等待命令完成（设置超时时间，比如10秒）
    if (!process.waitForFinished(3000)) {
        qWarning() << "截图命令执行超时";
        process.kill();
        return false;
    }

    // 检查命令执行结果
    int exitCode = process.exitCode();
    QByteArray output = process.readAllStandardOutput();
    QByteArray errorOutput = process.readAllStandardError();

    if (exitCode != 0) {
        qWarning() << "截图命令执行失败，退出码:" << exitCode;
        qWarning() << "错误输出:" << errorOutput;
        return false;
    }

    // qDebug() << "截图命令输出:" << output;

    // 检查截图文件是否存在
    if (!QFile::exists(DX11_CAPTURE_PATH)) {
        Logger::log(QString("截图文件未生成:" + DX11_CAPTURE_PATH));
        return false;
    }

    // 使用 OpenCV 读取截图
    winImg = cv::imread(DX11_CAPTURE_PATH.toStdString());

    if (winImg.empty()) {
        qWarning() << "无法读取截图文件:" << DX11_CAPTURE_PATH;
        return false;
    }
    Logger::log(QString("截图成功，图像尺寸: %1 x %2  通道数: %3").arg(winImg.cols, winImg.rows, winImg.channels()));

    ok = true;
    return ok;
}

//使用DX11截图
bool ExecutionSteps::dllSetLogPath()
{
    bool ok = false;

    // 定义路径
    QString DX11_CAPTURE_PATH = ConfigManager::instance().dx11CapturePath();
    QString DX11_LOG_PATH = ConfigManager::instance().dx11LogPath();
    QString DX11_HOOK_DLL_PATH = ConfigManager::instance().dx11HookDllPath();
    QString DX11_HOOK_DLL_NAME = ConfigManager::instance().dx11HookDllName();
    QString remoteCaptureExe = ConfigManager::instance().remoteCaptureExePath();

    // 确保目录存在
    QDir logDir = QFileInfo(DX11_LOG_PATH).absoluteDir();
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    // 获取目标进程ID（这里需要您提供获取进程ID的方法）
    // 假设您有一个获取目标进程ID的函数或变量
    QString targetPid = getTargetProcessId();

    if (targetPid.isEmpty()) {
        qWarning() << "无法获取目标进程ID";
        return false;
    }

    // 检查 remote_capture_call.exe 是否存在
    if (!QFile::exists(remoteCaptureExe)) {
        qWarning() << "remote_capture_call.exe 不存在:" << remoteCaptureExe;
        return false;
    }

    // 检查 DLL 文件是否存在
    if (!QFile::exists(DX11_HOOK_DLL_PATH)) {
        qWarning() << "DLL 文件不存在:" << DX11_HOOK_DLL_PATH;
        return false;
    }

    // 执行修改log地址命令
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

    // 简化的等待逻辑
    bool finished = false;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    // 使用lambda处理完成和超时
    QObject::connect(&process, &QProcess::finished, [&](int exitCode, QProcess::ExitStatus) {
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

    // 检查结果
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

//使用DX11截图
bool ExecutionSteps::dllStopHook()
{
bool ok = false;

    // 定义路径
    QString DX11_CAPTURE_PATH = ConfigManager::instance().dx11CapturePath();
    QString DX11_LOG_PATH = ConfigManager::instance().dx11LogPath();
    QString DX11_HOOK_DLL_PATH = ConfigManager::instance().dx11HookDllPath();
    QString DX11_HOOK_DLL_NAME = ConfigManager::instance().dx11HookDllName();
    QString remoteCaptureExe = ConfigManager::instance().remoteCaptureExePath();

    // 获取目标进程ID（这里需要您提供获取进程ID的方法）
    // 假设您有一个获取目标进程ID的函数或变量
    QString targetPid = getTargetProcessId(); // 您需要实现这个函数

    if (targetPid.isEmpty()) {
        qWarning() << "无法获取目标进程ID";
        return false;
    }

    // 检查 remote_capture_call.exe 是否存在
    if (!QFile::exists(remoteCaptureExe)) {
        qWarning() << "remote_capture_call.exe 不存在:" << remoteCaptureExe;
        return false;
    }

    // 检查 DLL 文件是否存在
    if (!QFile::exists(DX11_HOOK_DLL_PATH)) {
        qWarning() << "DLL 文件不存在:" << DX11_HOOK_DLL_PATH;
        return false;
    }

    // 执行停止dx11_hook命令
    QProcess process;
    QStringList arguments;
    arguments << "-stop"
              << targetPid
              << DX11_HOOK_DLL_PATH
              << DX11_HOOK_DLL_NAME;

    qDebug() << "执行停止dx11_hook命令:" << remoteCaptureExe << arguments;

    process.start(remoteCaptureExe, arguments);

    // 等待命令完成（设置超时时间，比如10秒）
    if (!process.waitForFinished(10000)) {
        qWarning() << "停止dx11_hook命令执行超时";
        process.kill();
        return false;
    }

    // 检查命令执行结果
    int exitCode = process.exitCode();
    QByteArray output = process.readAllStandardOutput();
    QByteArray errorOutput = process.readAllStandardError();

    if (exitCode != 0) {
        qWarning() << "停止dx11_hook命令执行失败，退出码:" << exitCode;
        qWarning() << "错误输出:" << errorOutput;
        return false;
    }

    qDebug() << "停止dx11_hook命令输出:" << output;

    ok = true;
    return ok;
}

// 提升进程权限
bool EnableDebugPrivilege() {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        CloseHandle(hToken);
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL)) {
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return GetLastError() == ERROR_SUCCESS;
}

QString ExecutionSteps::opencvRecognizesAndClickByBase64(const QString& base64, const double threshold, const bool randomClick)
{
    // 检查 base64 字符串是否有效
    if (base64.isEmpty()) {
        return "错误: base64 图像数据为空";
    }

    // 解码 base64 数据
    QByteArray imageData = QByteArray::fromBase64(base64.toUtf8());
    if (imageData.isEmpty()) {
        return "错误: base64 数据解码失败";
    }

    // 创建临时文件
    QTemporaryFile tempFile;
    tempFile.setFileTemplate("recognizes_template.png"); // 设置临时文件模板
    if (!tempFile.open()) {
        return "错误: 无法创建临时文件";
    }

    // 将解码的数据写入临时文件
    if (tempFile.write(imageData) == -1) {
        tempFile.close();
        return "错误: 无法写入临时文件";
    }
    tempFile.close(); // 关闭文件以确保数据刷新

    QString result = opencvRecognizesAndClick(tempFile.fileName(),threshold,randomClick);
    return result;
}

QString ExecutionSteps::opencvRecognizesAndClick(const QString& templPath, const double threshold, const bool randomClick)
{
    cv::Mat winImg;
    bool hasWinImg = false;

    // 首先尝试提升权限
    if (!EnableDebugPrivilege()) {
        Logger::log(QString("提升调试权限失败!"));
    }

    QString screenshotMode = SETTING_CONFIG.getScreenshotMode();
    // 使用 if-else 替代 switch
    if (screenshotMode == "PrintWindow") {
        hasWinImg = getOnmyojiCaptureByPrintWindow(winImg);
    } else if (screenshotMode == "DirectX截图") {
        hasWinImg = getOnmyojiCaptureByDllInjection(winImg);
    } else {
        Logger::log(QString("无法识别鼠标控制模式，默认使用DirectX截图"));
        // 默认使用dll注入方式
        hasWinImg = getOnmyojiCaptureByDllInjection(winImg);
    }

    if (!hasWinImg)
    {
        Logger::log(QString("游戏画面获取失败，请查看日志！"));
        return nullptr;
    }

    //加载模板文件
    QString tempSavePath = getTemplatePath(templPath, ConfigManager::instance().screenshotPath());
    cv::Mat templ = cv::imread(tempSavePath.toStdString()); //cv::IMREAD_COLOR
    if (templ.empty()) {
        Logger::log("模板图片加载失败: " + tempSavePath);
        return nullptr;
    }else {
        Logger::log("已加载模板图片: " + tempSavePath);
    }

    // 保存原始的图片
    cv::Mat captureImg = winImg.clone();
    // 确保目录存在
    QString saveDir = QCoreApplication::applicationDirPath() + "/src/resource/thumbnail";
    // 保存图片（覆盖保存）
    QString saveCapturePath = saveDir + "/debug_capture_result.png";
    cv::imwrite(saveCapturePath.toStdString(), captureImg);

    // 在窗口图像中查找模板
    double score = 0.0;
    cv::Rect matchRect;
    bool found = findTemplateMultiScaleInMatNMS(winImg, templ, matchRect, score,
                                             0.4, 0.9, 0.1, threshold); // 参数根据需要调整

    if (!found) {
        Logger::log(QString("未找到匹配区域! score=%1").arg(score));
        return nullptr;
    }

    cv::Point clickPt;
    if (randomClick) {
        // 随机点击模式：在匹配区域内随机点击
        clickPt = getRandomPointInRect(matchRect);
        // Logger::log("随机点击点: (" + std::to_string(clickPt.x) + ", " + std::to_string(clickPt.y) + ")");
    } else {
        // 中心点击模式：点击匹配区域的中心点
        clickPt = cv::Point(matchRect.x + matchRect.width / 2,
                           matchRect.y + matchRect.height / 2);
        // Logger::log("中心点击点: (" + std::to_string(clickPt.x) + ", " + std::to_string(clickPt.y) + ")");
    }

    // 将物理坐标转换为逻辑坐标：除以缩放因子
    double scaleFactor = getDPIScalingFactor();
    clickPt.x = static_cast<int>(clickPt.x / scaleFactor);
    clickPt.y = static_cast<int>(clickPt.y / scaleFactor);

    // 保存带识别框和点击位置的图片
    cv::Mat resultImg = winImg.clone();

    // 绘制识别框（绿色）
    cv::rectangle(resultImg, matchRect, cv::Scalar(0, 255, 0), 2);

    // 绘制点击位置（红色圆点）
    cv::circle(resultImg, clickPt, 5, cv::Scalar(0, 0, 255), -1);

    QDir dir(saveDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 保存图片（覆盖保存）
    QString savePath = saveDir + "/debug_match_result.png";
    cv::imwrite(savePath.toStdString(), resultImg);
    // Logger::log("已保存识别结果图片: " + savePath);

    // 现在使用转换后的 clickPt 执行点击操作
    Logger::log(QString("转换后点击点: (%1, %2)").arg(clickPt.x).arg(clickPt.y));
    clickInWindow(clickPt);

    return savePath;
}

// 安全捕获窗口到 cv::Mat - 完全避免AVX2问题
bool ExecutionSteps::captureWindowToMat(HWND hwnd, cv::Mat& outBGR) {
    if (!IsWindow(hwnd)) {
        Logger::log(QString("无效的窗口句柄"));
        return false;
    }

    // 1. 确保窗口可见且在前台
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    Sleep(100); // 给窗口一些时间刷新

    // 2. 获取正确的窗口区域（包括客户区和非客户区）
    RECT rect{};

    // 方法1: 尝试获取扩展窗口边界
    HRESULT (WINAPI *DwmGetWindowAttribute)(HWND, DWORD, PVOID, DWORD) = nullptr;
    HMODULE dwmapi = LoadLibraryA("dwmapi.dll");
    if (dwmapi) {
        DwmGetWindowAttribute = (HRESULT (WINAPI*)(HWND, DWORD, PVOID, DWORD))GetProcAddress(dwmapi, "DwmGetWindowAttribute");
        if (DwmGetWindowAttribute) {
            DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect));
        }
        FreeLibrary(dwmapi);
    }

    // 方法2: 如果上面失败，使用GetWindowRect
    if (rect.right - rect.left <= 0 || rect.bottom - rect.top <= 0) {
        if (!GetWindowRect(hwnd, &rect)) {
            Logger::log(QString("获取窗口矩形失败"));
            return false;
        }
    }

    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    // 确保最小尺寸
    if (w <= 10 || h <= 10) {
        Logger::log("窗口尺寸过小：" + std::to_string(w) + "x" + std::to_string(h));
        return false;
    }

    Logger::log(QString("窗口区域: left=%1, top=%2, right=%3, bottom=%4, size=%5x%6")
            .arg(rect.left)
            .arg(rect.top)
            .arg(rect.right)
            .arg(rect.bottom)
            .arg(w)
            .arg(h));

    // 3. 创建设备上下文
    HDC hWindowDC = GetWindowDC(hwnd);
    if (!hWindowDC) {
        Logger::log(QString("获取窗口DC失败"));
        return false;
    }

    HDC hMemDC = CreateCompatibleDC(hWindowDC);
    if (!hMemDC) {
        Logger::log(QString("创建内存DC失败"));
        ReleaseDC(hwnd, hWindowDC);
        return false;
    }

    // 4. 创建兼容位图
    HBITMAP hBitmap = CreateCompatibleBitmap(hWindowDC, w, h);
    if (!hBitmap) {
        Logger::log(QString("创建兼容位图失败"));
        DeleteDC(hMemDC);
        ReleaseDC(hwnd, hWindowDC);
        return false;
    }

    HGDIOBJ oldBitmap = SelectObject(hMemDC, hBitmap);

    // 5. 捕获窗口内容 - 使用多种方法尝试
    BOOL captureOk = FALSE;

    HMODULE user32 = GetModuleHandleA("user32.dll");

    if (!user32) {
        Logger::log(QString("无法获取user32.dll模块句柄"));
        return FALSE;
    }

    // 方法1: PrintWindow with full content
    BOOL (WINAPI *PrintWindow)(HWND, HDC, UINT) = nullptr;
    PrintWindow = (BOOL (WINAPI*)(HWND, HDC, UINT))GetProcAddress(user32, "PrintWindow");

    if (!PrintWindow) {
        DWORD error = GetLastError();
        Logger::log(QString("获取PrintWindow函数失败，错误代码: %1").arg(error));
        return FALSE;
    }

    // 按兼容性顺序尝试不同的标志
    // 先尝试最兼容的默认方式
    captureOk = PrintWindow(hwnd, hMemDC, 0x0); // 默认方式
    if (!captureOk) {
        DWORD error = GetLastError();
        Logger::log(QString("PrintWindow默认方式失败，错误代码: %1").arg(error));

        // 再尝试PW_CLIENTONLY
        captureOk = PrintWindow(hwnd, hMemDC, 0x1); // PW_CLIENTONLY
        if (!captureOk) {
            error = GetLastError();
            Logger::log(QString("PrintWindow PW_CLIENTONLY失败，错误代码: %1").arg(error));

            // 最后尝试PW_RENDERFULLCONTENT（Windows 8.1+）
            captureOk = PrintWindow(hwnd, hMemDC, 0x2); // PW_RENDERFULLCONTENT
            if (!captureOk) {
                error = GetLastError();
                Logger::log(QString("PrintWindow PW_RENDERFULLCONTENT失败，错误代码: %1").arg(error));
            }
        }
    }

    // 方法2: BitBlt作为备选
    if (!captureOk) {
        Logger::log(QString("使用BitBlt捕获"));
        captureOk = BitBlt(hMemDC, 0, 0, w, h, hWindowDC, 0, 0, SRCCOPY);
    }

    if (!captureOk) {
        Logger::log(QString("所有捕获方法都失败"));
        SelectObject(hMemDC, oldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemDC);
        ReleaseDC(hwnd, hWindowDC);
        return false;
    }

    // 6. 准备BITMAPINFO
    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w;
    bi.biHeight = -h; // top-down
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;

    // 7. 使用安全的方法获取像素数据 - 完全避免OpenCV的AVX2优化
    std::vector<uint8_t> pixelData(w * h * 4);

    int result = GetDIBits(hMemDC, hBitmap, 0, h, pixelData.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    if (!result) {
        Logger::log(QString("GetDIBits失败"));
        SelectObject(hMemDC, oldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemDC);
        ReleaseDC(hwnd, hWindowDC);
        return false;
    }

    // 8. 手动创建BGR图像，完全绕过OpenCV的颜色转换
    outBGR.create(h, w, CV_8UC3);

    // 手动进行BGRA到BGR转换
    for (int y = 0; y < h; ++y) {
        const uint8_t* srcRow = pixelData.data() + y * w * 4;
        uint8_t* dstRow = outBGR.ptr<uint8_t>(y);

        for (int x = 0; x < w; ++x) {
            // BGRA -> BGR (忽略alpha通道)
            dstRow[x * 3 + 0] = srcRow[x * 4 + 0]; // Blue
            dstRow[x * 3 + 1] = srcRow[x * 4 + 1]; // Green
            dstRow[x * 3 + 2] = srcRow[x * 4 + 2]; // Red
        }
    }

    // 9. 清理资源
    SelectObject(hMemDC, oldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(hwnd, hWindowDC);

    // 10. 验证结果
    if (outBGR.empty()) {
        Logger::log(QString("输出图像为空"));
        return false;
    }

    Logger::log(QString("成功捕获完整窗口: %1x%2")
              .arg(outBGR.cols)
              .arg(outBGR.rows));

    return true;
}

// 多尺度模板匹配，返回最优匹配矩形和置信度
bool ExecutionSteps::findTemplateMultiScaleInMatNMS(const cv::Mat& haystack, const cv::Mat& needle,
                                                   cv::Rect& outRect, double& outScore,
                                                   double scaleMin, double scaleMax,
                                                   double scaleStep, double threshold) {
    outScore = -1;

    // 详细检查输入
    if (haystack.empty() || needle.empty()) {
        Logger::log(QString("[ERROR] 输入图像为空"));
        return false;
    }

    if (haystack.cols == 0 || haystack.rows == 0 ||
        needle.cols == 0 || needle.rows == 0) {
        Logger::log(QString("[ERROR] 输入图像尺寸为0"));
        return false;
    }

    // 强制禁用所有优化
    cv::setUseOptimized(false);

    cv::Mat gHay, gNeedle;

    // 安全的图像转换
    try {
        if (haystack.channels() == 3) {
            cv::cvtColor(haystack, gHay, cv::COLOR_BGR2GRAY);
        } else {
            gHay = haystack.clone();
        }

        if (needle.channels() == 3) {
            cv::cvtColor(needle, gNeedle, cv::COLOR_BGR2GRAY);
        } else {
            gNeedle = needle.clone();
        }
    } catch (const cv::Exception& e) {
        Logger::log(QString("[ERROR] 图像转换失败: %1").arg(e.what()));
        return false;
    }

    // 验证转换结果
    if (gHay.empty() || gNeedle.empty()) {
        Logger::log(QString("[ERROR] 灰度图像为空"));
        return false;
    }

    std::vector<cv::Rect> candidateRects;
    std::vector<double> candidateScores;

    // 使用更保守的尺度参数
    for (double s = scaleMin; s <= scaleMax + scaleStep/2; s += scaleStep) {
        try {
            // 计算新尺寸
            int newWidth = std::max(1, static_cast<int>(gNeedle.cols * s));
            int newHeight = std::max(1, static_cast<int>(gNeedle.rows * s));

            // 严格的尺寸检查
            if (newWidth < 10 || newHeight < 10) {
                continue;
            }
            if (newWidth > gHay.cols || newHeight > gHay.rows) {
                continue;
            }

            cv::Mat resizedNeedle;

            // 使用INTER_NEAREST，最安全的插值方法
            cv::resize(gNeedle, resizedNeedle, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_NEAREST);

            if (resizedNeedle.empty()) {
                continue;
            }

            cv::Mat result;

            // 使用try-catch包装可能崩溃的操作
            try {
                cv::matchTemplate(gHay, resizedNeedle, result, cv::TM_CCOEFF_NORMED);
            } catch (...) {
                Logger::log(QString("[WARN] matchTemplate 异常，跳过该尺度"));
                continue;
            }

            if (result.empty()) {
                continue;
            }

            double minVal, maxVal;
            cv::Point minLoc, maxLoc;

            try {
                cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
            } catch (...) {
                Logger::log(QString("[WARN] minMaxLoc 异常，跳过该尺度"));
                continue;
            }

            Logger::log(QString("[OpenCV] 缩放=%1 得分=%2 pos=(%3,%4) size=%5x%6")
                .arg(s).arg(maxVal).arg(maxLoc.x).arg(maxLoc.y)
                .arg(resizedNeedle.cols).arg(resizedNeedle.rows));

            if (maxVal >= threshold) {
                cv::Rect r(maxLoc.x, maxLoc.y, resizedNeedle.cols, resizedNeedle.rows);
                candidateRects.push_back(r);
                candidateScores.push_back(maxVal);
            }

        } catch (const std::exception& e) {
            Logger::log(QString("[WARN] 尺度 %1 处理异常: %2").arg(s).arg(e.what()));
            continue;
        }
    }

    if (candidateRects.empty()) {
        Logger::log(QString("[WARN] 没有找到任何匹配 >= %1").arg(threshold));
        return false;
    }

    // 简单的NMS实现
    std::vector<bool> keep(candidateRects.size(), true);

    for (size_t i = 0; i < candidateRects.size(); ++i) {
        if (!keep[i]) continue;

        for (size_t j = i + 1; j < candidateRects.size(); ++j) {
            if (!keep[j]) continue;

            cv::Rect intersection = candidateRects[i] & candidateRects[j];
            double overlap = intersection.area() / (double)std::min(candidateRects[i].area(), candidateRects[j].area());

            if (overlap > 0.3) {
                // 保留分数更高的
                if (candidateScores[j] > candidateScores[i]) {
                    keep[i] = false;
                } else {
                    keep[j] = false;
                }
            }
        }
    }

    // 收集保留的候选框
    std::vector<cv::Rect> keptRects;
    std::vector<double> keptScores;

    for (size_t i = 0; i < candidateRects.size(); ++i) {
        if (keep[i]) {
            keptRects.push_back(candidateRects[i]);
            keptScores.push_back(candidateScores[i]);
        }
    }

    if (keptScores.empty()) {
        Logger::log(QString("[WARN] NMS后无候选框"));
        return false;
    }

    // 找到最高分
    auto maxIt = std::max_element(keptScores.begin(), keptScores.end());
    int idx = std::distance(keptScores.begin(), maxIt);

    outRect = keptRects[idx];
    outScore = keptScores[idx];

    Logger::log(QString("[RESULT] bestScore=%1 rect=(%2,%3,%4x%5)")
            .arg(outScore)
            .arg(outRect.x)
            .arg(outRect.y)
            .arg(outRect.width)
            .arg(outRect.height));

    return true;
}

// 非极大值抑制合并重叠矩形
std::vector<cv::Rect> ExecutionSteps::nonMaxSuppression(const std::vector<cv::Rect>& rects, float iouThreshold = 0.3f) {
    std::vector<cv::Rect> result;
    std::vector<cv::Rect> sortedRects = rects;

    // 按面积从大到小排序
    std::sort(sortedRects.begin(), sortedRects.end(),
              [](const cv::Rect& a, const cv::Rect& b){ return a.area() > b.area(); });

    std::vector<bool> suppressed(sortedRects.size(), false);

    for (size_t i = 0; i < sortedRects.size(); ++i) {
        if (suppressed[i]) continue;
        result.push_back(sortedRects[i]);
        for (size_t j = i + 1; j < sortedRects.size(); ++j) {
            if (suppressed[j]) continue;
            if (rectIoU(sortedRects[i], sortedRects[j]) > iouThreshold) {
                suppressed[j] = true;
            }
        }
    }
    return result;
}

// 计算两个矩形的交并比
float ExecutionSteps::rectIoU(const cv::Rect& a, const cv::Rect& b) {
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width, b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);

    int w = std::max(0, x2 - x1);
    int h = std::max(0, y2 - y1);
    float inter = w * h;
    float unionArea = a.area() + b.area() - inter;
    return unionArea > 0 ? inter / unionArea : 0.f;
}

cv::Point ExecutionSteps::getRandomPointInRect(const cv::Rect& r) {
    int x = r.x + rand() % r.width;
    int y = r.y + rand() % r.height;
    return cv::Point(x, y);
}

/**
 * 在矩形区域内随机获取一个点，排除指定宽度百分比的垂直区域（相对于识别框）
 * @param r 目标矩形区域
 * @param excludeStartWidth 排除区域起始宽度百分比 (0.0-1.0，相对于识别框)
 * @param excludeEndWidth 排除区域结束宽度百分比 (0.0-1.0，相对于识别框)
 * @param maxAttempts 最大尝试次数，避免无限循环
 * @return 随机点坐标
 */
cv::Point ExecutionSteps::getRandomPointInRectExcludeWidth(const cv::Rect& r,
                                                          double excludeStartWidth, double excludeEndWidth,
                                                          int maxAttempts) {
    // 如果没有指定排除区域，直接随机生成
    if (excludeStartWidth >= excludeEndWidth) {
        int x = r.x + rand() % r.width;
        int y = r.y + rand() % r.height;
        return cv::Point(x, y);
    }

    // 计算排除区域的绝对坐标（相对于识别框）
    int excludeX = r.x + r.width * excludeStartWidth;
    int excludeWidth = r.width * (excludeEndWidth - excludeStartWidth);
    cv::Rect excludeRect(excludeX, r.y, excludeWidth, r.height);

    // 检查目标区域和排除区域是否有重叠（这里应该总是有重叠，因为排除区域在目标区域内）
    cv::Rect intersection = r & excludeRect;
    if (intersection.width <= 0 || intersection.height <= 0) {
        // 没有重叠，直接随机生成
        int x = r.x + rand() % r.width;
        int y = r.y + rand() % r.height;
        return cv::Point(x, y);
    }

    // 有重叠区域，需要避免生成在排除区域内的点c
    int attempts = 0;
    while (attempts < maxAttempts) {
        int x = r.x + rand() % r.width;
        int y = r.y + rand() % r.height;
        cv::Point candidate(x, y);

        // 检查候选点是否在排除区域内
        if (!excludeRect.contains(candidate)) {
            return candidate;
        }

        attempts++;
    }

    // 如果达到最大尝试次数仍未找到合适的点，返回中心点作为备选
    Logger::log(QString("达到最大尝试次数，使用备选中心点"));
    return cv::Point(r.x + r.width / 2, r.y + r.height / 2);
}

bool ExecutionSteps::deleteCaptureFile()
{
    QString DX11_CAPTURE_PATH = ConfigManager::instance().dx11CapturePath();

    QFile file(DX11_CAPTURE_PATH);
    if (file.exists()) {
        if (file.remove()) {
            // qDebug() << "成功删除截图文件:" << DX11_CAPTURE_PATH;
            return true;
        } else {
            qWarning() << "删除截图文件失败:" << DX11_CAPTURE_PATH << "错误:" << file.errorString();
            return false;
        }
    } else {
        // qDebug() << "截图文件不存在，无需删除:" << DX11_CAPTURE_PATH;
        return true; // 文件不存在也算成功
    }
}

void ExecutionSteps::clickInWindow(const cv::Point& clickPoint) {
    MouseSimulator simulator;
    simulator.SetHumanLikeMode(true);
    simulator.SetRandomDelayRange(20, 100);
    simulator.SetJitterLevel(3);

    //日志打印
    RECT clientRect, windowRect;
    GetClientRect(hwnd, &clientRect);
    GetWindowRect(hwnd, &windowRect);
    Logger::log(QString("窗口信息 - 客户区: %1x%2, 窗口: %3x%4")
        .arg(clientRect.right).arg(clientRect.bottom)
        .arg(windowRect.right - windowRect.left).arg(windowRect.bottom - windowRect.top));


    // 关键：将客户区坐标转换为屏幕坐标
    POINT screenPoint = { clickPoint.x, clickPoint.y };
    ClientToScreen(hwnd, &screenPoint);

    Logger::log(QString("坐标转换 - 客户区: (%1, %2) -> 屏幕: (%3, %4)")
        .arg(clickPoint.x).arg(clickPoint.y)
        .arg(screenPoint.x).arg(screenPoint.y));


    bool success = false;
    QString mouseClickMode = SETTING_CONFIG.getMouseClickMode();
    if (mouseClickMode == "PostMessage")
    {
        success = simulator.StealthMessageClick(hwnd,clickPoint.x, clickPoint.y);
    }else if (mouseClickMode == "InputMouse")
    {
        POINT start = MouseSimulator::GetCurrentPosition();
        POINT targetScreen = { screenPoint.x, screenPoint.y };
        success = simulator.ExecuteTrajectoryWithClick(start,targetScreen,TrajectoryType::BEZIER,SETTING_CONFIG.getMouseSpeed() * 10);
    }else
    {
        //其他...
        Logger::log(QString("无法识别的鼠标点击模式，执行默认策略"));
        success = simulator.StealthMessageClick(hwnd,clickPoint.x, clickPoint.y);
    }

    if (success)
    {
        Logger::log(QString("点击成功"));
    }else
    {
        Logger::log(QString("点击失败"));
    }
}

QJsonObject ExecutionSteps::parseOCROutput(const QString& ocrOutput)
{
    // 去除开头的无效信息
    QString jsonStr = ocrOutput;

    // 查找JSON开始位置
    int jsonStart = jsonStr.indexOf('{');
    if (jsonStart == -1) {
        qWarning() << "未找到JSON数据";
        return QJsonObject();
    }

    // 提取JSON部分
    jsonStr = jsonStr.mid(jsonStart);

    // 去除末尾的\r\n
    jsonStr = jsonStr.trimmed();

    // 将十六进制编码的中文字符转换为Unicode
    // 处理 \xE8\x8C\xB6 这种格式的编码
    QString processedStr;
    for (int i = 0; i < jsonStr.length(); ++i) {
        if (jsonStr[i] == '\\' && i + 3 < jsonStr.length() && jsonStr[i+1] == 'x') {
            // 提取十六进制部分
            QString hexStr = jsonStr.mid(i+2, 2);
            bool ok;
            ushort unicodeChar = hexStr.toUShort(&ok, 16);
            if (ok) {
                processedStr += QChar(unicodeChar);
                i += 3; // 跳过 \xXX
            } else {
                processedStr += jsonStr[i];
            }
        } else {
            processedStr += jsonStr[i];
        }
    }

    // 解析JSON
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(processedStr.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON解析错误:" << parseError.errorString();
        qWarning() << "错误位置:" << parseError.offset;
        qWarning() << "处理后的字符串:" << processedStr;
        return QJsonObject();
    }

    if (!jsonDoc.isObject()) {
        qWarning() << "解析结果不是JSON对象";
        return QJsonObject();
    }

    return jsonDoc.object();
}

QJsonObject ExecutionSteps::executeRapidOCR()
{
    QJsonObject result;
    bool ok = false;

    // 定义路径
    QString DX11_CAPTURE_PATH = ConfigManager::instance().dx11CapturePath();
    QString rapidOCRExe = ConfigManager::instance().rapidOCRExePath();
    QString rapidOCRModelsPath = ConfigManager::instance().rapidOCRModelsPath();
    QString rapidOCRDetPath = ConfigManager::instance().rapidOCRDetPathV4();
    QString rapidOCRClsPath = ConfigManager::instance().rapidOCRClsPathV4();
    QString rapidOCRRecPath = ConfigManager::instance().rapidOCRRecPathV4();
    QString rapidOCRKeysPath = ConfigManager::instance().rapidOCRKeysPath();

    QDir captureDir = QFileInfo(DX11_CAPTURE_PATH).absoluteDir();
    if (!captureDir.exists()) {
        captureDir.mkpath(".");
    }

    // 检查 remote_capture_call.exe 是否存在
    if (!QFile::exists(rapidOCRExe)) {
        qWarning() << "rapidOCR-json.exe 不存在:" << rapidOCRExe;
        return result;
    }

    // 执行ocr识别命令
    QProcess process;
    QStringList arguments;
    arguments << ("--image_path=" + DX11_CAPTURE_PATH);
    arguments << ("--models=" + rapidOCRModelsPath);
    arguments << ("--det=" + rapidOCRDetPath);
    arguments << ("--cls=" + rapidOCRClsPath);
    arguments << ("--rec=" + rapidOCRRecPath);
    // arguments << ("--keys=" + rapidOCRKeysPath);

    qDebug() << "执行ocr识别命令:" << rapidOCRExe << arguments;

    process.start(rapidOCRExe, arguments);

    // 等待命令完成（设置超时时间）
    if (!process.waitForFinished(5000)) {
        qWarning() << "ocr识别命令执行超时";
        process.kill();
        return result;
    }

    // 检查命令执行结果
    int exitCode = process.exitCode();
    QByteArray output = process.readAllStandardOutput();
    QByteArray errorOutput = process.readAllStandardError();

    if (exitCode != 0) {
        qWarning() << "ocr识别命令执行失败，退出码:" << exitCode;
        qWarning() << "错误输出:" << errorOutput;
        return result;
    }
    result = parseOCROutput(output);

    return result;
}

QJsonArray ExecutionSteps::ocrRecognizes()
{
    cv::Mat winImg;
    bool hasWinImg = false;

    // 首先尝试提升权限
    if (!EnableDebugPrivilege()) {
        Logger::log(QString("提升调试权限失败!"));
    }

    QString screenshotMode = SETTING_CONFIG.getScreenshotMode();
    // 使用 if-else 替代 switch
    if (screenshotMode == "PrintWindow") {
        hasWinImg = getOnmyojiCaptureByPrintWindow(winImg);
    } else if (screenshotMode == "DirectX截图") {
        hasWinImg = getOnmyojiCaptureByDllInjection(winImg);
    } else {
        Logger::log(QString("无法识别鼠标控制模式，默认使用DirectX截图"));
        // 默认使用dll注入方式
        hasWinImg = getOnmyojiCaptureByDllInjection(winImg);
    }

    if (!hasWinImg)
    {
        Logger::log(QString("游戏画面获取失败，请查看日志！"));
        QJsonArray empty;
        return empty;
    }

    // 保存原始的图片
    cv::Mat captureImg = winImg.clone();
    // 确保目录存在
    QString saveDir = QCoreApplication::applicationDirPath() + "/src/resource/thumbnail";
    // 保存图片（覆盖保存） debug用
    QString saveCapturePath = saveDir + "/debug_capture_result.png";
    cv::imwrite(saveCapturePath.toStdString(), captureImg);

    QJsonObject result = executeRapidOCR();
    QJsonArray dataArray = result["data"].toArray();

    return dataArray;
}

QString ExecutionSteps::ocrRecognizesAndClick(const QString& ocrText, const double threshold, const bool randomClick)
{
    cv::Mat winImg;
    bool hasWinImg = false;
    bool hasOcrText = false;

    // 首先尝试提升权限
    if (!EnableDebugPrivilege()) {
        Logger::log(QString("提升调试权限失败!"));
    }

    QString screenshotMode = SETTING_CONFIG.getScreenshotMode();
    // 使用 if-else 替代 switch
    if (screenshotMode == "PrintWindow") {
        hasWinImg = getOnmyojiCaptureByPrintWindow(winImg);
    } else if (screenshotMode == "DirectX截图") {
        hasWinImg = getOnmyojiCaptureByDllInjection(winImg);
    } else {
        Logger::log(QString("无法识别鼠标控制模式，默认使用DirectX截图"));
        // 默认使用dll注入方式
        hasWinImg = getOnmyojiCaptureByDllInjection(winImg);
    }

    if (!hasWinImg)
    {
        Logger::log(QString("游戏画面获取失败，请查看日志！"));
        return nullptr;
    }

    // 保存原始的图片
    cv::Mat captureImg = winImg.clone();
    // 确保目录存在
    QString saveDir = QCoreApplication::applicationDirPath() + "/src/resource/thumbnail";
    // 保存图片（覆盖保存） debug用
    QString saveCapturePath = saveDir + "/debug_capture_result.png";
    cv::imwrite(saveCapturePath.toStdString(), captureImg);

    QJsonObject result = executeRapidOCR();
    QString savePath;

    //Debug用，后期记得注释掉
    if (!result.isEmpty()) {
        // qDebug() << "解析成功:";
        // qDebug() << "code:" << result["code"].toInt();

        QJsonArray dataArray = result["data"].toArray();

        for (int i = 0; i < dataArray.size(); ++i) {
            QJsonObject item = dataArray[i].toObject();
            QString text = item["text"].toString();
            QJsonArray box = item["box"].toArray();
            double score = item["score"].toDouble();

            if (comparesEqual(text,ocrText))
            {
                hasOcrText = true;
                // qDebug() << "识别到文本" << i << ":" << text << "置信度:" << score << "范围：" << box;
                if (score >= threshold)
                {
                    cv::Rect matchRect;
                    cv::Point clickPt;
                    if (box.size() == 4) {
                        // 提取四个点的坐标
                        QJsonArray point1 = box[0].toArray();
                        QJsonArray point2 = box[1].toArray();
                        QJsonArray point3 = box[2].toArray();
                        QJsonArray point4 = box[3].toArray();

                        int x1 = point1[0].toInt();
                        int y1 = point1[1].toInt();
                        int x2 = point2[0].toInt();
                        int y2 = point2[1].toInt();
                        int x3 = point3[0].toInt();
                        int y3 = point3[1].toInt();
                        int x4 = point4[0].toInt();
                        int y4 = point4[1].toInt();

                        // 计算矩形的最小外接矩形
                        int minX = std::min({x1, x2, x3, x4});
                        int minY = std::min({y1, y2, y3, y4});
                        int maxX = std::max({x1, x2, x3, x4});
                        int maxY = std::max({y1, y2, y3, y4});

                        matchRect = cv::Rect(minX, minY, maxX - minX, maxY - minY);

                        // qDebug() << "转换后的矩形: x=" << matchRect.x << " y=" << matchRect.y
                        //          << " width=" << matchRect.width << " height=" << matchRect.height;
                    } else {
                        qWarning() << "box数组大小不正确，期望4个点，实际:" << box.size();
                        continue;
                    }

                    if (randomClick) {
                        // 随机点击模式：在匹配区域内随机点击
                        clickPt = getRandomPointInRect(matchRect);
                        // Logger::log("随机点击点: (" + std::to_string(clickPt.x) + ", " + std::to_string(clickPt.y) + ")");
                    } else {
                        // 中心点击模式：点击匹配区域的中心点
                        clickPt = cv::Point(matchRect.x + matchRect.width / 2,
                                           matchRect.y + matchRect.height / 2);
                        // Logger::log("中心点击点: (" + std::to_string(clickPt.x) + ", " + std::to_string(clickPt.y) + ")");
                    }

                    // 保存带识别框和点击位置的图片
                    cv::Mat resultImg = winImg.clone();

                    // 绘制识别框（绿色）
                    cv::rectangle(resultImg, matchRect, cv::Scalar(0, 255, 0), 2);

                    // 绘制点击位置（红色圆点）
                    cv::circle(resultImg, clickPt, 5, cv::Scalar(0, 0, 255), -1);

                    QDir dir(saveDir);
                    if (!dir.exists()) {
                        dir.mkpath(".");
                    }

                    // 保存图片（覆盖保存）
                    savePath = saveDir + "/debug_match_result.png";
                    cv::imwrite(savePath.toStdString(), resultImg);

                    clickInWindow(clickPt);
                    break;
                }else
                {
                    Logger::log(QString("[OCR] 已识别到:" + text + " 但分数过低"));
                }
            }
        }

        if (!hasOcrText)
        {
            Logger::log(QString("[OCR] 未识别到文字：" + ocrText));
            // QJsonDocument doc(result);
            // Logger::log("OCR Result JSON:\n" + doc.toJson(QJsonDocument::Indented));
            qDebug() << result;
        }
    }


    return savePath;
}

/**
 *
 * @param threshold 得分
 * @param randomClick 是否随机点击
 * @param targetLabelName 需要识别的标签名称 名称列表参考 :resource/classes.txt，可以看缩略图
 * @param excludeStartWidth 排除区域起始宽度百分比 (0.0-1.0)
 * @param excludeEndWidth 排除区域结束宽度百分比 (0.0-1.0)
 * @return
 */
QString ExecutionSteps::yoloRecognizesAndClick(const double threshold, const bool randomClick, const QString& targetLabelName, double excludeStartWidth, double excludeEndWidth)
{
    cv::Mat winImg;
    bool hasWinImg = false;

    // 首先尝试提升权限
    if (!EnableDebugPrivilege()) {
        Logger::log(QString("提升调试权限失败!"));
    }

    QString screenshotMode = SETTING_CONFIG.getScreenshotMode();
    // 使用 if-else 替代 switch
    if (screenshotMode == "PrintWindow") {
        hasWinImg = getOnmyojiCaptureByPrintWindow(winImg);
    } else if (screenshotMode == "DirectX截图") {
        hasWinImg = getOnmyojiCaptureByDllInjection(winImg);
    } else {
        Logger::log(QString("无法识别鼠标控制模式，默认使用DirectX截图"));
        // 默认使用dll注入方式
        hasWinImg = getOnmyojiCaptureByDllInjection(winImg);
    }

    if (!hasWinImg)
    {
        Logger::log(QString("游戏画面获取失败，请查看日志！"));
        return nullptr;
    }

    // 保存原始的图片
    cv::Mat captureImg = winImg.clone();
    // 确保目录存在
    QString saveDir = QCoreApplication::applicationDirPath() + "/src/resource/thumbnail";
    // 保存图片（覆盖保存） debug用
    QString saveCapturePath = saveDir + "/debug_capture_result.png";
    cv::imwrite(saveCapturePath.toStdString(), captureImg);

    cv::Point clickPt;
    cv::Rect matchRect;
    auto final_detections = YOLODetector::getInstance().detect(captureImg, threshold);

    // 在图像上绘制检测结果
    for (const auto& det : final_detections) {
        QString labelName = ClassNameCache::getClassName(det.class_id);
        cv::rectangle(captureImg, det.bbox, cv::Scalar(0, 255, 0), 2);
        std::string label = labelName.toStdString();
        // std::string label = "Class " + labelName.toStdString() +
        // " Conf: " + std::to_string(det.confidence);
        cv::putText(captureImg, label, cv::Point(det.bbox.x, det.bbox.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

        if (comparesEqual(targetLabelName, labelName))
        {
            //找到需要的目标标签
            matchRect = det.bbox;
            //此处写法默认最后一个 也可视作随机
        }
    }

    if (!matchRect.empty())
    {
        if (randomClick) {
            // 随机点击模式：在匹配区域内随机点击
            clickPt = getRandomPointInRectExcludeWidth(matchRect, excludeStartWidth, excludeEndWidth, 10);
            // Logger::log("随机点击点: (" + std::to_string(clickPt.x) + ", " + std::to_string(clickPt.y) + ")");
        } else {
            // 中心点击模式：点击匹配区域的中心点
            clickPt = cv::Point(matchRect.x + matchRect.width / 2,
                               matchRect.y + matchRect.height / 2);
            // Logger::log("中心点击点: (" + std::to_string(clickPt.x) + ", " + std::to_string(clickPt.y) + ")");
        }

        // 在图像上标记点击点（红色圆点）
        cv::circle(captureImg, clickPt, 5, cv::Scalar(0, 0, 255), -1);

        clickInWindow(clickPt);
    }else
    {
        Logger::log(QString("未识别到指定目标"));
    }

    // 如果指定了排除区域，也在图像上标记出来（蓝色矩形）
    if (excludeStartWidth < excludeEndWidth && matchRect.width > 0) {
        int excludeX = matchRect.x + matchRect.width * excludeStartWidth;
        int excludeWidth = matchRect.width * (excludeEndWidth - excludeStartWidth);
        cv::Rect excludeRect(excludeX, matchRect.y, excludeWidth, matchRect.height);
        cv::rectangle(captureImg, excludeRect, cv::Scalar(255, 0, 0), 2);
        cv::putText(captureImg, "Exclude Area", cv::Point(excludeRect.x, excludeRect.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 1);
    }

    QString savePath = ConfigManager::instance().matchResultPath();
    // 保存识别点击的结果
    cv::imwrite(savePath.toStdString(), captureImg);

    if (matchRect.empty())
    {
        return {};
    }
    return savePath;
}

/**
 * 返回识别数据
 * @param threshold 得分
 * @param excludeStartWidth 排除区域起始宽度百分比 (0.0-1.0)
 * @param excludeEndWidth 排除区域结束宽度百分比 (0.0-1.0)
 * @return
 */
std::vector<Detection> ExecutionSteps::yoloRecognizes(const double threshold, double excludeStartWidth,
                                                      double excludeEndWidth)
{
    cv::Mat winImg;
    bool hasWinImg = false;

    // 首先尝试提升权限
    if (!EnableDebugPrivilege()) {
        Logger::log(QString("提升调试权限失败!"));
    }

    QString screenshotMode = SETTING_CONFIG.getScreenshotMode();
    // 使用 if-else 替代 switch
    if (screenshotMode == "PrintWindow") {
        hasWinImg = getOnmyojiCaptureByPrintWindow(winImg);
    } else if (screenshotMode == "DirectX截图") {
        hasWinImg = getOnmyojiCaptureByDllInjection(winImg);
    } else {
        Logger::log(QString("无法识别鼠标控制模式，默认使用DirectX截图"));
        // 默认使用dll注入方式
        hasWinImg = getOnmyojiCaptureByDllInjection(winImg);
    }

    if (!hasWinImg)
    {
        Logger::log(QString("游戏画面获取失败，请查看日志！"));
        return {};
    }

    // 保存原始的图片
    cv::Mat captureImg = winImg.clone();
    // 确保目录存在
    QString saveDir = QCoreApplication::applicationDirPath() + "/src/resource/thumbnail";
    // 保存图片（覆盖保存） debug用
    QString saveCapturePath = saveDir + "/debug_capture_result.png";
    cv::imwrite(saveCapturePath.toStdString(), captureImg);

    cv::Rect matchRect;
    auto final_detections = YOLODetector::getInstance().detect(captureImg, threshold);

    // 在图像上绘制检测结果
    for (auto& det : final_detections) {
        QString labelName = ClassNameCache::getClassName(det.class_id);
        cv::rectangle(captureImg, det.bbox, cv::Scalar(0, 255, 0), 2);
        std::string label = labelName.toStdString();
        // std::string label = "Class " + labelName.toStdString() +
        // " Conf: " + std::to_string(det.confidence);
        cv::putText(captureImg, label, cv::Point(det.bbox.x, det.bbox.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        det.className = labelName;
    }

    // 如果指定了排除区域，也在图像上标记出来（蓝色矩形）
    if (excludeStartWidth < excludeEndWidth && matchRect.width > 0) {
        int excludeX = matchRect.x + matchRect.width * excludeStartWidth;
        int excludeWidth = matchRect.width * (excludeEndWidth - excludeStartWidth);
        cv::Rect excludeRect(excludeX, matchRect.y, excludeWidth, matchRect.height);
        cv::rectangle(captureImg, excludeRect, cv::Scalar(255, 0, 0), 2);
        cv::putText(captureImg, "Exclude Area", cv::Point(excludeRect.x, excludeRect.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 1);
    }

    QString savePath = ConfigManager::instance().matchResultPath();
    // 保存识别点击的结果
    cv::imwrite(savePath.toStdString(), captureImg);



    return final_detections;
}

// 智能路径处理方法
QString ExecutionSteps::getTemplatePath(const QString& templatePath, const QString& basePath) {
    QFileInfo fileInfo(templatePath);

    // 如果已经是绝对路径，直接返回
    if (fileInfo.isAbsolute()) {
        return templatePath;
    }

    // 如果是相对路径，则拼接基础路径
    return basePath + templatePath;
}

// 获取DPI缩放因子
double ExecutionSteps::getDPIScalingFactor() {
    HDC hdc = GetDC(NULL);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    return dpiX / 96.0; // 标准DPI为96
}

//执行 系统方案-结界突破 完整的逻辑 直到消耗完所有券
void ExecutionSteps::executeBorderBreakthrough()
{
    Logger::log(QString("开始执行结界突破"));

    // 识别当前界面状态
    auto detections = yoloRecognizes(0.5, 0.0, 0.3);

    if (detections.empty()) {
        Logger::log(QString("识别失败，无法继续执行"));
        return;
    }

    // 检查是否需要刷新
    if (hasDetectionWithLabel(detections, "realm_raid-realm-penetrated")) {
        Logger::log(QString("检测到已挑战结界，执行刷新"));

        // 点击刷新按钮
        if (clickDetectionByLabel("common-btn-yellow_confirm", 0.5, 0.0, 0.0)) {
            QThread::msleep(2000); // 等待刷新确认界面出现

            // 点击确认刷新
            if (clickDetectionByLabel("common-btn-yellow_confirm", 0.5, 0.0, 0.0)) {
                Logger::log(QString("刷新成功"));
                QThread::msleep(3000); // 等待刷新完成

                // 刷新后重新执行
                executeBorderBreakthrough(); //此时会进入投4逻辑
                return;
            } else {
                Logger::log(QString("未找到确认刷新按钮"));
            }
        } else {
            Logger::log(QString("未找到刷新按钮"));
        }
        return;
    }

    std::vector<Detection> vec;
    //开始进行投4
    Logger::log(QString("结界突破-开始进行投4"));
    for (const auto& det : detections) {
        if (comparesEqual(det.className, "realm_raid-realm-normal"))
        {
            //正常会有9个
            vec.push_back(det);
        }
    }
    int surrenderIndex[4] = {1,3,5,7};
    for (int i : surrenderIndex)
    {
        cv::Rect matchRect = vec[i].bbox;
        cv::Point clickPt = getRandomPointInRectExcludeWidth(matchRect, 0.0, 0.3, 10);
        clickInWindow(clickPt);
        QThread::msleep(2000);

        Logger::log(QString("点击进攻"));
        //点击攻击
        if (clickDetectionByLabel("common-btn-yellow_confirm",0.55,0.0,0.0))
        {
            QThread::msleep(4000);
            //按下esc 再按下enter
            // 按下 ESC 键，发送到指定窗口
            PostMessage(hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
            Sleep(10);
            PostMessage(hwnd, WM_KEYUP, VK_ESCAPE, 0);

            // 等待 100 毫秒
            Sleep(100);

            // 按下回车键，发送到指定窗口
            PostMessage(hwnd, WM_KEYDOWN, VK_RETURN, 0);
            Sleep(10);
            PostMessage(hwnd, WM_KEYUP, VK_RETURN, 0);

            QThread::msleep(5000);

            Logger::log(QString("识别失败并点击"));
            //识别战斗失败 并点击
            ocrRecognizesAndClick("失败", 0.5, true);
            QThread::msleep(2000);
        }else
        {
            //找不到 可按下的攻击按钮 话基本就是没有券了，直接结束
            Logger::log(QString("找不到 可按下的攻击按钮，票已清空"));
            return;
        }
    }

    Logger::log(QString("结界突破-开始进行清票操作"));
    for (const Detection det : vec)
    {
        //点击突破框
        cv::Rect matchRect = det.bbox;
        cv::Point clickPt = getRandomPointInRectExcludeWidth(matchRect, 0.0, 0.3, 10);
        clickInWindow(clickPt);
        QThread::msleep(2000);

        //点击攻击
        if (clickDetectionByLabel("common-btn-yellow_confirm",0.55,0.0,0.0))
        {
            // 循环等待直到找到战斗结束框，最多等待10分钟
            const int MAX_ATTEMPTS = 120;  // 120次 * 5秒 = 10分钟
            int attempts = 0;

            while (attempts < MAX_ATTEMPTS) {
                QThread::msleep(5000);  // 每次循环前等待5秒
                attempts++;

                qDebug() << "第" << attempts << "次尝试OCR识别...";

                QJsonArray res = ocrRecognizes();
                bool found = false;

                for (const QJsonValue& value : res) {
                    QJsonObject item = value.toObject();
                    QString text = item["text"].toString();

                    // 检查是否为胜利或失败
                    if (text == "点击屏幕继续" || text == "失败") {
                        found = true;

                        // 可以同时获取其他信息，比如坐标和置信度
                        QJsonArray box = item["box"].toArray();
                        if (box.size() == 4) {
                            // 提取四个点的坐标
                            QJsonArray point1 = box[0].toArray();
                            QJsonArray point2 = box[1].toArray();
                            QJsonArray point3 = box[2].toArray();
                            QJsonArray point4 = box[3].toArray();

                            int x1 = point1[0].toInt();
                            int y1 = point1[1].toInt();
                            int x2 = point2[0].toInt();
                            int y2 = point2[1].toInt();
                            int x3 = point3[0].toInt();
                            int y3 = point3[1].toInt();
                            int x4 = point4[0].toInt();
                            int y4 = point4[1].toInt();

                            // 计算矩形的最小外接矩形
                            int minX = std::min({x1, x2, x3, x4});
                            int minY = std::min({y1, y2, y3, y4});
                            int maxX = std::max({x1, x2, x3, x4});
                            int maxY = std::max({y1, y2, y3, y4});

                            matchRect = cv::Rect(minX, minY, maxX - minX, maxY - minY);
                            cv::Point endClickPt = getRandomPointInRect(matchRect);
                            clickInWindow(endClickPt);

                            QThread::msleep(3000);

                            qDebug() << "找到目标文本:" << text << "并已点击";
                        } else {
                            qWarning() << "box数组大小不正确，期望4个点，实际:" << box.size();
                        }

                        break;  // 找到目标后跳出内层循环
                    }
                }

                if (found) {
                    qDebug() << "成功找到目标文本，退出循环";
                    break;  // 找到目标后跳出外层循环
                }

                if (attempts >= MAX_ATTEMPTS) {
                    qWarning() << "达到最大尝试次数" << MAX_ATTEMPTS << "，未找到目标文本，退出循环";
                    break;
                }
            }
        }else
        {
            //找不到 可按下的攻击按钮 话基本就是没有券了，直接结束
            Logger::log(QString("找不到 可按下的攻击按钮，票已清空"));
            return;
        }
    }

    Logger::log(QString("结界突破执行完成"));
}

// 检查检测结果中是否包含指定标签
bool ExecutionSteps::hasDetectionWithLabel(const std::vector<Detection>& detections, const QString& targetLabel)
{
    return std::any_of(detections.begin(), detections.end(),
        [&](const Detection& det) {
            return comparesEqual(det.className, targetLabel);
        });
}

// 通用函数：点击指定标签的检测结果
bool ExecutionSteps::clickDetectionByLabel(const QString& targetLabel, double threshold,
                                         double excludeStart, double excludeEnd)
{
    auto detections = yoloRecognizes(threshold, excludeStart, excludeEnd);

    for (const auto& det : detections) {
        if (comparesEqual(det.className, targetLabel)) {
            cv::Point clickPt = getRandomPointInRectExcludeWidth(det.bbox, 0.0, 0.0, 10);
            clickInWindow(clickPt);
            return true;
        }
    }

    return false;
}