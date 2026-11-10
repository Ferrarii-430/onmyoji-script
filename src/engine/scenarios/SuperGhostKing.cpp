#include "SuperGhostKing.h"

#include "ScenarioRegistry.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"

namespace scenarios {

    namespace
    {
        // 日志用：把配置值转成可读文本（布尔转「是/否」，空串给出提示）
        QString valueToText(const QJsonValue &value)
        {
            if (value.isBool())
            {
                return value.toBool() ? QStringLiteral("是") : QStringLiteral("否");
            }
            if (value.isDouble())
            {
                return QString::number(value.toDouble());
            }
            const QString text = value.toString();
            return text.isEmpty() ? QStringLiteral("(未填)") : text;
        }

        // 一条日志打印整份 systemConfig（label=value）：
        // 以后加新配置项不用再改日志代码，也不会像逐字段拼接那样漏项
        void logSystemConfig(const QString &configId)
        {
            QStringList parts;
            for (const QJsonValue &val : getSystemConfig(configId))
            {
                const QJsonObject field = val.toObject();
                const QString key = field.value("key").toString();
                if (key.isEmpty())
                {
                    continue;
                }
                const QString label = field.value("label").toString();
                parts << QString("%1=%2").arg(label.isEmpty() ? key : label,
                                              valueToText(field.value("value")));
            }

            Logger::log(parts.isEmpty()
                            ? QStringLiteral("超鬼王配置: (方案未配置任何参数)")
                            : QStringLiteral("超鬼王配置: ") + parts.join(QStringLiteral(" | ")));
        }
    } // namespace

    // 跨轮次状态示例：仅在单次任务运行的多次循环内保持的标记，
    // 需要更多跨轮次状态时在此追加，并同步在 resetSuperGhostKingStatus 中重置

    // 重置跨轮次状态：任务结束/停止后由 TaskRunner 调用，下次启动任务重新走初始化流程
    void resetSuperGhostKingStatus()
    {
    }

    SuperGhostKingConfig loadSuperGhostKingConfig()
    {
        SuperGhostKingConfig config;

        // currentItem 在任务启动时才填充，必须在运行时读，不能做成静态初始化
        const QString configId = currentItem.id;
        if (configId.isEmpty())
        {
            Logger::log(QString("超鬼王: 当前未选中方案，配置全部使用默认值"));
            return config;
        }

        // ---- 方案级配置：按 key 取值，新增字段只需在这里加一行 ----
        const auto readValue = [&configId](const char *key)
        {
            return getSystemConfigValue(configId, QString::fromLatin1(key));
        };

        config.schemeName = readValue("schemeName").toString();
        config.defaultLineupName = readValue("defaultLineupName").toString();
        config.defaultGreenLabel = qBound(0, readValue("defaultGreenLabel").toInt(0), 5);
        config.defaultMarkBoss = readValue("defaultMarkBoss").toBool(false);
        config.defaultStrongPursuit = readValue("defaultStrongPursuit").toBool(false);
        config.autoDrinkTea = readValue("autoDrinkTea").toBool(false);

        // ---- 鬼王列表：BossConfigDialog 里编辑的 bossConfig ----
        const QJsonArray bossArray = getBossConfig(configId);
        config.bosses.reserve(bossArray.size());
        for (const QJsonValue &value : bossArray)
        {
            if (!value.isObject())
            {
                continue;
            }
            const QJsonObject obj = value.toObject();

            BossEntry entry;
            entry.bossName = obj.value("bossName").toString().trimmed();
            // 空名称的行没有查表意义，跳过（弹窗保存时已拦过一次，这里防手工改配置）
            if (entry.bossName.isEmpty())
            {
                continue;
            }
            entry.lineupName = obj.value("lineupName").toString().trimmed();
            entry.greenMark = qBound(0, obj.value("greenMark").toInt(0), 5);
            entry.markBoss = obj.value("markBoss").toBool(false);
            // 老配置没有这个键，缺省按关闭处理
            entry.strongPursuit = obj.value("strongPursuit").toBool(false);

            config.bosses.append(entry);
        }

        // 只打方案级配置：鬼王列表可能有几十上百条，逐条打日志会刷屏，
        // 需要排查时直接看 config.json 里该方案的 bossConfig
        logSystemConfig(configId);

        return config;
    }

