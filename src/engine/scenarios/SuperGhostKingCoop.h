#ifndef SUPERGHOSTKINGCOOP_H
#define SUPERGHOSTKINGCOOP_H

namespace scenarios {

    // 系统方案-超鬼王(协同)：大小号协同模式，流程待实现。
    //
    // systemConfig 与「系统方案-超鬼王」完全一致，额外多两项：
    //   coopRole        —— 协同身份：大号(击杀方) / 小号(提供方)
    //   mainAccountName —— 大号名称，小号据此识别要协同的对象
    // 鬼王列表仍用方案顶层的 bossConfig，读取与查表逻辑见 SuperGhostKing.h：
    //   loadSuperGhostKingConfig() / findBossEntry() / resolveBossParam()

    // 执行一轮协同模式流程。
    // 返回 true 表示本轮正常完成（继续下一轮循环），false 表示中途失败/任务达成（终止任务循环）。
    bool executeSuperGhostKingCoop();

    // 重置跨轮次内部状态；任务结束/停止后由 TaskRunner 调用
    void resetSuperGhostKingCoopStatus();

} // namespace scenarios

#endif // SUPERGHOSTKINGCOOP_H
