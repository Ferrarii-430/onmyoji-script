#ifndef CAPTURESERVICE_H
#define CAPTURESERVICE_H

#include <opencv2/core/mat.hpp>

#include <functional>

namespace capture {

// 按 Setting 中配置的截图模式获取游戏画面（失败返回空 Mat），
// 并更新 GameWindow 的最近截图尺寸。
cv::Mat captureGameWindow();

// 初始化注入 DLL 的日志路径（基于当前定位到的游戏进程）
bool setDllLogPath();

// ------------------------------
// 截图连续失败熔断
// 游戏窗口失联（崩溃/被关闭）时 captureGameWindow 会持续返回空 Mat，
// 无人值守场景会傻等满全部轮询超时。这里在截图层统计连续失败次数，
// 超过阈值后触发通知，由上层（TaskRunner）注册回调自动停止任务。
// ------------------------------
// 连续失败阈值：达到该次数触发 notify（之后计数重新累计）
constexpr int kMaxConsecutiveCaptureFailures = 5;

using CaptureFailNotifier = std::function<void(int failures)>;

// 注册/更新连续失败通知（传空清除）；计数达标时回调并以当前次数为参数
void setCaptureFailNotifier(CaptureFailNotifier notifier);

// 重置连续失败计数（任务启动时调用）
void resetCaptureFailCount();

} // namespace capture

#endif // CAPTURESERVICE_H