    const BossEntry *findBossEntry(const SuperGhostKingConfig &config, const QString &bossName)
    {
        const QString target = bossName.trimmed();
        if (target.isEmpty())
        {
            return nullptr;
        }

        // 1. 精确匹配（大小写不敏感）
        for (const BossEntry &entry : config.bosses)
        {
            if (entry.bossName.compare(target, Qt::CaseInsensitive) == 0)
            {
                return &entry;
            }
        }

        // 2. OCR 常把前后缀一起识别进来（如「八岐大蛇 · 三星」），退化为包含匹配
        //    取最长的命中键，避免「大蛇」这类短键抢走「八岐大蛇」的结果
        const BossEntry *best = nullptr;
        for (const BossEntry &entry : config.bosses)
        {
            if (target.contains(entry.bossName, Qt::CaseInsensitive))
            {
                if (!best || entry.bossName.length() > best->bossName.length())
                {
                    best = &entry;
                }
            }
        }
        return best;
    }

    BossRuntimeParam resolveBossParam(const SuperGhostKingConfig &config, const QString &bossName)
    {
        BossRuntimeParam param;
        // 先铺方案级默认值（通用阵容 + 默认绿标/标记/强力追击），命中后再被该鬼王的配置覆盖
        param.lineupName = config.defaultLineupName;
        param.greenMark = config.defaultGreenLabel;
        param.markBoss = config.defaultMarkBoss;
        param.strongPursuit = config.defaultStrongPursuit;

        const BossEntry *entry = findBossEntry(config, bossName);
        if (!entry)
        {
            Logger::log(QString("鬼王【%1】未在配置列表中，使用方案默认: 阵容=%2 绿标=%3 标记BOSS=%4 强力追击=%5")
                            .arg(bossName.trimmed(),
                                 param.lineupName.isEmpty() ? QStringLiteral("(未填)") : param.lineupName)
                            .arg(param.greenMark)
                            .arg(param.markBoss ? QStringLiteral("是") : QStringLiteral("否"))
                            .arg(param.strongPursuit ? QStringLiteral("是") : QStringLiteral("否")));
            return param;
        }

        // 该行没填阵容时沿用方案里的通用阵容；其余开关以行为准
        if (!entry->lineupName.isEmpty())
        {
            param.lineupName = entry->lineupName;
        }
        param.greenMark = entry->greenMark;
        param.markBoss = entry->markBoss;
        param.strongPursuit = entry->strongPursuit;

        Logger::log(QString("命中鬼王配置【%1】: 阵容=%2 绿标=%3 标记BOSS=%4 强力追击=%5")
                        .arg(entry->bossName,
                             param.lineupName.isEmpty() ? QStringLiteral("(未填)") : param.lineupName)
                        .arg(param.greenMark)
                        .arg(param.markBoss ? QStringLiteral("是") : QStringLiteral("否"))
                        .arg(param.strongPursuit ? QStringLiteral("是") : QStringLiteral("否")));
        return param;
    }

    bool executeSuperGhostKing()
    {
        // 每轮重新读取：任务运行期间在界面上改了鬼王配置，下一轮即生效。
        // 加载函数内部会把 systemConfig 打进日志；bosses 留给流程识别后查表用。
        const SuperGhostKingConfig config = loadSuperGhostKingConfig();
        Q_UNUSED(config); // 流程尚未实现，先保留读取以便日志与后续接入

        Logger::log(QString("开始执行超鬼王"));

        // TODO 超鬼王执行流程待实现（识别界面、进入挑战、结算循环）。
        // 识别到鬼王名称后按下面方式取该鬼王的阵容/绿标/标记：
        //   const BossRuntimeParam param = resolveBossParam(config, ocrBossName);
        //   if (!param.lineupName.isEmpty()) { 切换阵容(param.lineupName); }
        //   标记(param.greenMark, param.markBoss);

        Logger::log(QString("超鬼王方案流程尚未实现"));
        return false;
    }

    // 注册为系统方案：TaskRunner 按注册表调度（注册方式见 ScenarioRegistry.h）
    REGISTER_SCENARIO("SYSTEM_SUPER_GHOST_KING", executeSuperGhostKing, resetSuperGhostKingStatus, false)

} // namespace scenarios
