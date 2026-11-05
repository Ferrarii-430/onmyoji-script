#include "SuperGhostKing.h"

#include "ScenarioRegistry.h"

#include <QString>

#include "src/core/Logger.h"

namespace scenarios {

    // 跨轮次状态示例：仅在单次任务运行的多次循环内保持的标记，
    // 需要更多跨轮次状态时在此追加，并同步在 resetSuperGhostKingStatus 中重置

    // 重置跨轮次状态：任务结束/停止后由 TaskRunner 调用，下次启动任务重新走初始化流程
    void resetSuperGhostKingStatus()
    {
    }

    bool executeSuperGhostKing()
    {
        Logger::log(QString("开始执行超鬼王"));

        // TODO 超鬼王执行流程待实现（识别界面、进入挑战、结算循环）

        Logger::log(QString("超鬼王方案流程尚未实现"));
        return false;
    }

    // 注册为系统方案：TaskRunner 按注册表调度（注册方式见 ScenarioRegistry.h）
    REGISTER_SCENARIO("SYSTEM_SUPER_GHOST_KING", executeSuperGhostKing, resetSuperGhostKingStatus, false)

} // namespace scenarios
