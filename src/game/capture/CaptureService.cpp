#include "src/game/capture/CaptureService.h"

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
        return winImg;
    }

    // 持久化开关开启时才保存 debug 图片
    if (SETTING_CONFIG.getPersistScreenshot()) {
        QString saveCapturePath = AppPaths::instance().thumbnailPath() + "/debug_capture_result.png";
        vision::imwriteQt(saveCapturePath, winImg);
    }

    window.setLastCaptureSize(winImg.size());
    return winImg;
}

bool setDllLogPath()
{
    return dllSetLogPath(GameWindow::instance().processId());
}

} // namespace capture
