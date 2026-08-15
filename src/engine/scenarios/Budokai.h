#ifndef BUDOKAI_H
#define BUDOKAI_H

namespace scenarios {

// 系统方案-武道大会：执行一轮武道大会流程。
// 返回 true 表示本轮正常完成（继续下一轮循环），false 表示中途失败（终止任务循环）。
// 由 TaskRunner 的循环驱动重复执行，循环次数设为 0 时即无限挂机。
bool executeBudokai();
void resetBudokaiStatus();

} // namespace scenarios

#endif // BUDOKAI_H
