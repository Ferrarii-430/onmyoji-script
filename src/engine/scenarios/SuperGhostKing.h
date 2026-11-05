#ifndef SUPERGHOSTKING_H
#define SUPERGHOSTKING_H

namespace scenarios {

    // 系统方案-超鬼王：执行一轮超鬼王活动流程。
    // 返回 true 表示本轮正常完成（继续下一轮循环），false 表示中途失败/任务达成（终止任务循环）。
    bool executeSuperGhostKing();

    // 重置跨轮次内部状态：单次任务运行的多次循环内保持，
    // 任务结束/停止后由 TaskRunner 调用重置，保证下次启动从初始状态开始
    void resetSuperGhostKingStatus();

} // namespace scenarios

#endif // SUPERGHOSTKING_H
