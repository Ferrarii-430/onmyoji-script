#ifndef MITAMA_H
#define MITAMA_H

namespace scenarios {

    // 系统方案-御魂大蛇
    // 返回 true 表示本轮正常完成（继续下一轮循环），false 表示中途失败（终止任务循环）
    bool executeMitama();
    int getCurrentInterface();
    bool processingPopUpWindow();

    // 重置跨轮次内部状态（如阵容锁定标记）：单次任务运行的多次循环内保持，
    // 任务结束/停止后由 TaskRunner 调用重置，保证下次启动从初始状态开始
    void resetMitamaStatus();
} // namespace scenarios

#endif //MITAMA_H
