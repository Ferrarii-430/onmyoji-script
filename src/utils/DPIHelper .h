// DPIHelper.h
#pragma once
#include <windows.h>
#include <cmath>
#include <opencv2/core/types.hpp>

class DPIHelper {
public:
    // 获取系统DPI缩放因子
    static double GetSystemDPIScaling() {
        HDC hdc = GetDC(nullptr);
        if (hdc) {
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(nullptr, hdc);
            return dpiX / 96.0;
        }
        return 1.0;
    }

    // 获取窗口DPI缩放因子（更精确）
    static double GetWindowDPIScaling(HWND hwnd) {
        // 尝试使用Windows 10+的Per-Monitor DPI Awareness
        HMODULE hUser32 = LoadLibraryA("user32.dll");
        if (hUser32) {
            typedef UINT (WINAPI *GetDpiForWindowFunc)(HWND);
            GetDpiForWindowFunc pGetDpiForWindow =
                (GetDpiForWindowFunc)GetProcAddress(hUser32, "GetDpiForWindow");
            if (pGetDpiForWindow) {
                UINT dpi = pGetDpiForWindow(hwnd);
                FreeLibrary(hUser32);
                return dpi / 96.0;
            }
            FreeLibrary(hUser32);
        }
        return GetSystemDPIScaling();
    }

    // 将物理坐标转换为窗口客户区坐标
    static bool PhysicalToClientCoord(HWND hwnd, cv::Point& physicalPoint, cv::Point& outClientPoint) {
        // 获取窗口信息
        RECT clientRect;
        if (!GetClientRect(hwnd, &clientRect)) {
            return false;
        }

        POINT clientTopLeft = {0, 0};
        if (!ClientToScreen(hwnd, &clientTopLeft)) {
            return false;
        }

        // 获取DPI缩放因子
        double scaleFactor = GetWindowDPIScaling(hwnd);

        // 转换坐标：物理坐标 -> 逻辑屏幕坐标 -> 客户区坐标
        int logicalScreenX = static_cast<int>(std::round(physicalPoint.x / scaleFactor));
        int logicalScreenY = static_cast<int>(std::round(physicalPoint.y / scaleFactor));

        outClientPoint.x = logicalScreenX - clientTopLeft.x;
        outClientPoint.y = logicalScreenY - clientTopLeft.y;

        // 钳制坐标到客户区范围内 - 使用强制类型转换
        int clientRight = static_cast<int>(clientRect.right);
        int clientBottom = static_cast<int>(clientRect.bottom);

        outClientPoint.x = std::max(0, std::min(outClientPoint.x, clientRight - 1));
        outClientPoint.y = std::max(0, std::min(outClientPoint.y, clientBottom - 1));

        return true;
    }

    // 验证坐标是否在客户区内
    static bool IsPointInClientRect(HWND hwnd, const cv::Point& point) {
        RECT clientRect;
        if (!GetClientRect(hwnd, &clientRect)) {
            return false;
        }
        return (point.x >= 0 && point.x < clientRect.right &&
                point.y >= 0 && point.y < clientRect.bottom);
    }
};
