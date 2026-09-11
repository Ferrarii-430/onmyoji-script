#ifndef ANNIVERSARY999_H
#define ANNIVERSARY999_H

namespace scenarios {

    // 系统方案-十周年999：执行一轮十周年999活动流程。
    // 返回 true 表示本轮正常完成（继续下一轮循环），false 表示中途失败/任务达成（终止任务循环）。
    // 由 TaskRunner 的循环驱动重复执行，循环次数设为 0 时即无限挂机。
    bool executeAnniversary999();
    bool hasCollaboration();

    // 重置跨轮次内部状态（如阵容锁定标记）：单次任务运行的多次循环内保持，
    // 任务结束/停止后由 TaskRunner 调用重置，保证下次启动从初始状态开始
    void resetAnniversary999Status();
} // namespace scenarios

#endif // ANNIVERSARY999_H
