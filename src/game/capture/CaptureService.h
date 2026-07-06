#ifndef CAPTURESERVICE_H
#define CAPTURESERVICE_H

#include <opencv2/core/mat.hpp>

namespace capture {

// 按 Setting 中配置的截图模式获取游戏画面（失败返回空 Mat），
// 并更新 GameWindow 的最近截图尺寸。
cv::Mat captureGameWindow();

// 初始化注入 DLL 的日志路径（基于当前定位到的游戏进程）
bool setDllLogPath();

} // namespace capture

#endif // CAPTURESERVICE_H
