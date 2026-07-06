#ifndef EVENTLOOPUTILS_H
#define EVENTLOOPUTILS_H

#include <functional>

namespace core {

// 在等待期间继续处理 Qt 事件循环（保持 UI 响应）
void waitWithEventProcessing(int milliseconds);

// 同上，但每个时间片检查 keepWaiting()，返回 false 时提前结束等待
void waitWithEventProcessing(int milliseconds, const std::function<bool()>& keepWaiting);

} // namespace core

#endif // EVENTLOOPUTILS_H
