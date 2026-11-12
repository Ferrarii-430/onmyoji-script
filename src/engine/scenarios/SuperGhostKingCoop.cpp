#include "SuperGhostKingCoop.h"

#include "ScenarioRegistry.h"
#include "SuperGhostKing.h" // 复用方案级配置读取与鬼王列表查表

#include <algorithm>

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

    // 组队协战与单人超鬼王是两条独立流程，状态各自一份。
    // 必须加 static（内部链接）：否则与 SuperGhostKing.cpp 的同名 thread_local
    // 会在链接期重复定义（thread_local 不影响符号唯一性）。
    static thread_local QString currentLineup = "";
    // 当前强力追击状态：空值表示还没识别过，避免「已知关闭」与「未知」混淆
    static thread_local std::optional<bool> currentStrongPursuitStatus;
    static thread_local std::optional<bool> isLock; // 阵容锁定状态，仅本文件内使用

    QString coopRoleToText(CoopRole role)
    {
        return role == CoopRole::Provider ? QStringLiteral("小号(提供方)") : QStringLiteral("大号(击杀方)");
    }

    bool isInviteStarLevelEnabled(const SuperGhostKingCoopConfig &config, int star)
    {
        return config.inviteStarLevels.contains(star);
    }

    SuperGhostKingCoopConfig loadSuperGhostKingCoopConfig()
    {
        SuperGhostKingCoopConfig config;

        // 方案级配置与鬼王列表完全复用超鬼王的读取逻辑：
        // 两个方案的 systemConfig key 一致，bossConfig 也挂在同一个方案对象上，
        // 读取函数内部会把方案级配置打进日志，未选中方案时返回默认值
        config.base = loadSuperGhostKingConfig();

        // currentItem 在任务启动时才填充，必须在运行时读，不能做成静态初始化
        const QString configId = currentItem.id;
        if (configId.isEmpty())
        {
            // loadSuperGhostKingConfig 已经打过「未选中方案」的日志，这里直接返回默认值
            return config;
        }

        // ---- 协同模式专属配置：按 key 取值 ----
        const QString roleText = getSystemConfigValue(configId, QStringLiteral("coopRole")).toString().trimmed();
        // 配置里存的是「大号(击杀方)」「小号(提供方)」两种文本，按「小号」关键字判断；
        // 空值/异常值一律按大号(击杀方)处理
        config.role = roleText.contains(QStringLiteral("小号")) ? CoopRole::Provider : CoopRole::Killer;
        config.mainAccountName = getSystemConfigValue(configId, QStringLiteral("mainAccountName")).toString().trimmed();

        // 邀请星级：只邀请勾选星级的鬼王；缺省（老配置没有该键）按 1-6 全选处理，
        // 空数组表示一个都不邀请，所以必须用 isArray 区分「没配」和「全不选」
        const QJsonValue starValue = getSystemConfigValue(configId, QStringLiteral("inviteStarLevels"));
        if (starValue.isArray())
        {
            for (const QJsonValue &value : starValue.toArray())
            {
                const int star = value.toInt();
                if (star >= 1 && star <= 6 && !config.inviteStarLevels.contains(star))
                {
                    config.inviteStarLevels.append(star);
                }
            }
            std::sort(config.inviteStarLevels.begin(), config.inviteStarLevels.end());
        }
        else
        {
            for (int star = 1; star <= 6; ++star)
            {
                config.inviteStarLevels.append(star);
            }
        }

        QStringList starParts;
        for (int star : config.inviteStarLevels)
        {
            starParts << QString::number(star);
        }
        Logger::log(QString("超鬼王协同配置: 协同身份=%1 | 大号名称=%2 | 邀请星级=%3")
                        .arg(coopRoleToText(config.role),
                             config.mainAccountName.isEmpty() ? QStringLiteral("(未填)") : config.mainAccountName,
                             starParts.isEmpty() ? QStringLiteral("(全不选)") : starParts.join(QStringLiteral(","))));

        return config;
    }

    const BossEntry *findBossEntry(const SuperGhostKingCoopConfig &config, const QString &bossName)
    {
        // 查表逻辑与超鬼王完全一致，直接转发到基配置，避免两处规则漂移
        return findBossEntry(config.base, bossName);
    }

    BossRuntimeParam resolveBossParam(const SuperGhostKingCoopConfig &config, const QString &bossName)
    {
        // 命中/未命中的兜底规则同样复用超鬼王的实现
        return resolveBossParam(config.base, bossName);
    }

    // 重置跨轮次状态：任务结束/停止后由 TaskRunner 调用，下次启动任务重新走初始化流程
    void resetSuperGhostKingCoopStatus()
    {
        currentLineup = "";
        currentStrongPursuitStatus.reset();
        isLock.reset();
    }

    bool executeSuperGhostKingCoop()
    {
        // 每轮重新读取：任务运行期间在界面上改配置，下一轮即生效。
        // 加载函数内部会把方案级配置、协同身份、邀请星级打进日志
        const SuperGhostKingCoopConfig config = loadSuperGhostKingCoopConfig();

        // 按协同身份分流：大号负责击杀，小号负责提供
        if (config.role == CoopRole::Provider)
        {
            Logger::log(QStringLiteral("进入小号(提供方)流程"));
            return executeCoopProvider(config);
        } else
        {
            Logger::log(QStringLiteral("进入大号(击杀方)流程"));
            return executeCoopKiller(config);
        }
    }

    //大号流程
    bool executeCoopKiller(const SuperGhostKingCoopConfig &config)
    {
        // TODO: 大号(击杀方)流程待实现。可用参数：
        //   config.base.bosses          鬼王列表（配合 resolveBossParam 查阵容/绿标/红标）
        //   config.inviteStarLevels     只邀请这些星级（isInviteStarLevelEnabled 判断）
        //   config.base.schemeName / defaultLineupName / defaultGreenLabel 等方案级配置
        Q_UNUSED(config); // 参数已接通，流程待实现

        // 空流程直接返回 false 终止循环，避免被无限重复执行
        return false;
    }

    //小号流程
    bool executeCoopProvider(const SuperGhostKingCoopConfig &config)
    {
        // TODO: 小号(提供方)流程待实现。可用参数：
        //   config.mainAccountName      要协同的大号名称
        //   config.inviteStarLevels     只处理这些星级（isInviteStarLevelEnabled 判断）
        //   config.base.bosses          鬼王列表
        Q_UNUSED(config); // 参数已接通，流程待实现

        // 空流程直接返回 false 终止循环，避免被无限重复执行
        return false;
    }

    // 注册为系统方案：TaskRunner 按注册表调度（注册方式见 ScenarioRegistry.h）
    REGISTER_SCENARIO("SYSTEM_SUPER_GHOST_KING_COOP", executeSuperGhostKingCoop, resetSuperGhostKingCoopStatus, false)

} // namespace scenarios
