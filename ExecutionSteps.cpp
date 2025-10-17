//
// Created by CZY on 2025/9/25.
//

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
#include <random>
#include <src/widget/main/mainwindow.h>
#include "Logger.h"
#include "SettingManager.h"
#include "ConfigManager.h"
#include "src/utils/MouseSimulator.h"

ExecutionSteps& ExecutionSteps::getInstance() {
    static ExecutionSteps instance; // C++11 保证线程安全
    return instance;
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

    qDebug() << "执行截图命令:" << remoteCaptureExe << arguments;

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

    qDebug() << "截图命令输出:" << output;

    // 检查截图文件是否存在
    if (!QFile::exists(DX11_CAPTURE_PATH)) {
        qWarning() << "截图文件未生成:" << DX11_CAPTURE_PATH;
        return false;
    }

    // 使用 OpenCV 读取截图
    winImg = cv::imread(DX11_CAPTURE_PATH.toStdString());

    if (winImg.empty()) {
        qWarning() << "无法读取截图文件:" << DX11_CAPTURE_PATH;
        return false;
    }

    qDebug() << "截图成功，图像尺寸:" << winImg.cols << "x" << winImg.rows
             << "通道数:" << winImg.channels();

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

    // 执行截图命令
    QProcess process;
    QStringList arguments;
    arguments << "-log"
              << targetPid
              << DX11_HOOK_DLL_PATH
              << DX11_HOOK_DLL_NAME
              << DX11_LOG_PATH;

    qDebug() << "执行修改log地址命令:" << remoteCaptureExe << arguments;

    process.start(remoteCaptureExe, arguments);

    // 等待命令完成（设置超时时间，比如10秒）
    if (!process.waitForFinished(5000)) {
        qWarning() << "修改log地址命令执行超时";
        process.kill();
        return false;
    }

    // 检查命令执行结果
    int exitCode = process.exitCode();
    QByteArray output = process.readAllStandardOutput();
    QByteArray errorOutput = process.readAllStandardError();

    if (exitCode != 0) {
        qWarning() << "修改log地址命令执行失败，退出码:" << exitCode;
        qWarning() << "错误输出:" << errorOutput;
        return false;
    }

    qDebug() << "修改log地址命令输出:" << output;

    ok = true;
    return ok;
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

    // 执行截图命令
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
        hasWinImg = getOnmyojiCaptureByDllInjection(winImg);
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
    cv::Mat templ = cv::imread(templPath.toStdString()); //cv::IMREAD_COLOR
    if (templ.empty()) {
        Logger::log("模板图片加载失败: " + templPath);
        return nullptr;
    }else {
        Logger::log("已加载模板图片: " + templPath);
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
        Logger::log("随机点击点: (" + std::to_string(clickPt.x) + ", " + std::to_string(clickPt.y) + ")");
    } else {
        // 中心点击模式：点击匹配区域的中心点
        clickPt = cv::Point(matchRect.x + matchRect.width / 2,
                           matchRect.y + matchRect.height / 2);
        Logger::log("中心点击点: (" + std::to_string(clickPt.x) + ", " + std::to_string(clickPt.y) + ")");
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
    QString savePath = saveDir + "/debug_match_result.png";
    cv::imwrite(savePath.toStdString(), resultImg);
    // Logger::log("已保存识别结果图片: " + savePath);

    clickInWindow2(clickPt);

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

            Logger::log(QString("[LOG] scale=%1 maxVal=%2 pos=(%3,%4) size=%5x%6")
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

// 在 hwnd 窗口模拟鼠标点击
void ExecutionSteps::clickInWindow(const cv::Point& ptClient) {
    // 转换到屏幕坐标
    POINT pt = { ptClient.x, ptClient.y };
    ClientToScreen(hwnd, &pt);

    // 模拟鼠标输入
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dx = pt.x * (65535.0f / GetSystemMetrics(SM_CXSCREEN));
    inputs[0].mi.dy = pt.y * (65535.0f / GetSystemMetrics(SM_CYSCREEN));
    inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN;

    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dx = inputs[0].mi.dx;
    inputs[1].mi.dy = inputs[0].mi.dy;
    inputs[1].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTUP;

    SendInput(2, inputs, sizeof(INPUT));

    Logger::log(QString("已点击窗口 (%1, %2)")
        .arg(pt.x)
        .arg(pt.y));
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

void ExecutionSteps::clickInWindow2(const cv::Point& clickPoint) {
    // 创建 MouseSimulator 实例
    MouseSimulator simulator;

    // 配置模拟器以增强隐蔽性和后台兼容性
    simulator.SetHumanLikeMode(true);       // 启用人类行为模式
    simulator.SetRandomDelayRange(20, 100); // 设置短随机延迟以减少检测风险
    simulator.SetJitterLevel(3);            // 设置轻微抖动以模拟人类操作

    // 使用静默点击方法：StealthClick 使用混合模式（硬件+消息），确保后台执行
    bool success = simulator.StealthClick(clickPoint.x, clickPoint.y, true);

    if (success) {
        Logger::log("静默点击成功 at (" + std::to_string(clickPoint.x) + ", " + std::to_string(clickPoint.y) + ")");
    } else {
        Logger::log("静默点击失败 at (" + std::to_string(clickPoint.x) + ", " + std::to_string(clickPoint.y) + ")");
        // 可选： fallback 到硬件点击
        if (!simulator.HardwareClick(clickPoint.x, clickPoint.y)) {
            Logger::log(QString("备用硬件点击也失败"));
        }
    }
}