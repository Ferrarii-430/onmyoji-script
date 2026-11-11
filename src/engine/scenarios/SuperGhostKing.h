#ifndef SUPERGHOSTKING_H
#define SUPERGHOSTKING_H

#include <QString>
#include <QVector>

namespace scenarios {

    // 超鬼王单个鬼王的配置，对应 BossConfigDialog 表格里的一行
    struct BossEntry {
        QString bossName;             // 鬼王名称，唯一键
        QString lineupName;           // 阵容名称
        int greenMark = 0;            // 绿标位置 0-5，0 表示不标记
        bool markBoss = false;        // 是否红标 BOSS
        bool strongPursuit = false;   // 是否开启强力追击
    };

    // 超鬼王整份运行配置：
    // 前半部分来自方案的 systemConfig(主界面表单里那几项)，
    // bosses 来自方案的 bossConfig(BossConfigDialog 里编辑的列表)
    struct SuperGhostKingConfig {
        QString schemeName;                // 方案名称，用于切换御魂
        QString defaultLineupName;         // 通用阵容：鬼王没单独配阵容时用它
        int defaultGreenLabel = 0;         // 默认绿标位置(未命中鬼王配置时兜底)
        bool defaultMarkBoss = false;      // 默认标记 BOSS(未命中鬼王配置时兜底)
        bool defaultStrongPursuit = false; // 默认强力追击(未命中鬼王配置时兜底)
        bool autoDrinkTea = false;         // 自动喝茶，具体怎么用由流程决定
        QVector<BossEntry> bosses;         // 鬼王列表，只用于查表，不逐条打日志
    };

    // 命中鬼王配置后的实际操作参数
    struct BossRuntimeParam {
        QString lineupName;           // 命中行没填阵容时回落到方案里的「通用阵容」
        int greenMark = 0;
        bool markBoss = false;
        bool strongPursuit = false;   // 未命中鬼王配置时为 false
    };

    // 从当前选中的方案(currentItem.id)读取运行配置；
    // 未选中方案或字段缺失时返回默认值，不会抛错
    SuperGhostKingConfig loadSuperGhostKingConfig();

    // 按鬼王名称查一条配置，找不到返回 nullptr。
    // 先精确匹配；OCR 常把前后缀一起识别进来，再退化为包含匹配并取最长的键，
    // 避免「大蛇」误命中「八岐大蛇」这类短名
    const BossEntry *findBossEntry(const SuperGhostKingConfig &config, const QString &bossName);

    // 把「识别到的鬼王名称」解析成实际操作参数：
    // 命中 bossConfig 用该行的阵容/绿标/红标；未命中则阵容留空、绿标与红标回落到方案级默认
    BossRuntimeParam resolveBossParam(const SuperGhostKingConfig &config, const QString &bossName);

    // 获取当前疲劳值
    int getFatigue();

    // 系统方案-超鬼王：执行一轮超鬼王活动流程。
    // 返回 true 表示本轮正常完成（继续下一轮循环），false 表示中途失败/任务达成（终止任务循环）。
    bool executeSuperGhostKing();

    // 重置跨轮次内部状态：单次任务运行的多次循环内保持，
    // 任务结束/停止后由 TaskRunner 调用重置，保证下次启动从初始状态开始
    void resetSuperGhostKingStatus();

} // namespace scenarios

#endif // SUPERGHOSTKING_H
