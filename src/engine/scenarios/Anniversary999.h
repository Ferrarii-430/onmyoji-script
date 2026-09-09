#ifndef ANNIVERSARY999_H
#define ANNIVERSARY999_H

namespace scenarios {

// 系统方案-十周年999：执行一轮十周年999活动流程。
// 返回 true 表示本轮正常完成（继续下一轮循环），false 表示中途失败/任务达成（终止任务循环）。
// 由 TaskRunner 的循环驱动重复执行，循环次数设为 0 时即无限挂机。
bool executeAnniversary999();
bool hasCollaboration();
} // namespace scenarios

#endif // ANNIVERSARY999_H
