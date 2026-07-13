#ifndef ARENA_H
#define ARENA_H

namespace scenarios {

// 系统方案-自动挂机斗技：执行一轮斗技（匹配 -> 准备 -> 自动战斗 -> 结算）。
// 由 TaskRunner 的循环驱动重复执行，循环次数设为 0 时即无限挂机。
void executeArena();
bool getNumberOfFraction();

} // namespace scenarios

#endif // ARENA_H
