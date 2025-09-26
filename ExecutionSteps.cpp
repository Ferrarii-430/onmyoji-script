//
// Created by CZY on 2025/9/25.
//

#include "ExecutionSteps.h"

#include <bemapiset.h>
#include <iostream>
#include <qwindowdefs_win.h>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <dwmapi.h>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <random>
#include <src/widget/main/mainwindow.h>

#include "Logger.h"

ExecutionSteps& ExecutionSteps::getInstance() {
    static ExecutionSteps instance; // C++11 保证线程安全
    return instance;
}

bool ExecutionSteps::checkHWNDHandle() {
    DWORD pid = findPidByProcessName(target);
    if (pid == 0) {
        Logger::log(QString("找不到进程： %1").arg(target));
        return false;
    }
    hwnd = findWindowByPid(pid);

    if (!hwnd) {
        Logger::log(QString("窗口未找到"));
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

void ExecutionSteps::opencvRecognizesAndClick(const std::string& templPath)
{
    cv::Mat winImg;
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
        return;
    }

    //加载模板文件
    cv::Mat templ = cv::imread(templPath, cv::IMREAD_COLOR);
    if (templ.empty()) {
        Logger::log("模板图片加载失败" + templPath);
        return;
    }

    // 在窗口图像中查找模板
    cv::Rect matchRect;
    double score;
    bool found = findTemplateMultiScaleInMatNMS(winImg, templ, matchRect, score,
                                             0.9, 1.15, 0.04, 0.86); // 根据需要调整

    if (!found) {
        Logger::log(QString("未找到匹配区域"));
        return;
    }

    cv::Point clickPt = getRandomPointInRect(matchRect);
    Logger::log("随机点击点: (" + std::to_string(clickPt.x) + ", " + std::to_string(clickPt.y) + ")");

    clickInWindow(clickPt);
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

    // 方法1: PrintWindow with full content
    BOOL (WINAPI *PrintWindow)(HWND, HDC, UINT) = nullptr;
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        PrintWindow = (BOOL (WINAPI*)(HWND, HDC, UINT))GetProcAddress(user32, "PrintWindow");
        if (PrintWindow) {
            // 尝试不同的标志
            captureOk = PrintWindow(hwnd, hMemDC, 0x2); // PW_RENDERFULLCONTENT
            if (!captureOk) {
                captureOk = PrintWindow(hwnd, hMemDC, 0x1); // PW_CLIENTONLY
            }
            if (!captureOk) {
                captureOk = PrintWindow(hwnd, hMemDC, 0x0); // 默认
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

    // 11. 调试输出
    // try {
    //     std::filesystem::path outDir = std::string(PROJECT_SOURCE_DIR) + "/debug";
    //     std::filesystem::create_directories(outDir);
    //     std::filesystem::path outPath = outDir / "debug_capture.png";
    //
    //     if (cv::imwrite(outPath.string(), outBGR)) {
    //         std::cout << "调试截图保存到: " << outPath.string() << std::endl;
    //
    //         // 同时保存原始BGRA数据用于对比
    //         cv::Mat debugBGRA(h, w, CV_8UC4, pixelData.data());
    //         cv::imwrite((outDir / "debug_bgra.png").string(), debugBGRA);
    //     }
    // } catch (const std::exception& e) {
    //     std::cerr << "保存调试截图失败: " << e.what() << std::endl;
    // }

    return true;
}

// 多尺度模板匹配，返回最优匹配矩形和置信度
bool ExecutionSteps::findTemplateMultiScaleInMatNMS(const cv::Mat& haystack, const cv::Mat& needle, cv::Rect& outRect, double& outScore,
                                    double scaleMin = 0.8, double scaleMax = 1.25, double scaleStep = 0.05, double threshold = 0.85)
{
    outScore = -1;
    if (haystack.empty() || needle.empty()) {
        std::cout << "[ERROR] haystack 或 needle 为空" << std::endl;
        return false;
    }

    cv::Mat gHay, gNeedle;
    cv::setUseOptimized(false);
    cvtColor(haystack, gHay, cv::COLOR_BGR2GRAY);
    cvtColor(needle, gNeedle, cv::COLOR_BGR2GRAY);

    std::vector<cv::Rect> candidateRects;
    std::vector<double> candidateScores;

    for (double s = scaleMin; s <= scaleMax; s += scaleStep) {
        cv::Mat resizedNeedle;
        resize(gNeedle, resizedNeedle, cv::Size(), s, s, cv::INTER_LINEAR);
        if (resizedNeedle.cols < 8 || resizedNeedle.rows < 8) continue;
        if (resizedNeedle.cols > gHay.cols || resizedNeedle.rows > gHay.rows) continue;

        cv::Mat result;
        matchTemplate(gHay, resizedNeedle, result, cv::TM_CCOEFF_NORMED);

        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

        // 打印调试日志
        Logger::log(QString("[LOG] scale=%1 minVal=%2 maxVal=%3 pos=(%4,%5)")
            .arg(s)
            .arg(minVal)
            .arg(maxVal)
            .arg(maxLoc.x)
            .arg(maxLoc.y));

        if (maxVal >= threshold) {
            cv::Rect r(maxLoc.x, maxLoc.y, resizedNeedle.cols, resizedNeedle.rows);
            candidateRects.push_back(r);
            candidateScores.push_back(maxVal);
        }
    }

    if (candidateRects.empty()) {
        Logger::log(QString("[WARN] 没有找到任何匹配 >= %1").arg(threshold));
        return false;
    }

    // NMS 合并重叠区域
    std::vector<cv::Rect> mergedRects = nonMaxSuppression(candidateRects, 0.3f);

    // 找到分数最高的矩形
    auto maxIt = std::max_element(candidateScores.begin(), candidateScores.end());
    int idx = std::distance(candidateScores.begin(), maxIt);

    outRect = candidateRects[idx];
    outScore = candidateScores[idx];

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

    Logger::log(QString("已点击窗口 (%1, %2")
        .arg(pt.x)
        .arg(pt.y));

}