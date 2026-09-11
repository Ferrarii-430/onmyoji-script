#ifndef MITAMA_H
#define MITAMA_H

namespace scenarios {

    // 系统方案-御魂大蛇
    // 返回 true 表示本轮正常完成（继续下一轮循环），false 表示中途失败（终止任务循环）
    bool executeMitama();
    int getCurrentInterface();
    bool processingPopUpWindow();
} // namespace scenarios

#endif //MITAMA_H
