#include "SuperGhostKingCoop.h"

#include "ScenarioRegistry.h"
#include "SuperGhostKing.h" // 复用方案级配置读取与鬼王列表查表

#include <optional>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/core/SettingManager.h"
#include "src/engine/ScriptActions.h"
#include "src/engine/TaskRunner.h"
#include "src/game/GameWindow.h"
#include "src/utils/ScenarioHelpers.h"

using core::waitWithEventProcessing;
using core::waitRandomWithEventProcessing;

namespace scenarios {

    // 本文件内所有处理方法统一复用的脚本动作实例（与 SuperGhostKing.cpp 一致的线程局部写法）
    [[maybe_unused]] static thread_local ScriptActions& actions = ScriptActions::instance();

    // 跨轮次状态：待流程实现时按需补充，并同步在 resetSuperGhostKingCoopStatus 中重置

    // 重置跨轮次状态：任务结束/停止后由 TaskRunner 调用，下次启动任务重新走初始化流程
    void resetSuperGhostKingCoopStatus()
    {
        // 暂无跨轮次状态
    }

    bool executeSuperGhostKingCoop()
    {
        // TODO: 协同模式流程待实现，参考 SuperGhostKing.cpp：
        //   1. loadSuperGhostKingConfig() 读取方案级配置（两个方案共用同一套 key，
        //      新增 coopRole / mainAccountName 两项，可直接 getSystemConfigValue 读取）
        //   2. 按 coopRole 分流：大号(击杀方) / 小号(提供方)，
        //      mainAccountName 用于小号识别要协同的大号
        //   3. 识别鬼王名称后 resolveBossParam() 查表拿阵容/绿标/红标

        // 空流程直接返回 false 终止循环，避免被无限重复执行
        Logger::log(QStringLiteral("超鬼王(协同)：流程待实现"));
        return false;
    }

    // 注册为系统方案：TaskRunner 按注册表调度（注册方式见 ScenarioRegistry.h）
    REGISTER_SCENARIO("SYSTEM_SUPER_GHOST_KING_COOP", executeSuperGhostKingCoop, resetSuperGhostKingCoopStatus, false)

} // namespace scenarios
