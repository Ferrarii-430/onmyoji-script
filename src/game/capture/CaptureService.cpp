#include "src/game/capture/CaptureService.h"

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

    noteCaptureSuccess();
    window.setLastCaptureSize(winImg.size());
    return winImg;
}

bool setDllLogPath()
{
    return dllSetLogPath(GameWindow::instance().processId());
}

} // namespace capture
