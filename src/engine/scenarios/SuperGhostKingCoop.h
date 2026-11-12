#ifndef SUPERGHOSTKINGCOOP_H
#define SUPERGHOSTKINGCOOP_H

#include <QString>
#include <QVector>

#include "SuperGhostKing.h"

namespace scenarios {

    // 系统方案-超鬼王(协同)：大小号协同模式，流程待实现。
    //
    // systemConfig 与「系统方案-超鬼王」完全一致，额外多两项：
    //   coopRole        —— 协同身份：大号(击杀方) / 小号(提供方)
    //   mainAccountName —— 大号名称，小号据此识别要协同的对象
    // 鬼王列表仍用方案顶层的 bossConfig，读取与查表逻辑复用 SuperGhostKing.h

    // 协同身份
    enum class CoopRole {
        Killer,  // 大号(击杀方)
        Provider // 小号(提供方)
    };

    // 超鬼王协同整份运行配置：
    // base 与超鬼王完全一致（方案级配置 + bossConfig 鬼王列表），
    // 另加协同模式专属的两项
    struct SuperGhostKingCoopConfig {
        SuperGhostKingConfig base;        // 方案级配置 + 鬼王列表
        CoopRole role = CoopRole::Killer; // 协同身份
        QString mainAccountName;          // 大号名称（小号协同时使用）
        // 只邀请这些星级的鬼王（1-6）；空表示一个都不邀请。
        // 配置里没有该项（老配置）时按 1-6 全选处理，避免行为突变
        QVector<int> inviteStarLevels;
    };

    // 从当前选中的方案(currentItem.id)读取运行配置；
    // 未选中方案或字段缺失时返回默认值，不会抛错
    SuperGhostKingCoopConfig loadSuperGhostKingCoopConfig();

    // 协同身份转可读文本（日志/界面提示用）
    QString coopRoleToText(CoopRole role);

    // 该星级是否允许邀请（1-6）；未勾选的星级不邀请
    bool isInviteStarLevelEnabled(const SuperGhostKingCoopConfig &config, int star);

    // 按鬼王名称查一条配置，找不到返回 nullptr。
    // 查表规则与超鬼王一致：先精确匹配，再退化为包含匹配并取最长的键，
    // 避免「大蛇」误命中「八岐大蛇」这类短名
    const BossEntry *findBossEntry(const SuperGhostKingCoopConfig &config, const QString &bossName);

    // 把「识别到的鬼王名称」解析成实际操作参数：
    // 命中 bossConfig 用该行的阵容/绿标/红标；未命中则阵容留空、绿标与红标回落到方案级默认
    BossRuntimeParam resolveBossParam(const SuperGhostKingCoopConfig &config, const QString &bossName);

    // 执行一轮协同模式流程：读取配置后按协同身份分流到击杀方/提供方。
    // 返回 true 表示本轮正常完成（继续下一轮循环），false 表示中途失败/任务达成（终止任务循环）。
    bool executeSuperGhostKingCoop();

    // 大号(击杀方)流程：流程待实现，config 已按方案 + 本标签页覆盖值准备好
    bool executeCoopKiller(const SuperGhostKingCoopConfig &config);

    // 小号(提供方)流程：流程待实现，config 已按方案 + 本标签页覆盖值准备好
    bool executeCoopProvider(const SuperGhostKingCoopConfig &config);

    // 重置跨轮次内部状态；任务结束/停止后由 TaskRunner 调用
    void resetSuperGhostKingCoopStatus();

} // namespace scenarios

#endif // SUPERGHOSTKINGCOOP_H
