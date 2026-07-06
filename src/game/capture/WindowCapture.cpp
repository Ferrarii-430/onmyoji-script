#include "src/game/capture/WindowCapture.h"

#include <dwmapi.h>
#include <cstdint>
#include <string>
#include <vector>

#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"

namespace capture {

bool captureWindowToMat(HWND hwnd, cv::Mat& outBGR)
{
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
    core::waitWithEventProcessing(100); // 给窗口一些时间刷新

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
    captureOk = PrintWindow(hwnd, hMemDC, 0x0); // 默认方式
    if (!captureOk) {
        DWORD error = GetLastError();
        Logger::log(QString("PrintWindow默认方式失败，错误代码: %1").arg(error));

        captureOk = PrintWindow(hwnd, hMemDC, 0x1); // PW_CLIENTONLY
        if (!captureOk) {
            error = GetLastError();
            Logger::log(QString("PrintWindow PW_CLIENTONLY失败，错误代码: %1").arg(error));

            captureOk = PrintWindow(hwnd, hMemDC, 0x2); // PW_RENDERFULLCONTENT（Windows 8.1+）
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

bool captureByPrintWindow(HWND hwnd, cv::Mat& winImg)
{
    bool ok = false;
    for (int i = 0; i < 5; ++i)
    {
        ok = captureWindowToMat(hwnd, winImg);
        if (!ok || winImg.empty()) {
            Logger::log(QString("未能捕获窗口。它是最小化的还是受保护的？1秒内重试..."));
            core::waitWithEventProcessing(1000);
        }
    }
    if (!ok)
    {
        Logger::log(QString("5次重试未能捕获窗口。任务结束"));
    }
    return ok;
}

} // namespace capture
