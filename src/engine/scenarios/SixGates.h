#ifndef SIXGATES_H
#define SIXGATES_H

#include <QString>

namespace scenarios {

    // 系统方案-六道之门：执行一轮六道之门活动流程。
    // 返回 true 表示本轮正常完成（继续下一轮循环），false 表示中途失败/任务达成（终止任务循环）。
    // 由 TaskRunner 的循环驱动重复执行，循环次数设为 0 时即无限挂机。
    bool executeSixGates();

    // 重置跨轮次内部状态：单次任务运行的多次循环内保持，
    // 任务结束/停止后由 TaskRunner 调用重置，保证下次启动从初始状态开始
    void resetSixGatesStatus();

    // 技能选择处理：在技能卡片的「选择」按钮中，点击与技能名称水平位置最近的一项；
    // 成功后技能等级 +1 并等待界面刷新，可在六道之门任意流程中调用
    bool selectSixGatesSkill(const QString& skillName);
} // namespace scenarios

#endif // SIXGATES_H
