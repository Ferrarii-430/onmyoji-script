#include "src/game/capture/CaptureService.h"

#include <cmath>
#include <utility>

#include <opencv2/imgcodecs.hpp>

#include "src/core/AppPaths.h"
#include "src/vision/ImageIo.h"
#include "src/core/Logger.h"
#include "src/core/SettingManager.h"
#include "src/game/GameWindow.h"
#include "src/game/capture/Dx11HookCapture.h"
#include "src/game/capture/WindowCapture.h"
#include "src/platform/ProcessUtils.h"
#include "src/platform/DPIHelper.h"

namespace capture {

namespace {

// 截图连续失败计数：成功一次即清零；达到 kMaxConsecutiveCaptureFailures
// 时触发通知并重新从 0 累计（避免每次失败都重复回调）。
//
// 必须是 thread_local：每个会话的任务跑在自己的 worker 线程，
// 各实例的截图成功率互不相干。若共用一份计数，A 实例的连续失败会把
// B 实例正在跑的任务一起熔断掉。
thread_local int t_consecutiveFailures = 0;
thread_local CaptureFailNotifier t_failNotifier;

// 成功截图：清零连续失败计数
void noteCaptureSuccess()
{
    t_consecutiveFailures = 0;
}

// 截图失败：累计并按需触发熔断通知
void noteCaptureFailure()
{
    ++t_consecutiveFailures;
    if (t_consecutiveFailures >= kMaxConsecutiveCaptureFailures && t_failNotifier) {
        t_failNotifier(t_consecutiveFailures);
        t_consecutiveFailures = 0;
    }
}

} // namespace

void setCaptureFailNotifier(CaptureFailNotifier notifier)
{
    t_failNotifier = std::move(notifier);
}

void resetCaptureFailCount()
{
    t_consecutiveFailures = 0;
}

cv::Mat captureGameWindow()
{
    cv::Mat winImg;
    bool hasWinImg = false;

    // 首先尝试提升权限
    if (!platform::enableDebugPrivilege()) {
        Logger::log(QString("提升调试权限失败!"));
        Logger::log(QString("【请用管理员权限启动!】"));
    }

    GameWindow& window = GameWindow::instance();

    // 所有截图模式前都按需无感调整窗口宽度至 700。
    // PrintWindow 模式同样需要窗口尺寸足够大才能截到清晰画面。
    constexpr int kMinCaptureWidth = 700;
    window.ensureMinWidthForCapture(kMinCaptureWidth);

    QString screenshotMode = SETTING_CONFIG.getScreenshotMode();
    const bool usePrintWindow = (screenshotMode == "PrintWindow");
    if (!usePrintWindow && screenshotMode != "DirectX截图") {
        Logger::log(QString("无法识别鼠标控制模式，默认使用DirectX截图"));
    }

    auto doCapture = [&]() -> bool {
        if (usePrintWindow) {
            return captureByPrintWindow(window.handle(), winImg);
        }
        return captureByDllInjection(window.processId(), winImg);
    };

    hasWinImg = doCapture();

    // 首次启动时 Unity 可能未处理窗口尺寸变化，swap chain 仍是旧的小尺寸
    // 此时按真实捕获尺寸强制调整窗口并重试一次，避免第一次任务启动识别失败。
    if (hasWinImg && winImg.cols < kMinCaptureWidth) {
        Logger::log(QString("截图尺寸 %1x%2 低于最小宽度 %3，调整窗口后重试截图")
                    .arg(winImg.cols).arg(winImg.rows).arg(kMinCaptureWidth));
        window.setLastCaptureSize(winImg.size());
        window.ensureMinWidthForCapture(kMinCaptureWidth);
        hasWinImg = doCapture();
    }

    if (!hasWinImg)
    {
        Logger::log(QString("游戏画面获取失败，请查看日志！"));
        noteCaptureFailure();
        return winImg;
    }

    // 持久化开关开启时才保存 debug 图片。
    // 文件名按实例 PID 区分：多标签页并发时各写各的，否则会互相覆盖
    if (SETTING_CONFIG.getPersistScreenshot()) {
        const DWORD capturePid = window.currentPid();
        QString saveCapturePath = AppPaths::instance().debugCapturePathFor(capturePid);
        vision::imwriteQt(saveCapturePath, winImg);
    }

    // 画幅诊断：ROI 以百分比保存，只有在两个环境的画幅比例一致时才能通用。
    // 这里把「截图尺寸 / 客户区 / 窗口矩形 / DPI」一次打全，
    // 便于跨分辨率排查时直接对比两边的日志。只在尺寸变化时输出，避免逐帧刷屏。
    {
        thread_local cv::Size t_lastDiagSize;
        if (winImg.size() != t_lastDiagSize) {
            t_lastDiagSize = winImg.size();

            RECT clientRect{};
            RECT windowRect{};
            const HWND hwnd = window.handle();
            if (hwnd) {
                GetClientRect(hwnd, &clientRect);
                GetWindowRect(hwnd, &windowRect);
            }
            const int clientW = clientRect.right - clientRect.left;
            const int clientH = clientRect.bottom - clientRect.top;
            const int windowW = windowRect.right - windowRect.left;
            const int windowH = windowRect.bottom - windowRect.top;

            const double captureAspect = winImg.rows > 0
                                             ? static_cast<double>(winImg.cols) / winImg.rows
                                             : 0.0;
            const double clientAspect = clientH > 0
                                            ? static_cast<double>(clientW) / clientH
                                            : 0.0;
            constexpr double kReferenceAspect = 16.0 / 9.0;
            const bool aspectOff = captureAspect > 0.0
                                   && std::abs(captureAspect - kReferenceAspect) / kReferenceAspect > 0.02;
            // 客户区通常是截图的整数倍：200% 缩放下游戏按逻辑像素渲染，DWM 再放大显示
            const double clientScale = winImg.cols > 0
                                           ? static_cast<double>(clientW) / winImg.cols
                                           : 0.0;

            Logger::log(QString("[截图诊断] 截图 %1x%2（比例 %3）| 客户区 %4x%5（比例 %6，÷截图 %7）| 窗口 %8x%9 | DPI %10x | %11")
                            .arg(winImg.cols)
                            .arg(winImg.rows)
                            .arg(QString::number(captureAspect, 'f', 4))
                            .arg(clientW)
                            .arg(clientH)
                            .arg(QString::number(clientAspect, 'f', 4))
                            .arg(QString::number(clientScale, 'f', 2))
                            .arg(windowW)
                            .arg(windowH)
                            .arg(QString::number(DPIHelper::GetWindowDPIScaling(hwnd), 'f', 2))
                            .arg(aspectOff ? QStringLiteral("⚠ 非 16:9（同一 ROI 百分比在其他画幅下会框到不同位置）")
                                           : QStringLiteral("16:9")));
        }
    }

    noteCaptureSuccess();
    window.setLastCaptureSize(winImg.size());
    return winImg;
}

bool setDllLogPath()
{
    return dllSetLogPath(GameWindow::instance().processId());
}

} // namespace capture
