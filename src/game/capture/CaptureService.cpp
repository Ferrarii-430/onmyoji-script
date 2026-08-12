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

    // 所有截图模式前都按需无感调整窗口宽度至 600。
    // PrintWindow 模式同样需要窗口尺寸足够大才能截到清晰画面。
    window.ensureMinWidthForCapture(340);

    QString screenshotMode = SETTING_CONFIG.getScreenshotMode();
    if (screenshotMode == "PrintWindow") {
        hasWinImg = captureByPrintWindow(window.handle(), winImg);
    } else if (screenshotMode == "DirectX截图") {
        hasWinImg = captureByDllInjection(window.processId(), winImg);
    } else {
        Logger::log(QString("无法识别鼠标控制模式，默认使用DirectX截图"));
        hasWinImg = captureByDllInjection(window.processId(), winImg);
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
