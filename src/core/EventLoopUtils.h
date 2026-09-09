#ifndef EVENTLOOPUTILS_H
#define EVENTLOOPUTILS_H

#include <functional>

namespace core {

// 在等待期间继续处理 Qt 事件循环（保持 UI 响应）
void waitWithEventProcessing(int milliseconds);

// 同上，但每个时间片检查 keepWaiting()，返回 false 时提前结束等待
void waitWithEventProcessing(int milliseconds, const std::function<bool()>& keepWaiting);

// 同 waitWithEventProcessing，但实际等待时长在
// [milliseconds - offsetMilliseconds, milliseconds + offsetMilliseconds] 内随机取值，
// offsetMilliseconds <= 0 时退化为固定等待；结果不会小于 0
void waitRandomWithEventProcessing(int milliseconds, int offsetMilliseconds);

} // namespace core

#endif // EVENTLOOPUTILS_H
