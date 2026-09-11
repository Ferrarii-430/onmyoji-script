#ifndef BORDERBREAKTHROUGH_H
#define BORDERBREAKTHROUGH_H

namespace scenarios {

// 系统方案-结界突破：完整执行直到消耗完所有券
// 返回 true 表示本轮正常完成（继续下一轮循环），false 表示中途失败（终止任务循环）
bool executeBorderBreakthrough();
int getNumberOfTickets(); //查看突破门票数量

} // namespace scenarios

#endif // BORDERBREAKTHROUGH_H
