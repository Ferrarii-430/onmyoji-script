#include "SuperGhostKing.h"

#include "ScenarioRegistry.h"

#include <optional>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRandomGenerator>
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

    // 本文件内所有处理方法统一复用的脚本动作实例
    static ScriptActions& actions = ScriptActions::instance();

    QString currentLineup = "";
    // 当前强力追击状态：空值表示还没识别过，避免「已知关闭」与「未知」混淆
    std::optional<bool> currentStrongPursuitStatus;

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
        currentLineup = "";
        currentStrongPursuitStatus.reset();
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
        // 截图目录必须在 QCoreApplication 创建后取：文件级静态初始化时 applicationDirPath() 还是空串，
        // 拼出来的路径无效，OpenCV 会报「无法加载图片」
        const QString screenshotPath = AppPaths::instance().screenshotPath();
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

        //识别召唤鬼王
        if (actions.opencvRecognizesAndClick(screenshotPath + "super-call.png", 0.65, true, true).isEmpty())
        {
            Logger::log(QString("点击【召唤鬼王】失败"));
            return false;
        }

        waitRandomWithEventProcessing(2500, 300);

        //获取鬼王名称
        QJsonArray ocrData = actions.ocrRecognizes(QRectF(38.4, 16.1, 6.5, 31.1), ocr::Enhance::Upscale | ocr::Enhance::AutoInvert);
        qDebug() << "[debug] ocrData:" << ocrData;
        if (ocrData.empty())
        {
            Logger::log(QString("无法识别鬼王名称"));
            return false;
        }
        // 默认配置兜底：识别到的名称在 bossConfig 里找不到时，param 保持这套方案级默认值
        BossRuntimeParam param;
        for (const QJsonValue& val : ocrData) {
            const QJsonObject item = val.toObject();
            const QString bossName = item["text"].toString();

            // 每条识别结果先回到默认配置，避免上一条命中的鬼王配置串到这一条
            param.lineupName = config.defaultLineupName;
            param.greenMark = config.defaultGreenLabel;
            param.markBoss = config.defaultMarkBoss;
            param.strongPursuit = config.defaultStrongPursuit;

            //从表单中寻找对应鬼王的配置，命中才覆盖默认值
            if (const BossEntry *entry = findBossEntry(config, bossName))
            {
                // 该行没填阵容时沿用方案里的通用阵容
                param.lineupName = entry->lineupName.isEmpty() ? config.defaultLineupName : entry->lineupName;
                param.greenMark = entry->greenMark;
                param.markBoss = entry->markBoss;
                param.strongPursuit = entry->strongPursuit;
            }

            Logger::log(QString("识别到鬼王【%1】: 阵容=%2 绿标=%3 标记BOSS=%4 强力追击=%5")
                            .arg(bossName,
                                 param.lineupName.isEmpty() ? QStringLiteral("(未填)") : param.lineupName)
                            .arg(param.greenMark)
                            .arg(param.markBoss ? QStringLiteral("是") : QStringLiteral("否"))
                            .arg(param.strongPursuit ? QStringLiteral("是") : QStringLiteral("否")));
        }

        waitRandomWithEventProcessing(1000, 200);

        if (currentLineup.isEmpty() || !comparesEqual(currentLineup, param.lineupName))
        {
            Logger::log(QString("开始切换预设"));
            if (actions.opencvRecognizesAndClick(screenshotPath + "super-lineup.png", 0.65, true, true).isEmpty())
            {
                Logger::log(QString("点击【切换预设队伍】失败"));
                return false;
            }

            waitRandomWithEventProcessing(1200, 200);

            //滑动到分组列表顶部
            Logger::log(QString("开始滑动到分组列表顶部"));
            utils::OcrTextTracker groupTopTracker;
            for (int i = 0; i < 10; ++i)
            {
                QJsonArray groupData = actions.ocrRecognizes(QRectF(37.7, 35.0, 11.1, 51.8), ocr::Enhance::Upscale);

                //文本内容与上次一致（忽略 box/score 波动），说明滑动未改变列表内容，已到顶部，结束循环
                if (groupTopTracker.unchangedSinceLast(groupData)) {
                    Logger::log(QString("分组列表别结果与上次一致，已滑动到顶部"));
                    break;
                }

                //向上滑动
                if (actions.swipeBetweenPoints(QPointF(43.7, 49.0), QPointF(43.3, 75.2),SETTING_CONFIG.getMouseControlMode(), 500).isEmpty())
                {
                    Logger::log(QString("滑动失败"));
                    return false;
                }
                //等列表滚动动画结束再识别，否则可能读到滑动前的旧画面
                waitRandomWithEventProcessing(2000,200);
            }

            //寻找目标分组列表
            Logger::log(QString("开始寻找目标分组列表"));
            bool foundGrouping = false;
            utils::OcrTextTracker groupingTracker;
            for (int i = 0; i < 10; ++i)
            {
                QJsonArray groupingData = actions.ocrRecognizes(QRectF(37.7, 35.0, 11.1, 51.8), ocr::Enhance::Upscale);

                //文本内容与上次一致（忽略 box/score 波动），说明滑动未改变列表内容，已到底部，结束循环
                if (groupingTracker.unchangedSinceLast(groupingData)) {
                    Logger::log(QString("分组列表识别结果与上次一致，已滑动到底部"));
                    break;
                }

                for (const QJsonValue& val : groupingData) {
                    const QJsonObject item = val.toObject();
                    if (item["text"].toString() == config.schemeName) {
                        const cv::Rect rect = utils::boxToRect(item["box"].toArray());
                        if (rect.width > 0 && rect.height > 0) {
                            GameWindow::instance().clickInWindow(utils::makeClickPoint(rect, true));
                            foundGrouping = true;
                            Logger::log(QString("已点击分组: %1").arg(config.schemeName));
                        }
                        break;
                    }
                }

                if (foundGrouping) {
                    //找到数据结束循环
                    break;
                }

                //向下滑动
                if (actions.swipeBetweenPoints(QPointF(43.3, 75.2),QPointF(43.7, 49.0), SETTING_CONFIG.getMouseControlMode(), 500).isEmpty())
                {
                    Logger::log(QString("滑动失败"));
                    return false;
                }
                //等列表滚动动画结束再识别，否则可能读到滑动前的旧画面
                waitRandomWithEventProcessing(2000,200);
            }
            if (!foundGrouping) {
                Logger::log(QString("未找到分组: %1").arg(config.schemeName));
                return false;
            }

            waitWithEventProcessing(1500);

            //先回到列表顶部，避免目标预设在上方时漏检
            Logger::log(QString("开始滑动到预设列表顶部"));
            utils::OcrTextTracker presetTopTracker;
            for (int i = 0; i < 10; ++i)
            {
                QJsonArray presetData = actions.ocrRecognizes(QRectF(48.5, 34.4, 30.8, 44.4), ocr::Enhance::Upscale);
                qDebug() << "[debug] presetData:" << presetData;

                //文本内容与上次一致（忽略 box/score 波动），说明滑动未改变列表内容，已到顶部，结束循环
                if (presetTopTracker.unchangedSinceLast(presetData)) {
                    Logger::log(QString("预设列表识别结果与上次一致，已滑动到顶部"));
                    break;
                }

                //向上滑动
                if (actions.swipeBetweenPoints(QPointF(63.2, 42.6), QPointF(62.8, 70.8),SETTING_CONFIG.getMouseControlMode(), 500).isEmpty())
                {
                    Logger::log(QString("滑动失败"));
                    return false;
                }
                //等列表滚动动画结束再识别，否则可能读到滑动前的旧画面
                waitRandomWithEventProcessing(2000,200);
            }

            //识别队伍预设名称，记录目标预设的 box
            bool foundPresets = false;
            QJsonArray targetPresetBox; // {"box":[[47,120],[77,120],[77,136],[47,136]]}
            utils::OcrTextTracker presetSearchTracker;
            Logger::log(QString("开始寻找目标预设列表"));
            for (int i = 0; i < 10; ++i)
            {
                QJsonArray presetData = actions.ocrRecognizes(QRectF(48.5, 34.4, 30.8, 44.4), ocr::Enhance::Upscale);

                //文本内容与上次一致（忽略 box/score 波动），说明滑动未改变列表内容，已到底部，结束循环
                if (presetSearchTracker.unchangedSinceLast(presetData)) {
                    Logger::log(QString("预设列表识别结果与上次一致，已滑动到底部"));
                    break;
                }

                for (const QJsonValue& val : presetData) {
                    const QJsonObject item = val.toObject();
                    if (item["text"].toString() == param.lineupName) {
                        targetPresetBox = item["box"].toArray();
                        break;
                    }
                }

                if (targetPresetBox.size() > 1) {
                    Logger::log(QString("找到预设阵容: %1").arg(param.lineupName));
                    foundPresets = true;
                    break;
                }

                //向下滑动
                if (actions.swipeBetweenPoints(QPointF(62.8, 70.8),QPointF(63.2, 42.6),SETTING_CONFIG.getMouseControlMode(), 500).isEmpty())
                {
                    Logger::log(QString("滑动失败"));
                    return false;
                }
                //等列表滚动动画结束再识别，否则可能读到滑动前的旧画面
                waitRandomWithEventProcessing(2000,200);
            }
            if (!foundPresets) {
                Logger::log(QString("未找到预设: %1").arg(param.lineupName));
                return false;
            }

            const cv::Rect presetRect = utils::boxToRect(targetPresetBox);
            if (presetRect.width <= 0 || presetRect.height <= 0) {
                Logger::log(QString("预设阵容 box 解析失败: %1").arg(param.lineupName));
                return false;
            }

            // 识别所有预设队伍切换按钮
            auto detections = actions.opencvFindAll(screenshotPath + "super-switch-soul.png", 0.6, false);
            if (detections.empty()) {
                Logger::log(QString("未找到预设队伍切换按钮"));
                return false;
            }

            // 在 detections 中找出 Y 轴最接近预设 box 中心的项并点击
            const int targetY = presetRect.y + presetRect.height / 2;
            const OpenCvMatch* best = nullptr;
            int minDY = INT_MAX;
            for (const auto& det : detections) {
                const int dy = std::abs(det.center.y - targetY);
                if (dy < minDY) {
                    minDY = dy;
                    best = &det;
                }
            }
            if (best) {
                GameWindow::instance().clickInWindow(utils::makeClickPoint(best->rect, false));
                Logger::log(QString("点击预设队伍切换: score=%1 位置(%2,%3) 目标Y=%4 偏差=%5")
                            .arg(best->score, 0, 'f', 2).arg(best->center.x).arg(best->center.y)
                            .arg(targetY).arg(minDY));
            }

            waitRandomWithEventProcessing(2000,200);

            //点击确认切换御魂
            if (actions.yoloContainsLabels(0.60, {"common-popup-confirm"}, false))
            {
                if (actions.yoloRecognizesAndClick(0.80, false, "common-btn-yellow_confirm").isEmpty())
                {
                    Logger::log(QString("无法识别到【确定】按钮"));
                    return false;
                }
            }

            //切换契灵 可能需要再点击一次确定
            if (actions.yoloContainsLabels(0.60, {"common-popup-confirm"}, false))
            {
                if (actions.yoloRecognizesAndClick(0.80, false, "common-btn-yellow_confirm").isEmpty())
                {
                    Logger::log(QString("无法识别到【确定】按钮"));
                    return false;
                }
            }

            waitRandomWithEventProcessing(2000,200);

            //点击目标预设队伍
            GameWindow::instance().clickInWindow(utils::makeClickPoint(presetRect, false));

            waitRandomWithEventProcessing(1200,200);

            //点击出战
            if (actions.ocrRecognizesAndClick("出战", 0.65, true, QRectF(48.5, 34.4, 30.8, 51.3), ocr::Enhance::Upscale).isEmpty())
            {
                Logger::log(QString("点击【出战】失败"));
            }

            //更新当前阵容名称
            currentLineup = param.lineupName;
        } else
        {
            Logger::log(QString("跳过预设切换"));
        }

        waitRandomWithEventProcessing(2000,200);

        // 只有还没识别过当前状态时才需要看界面，识别一次后跨轮次复用
        if (!currentStrongPursuitStatus.has_value())
        {
            currentStrongPursuitStatus = !actions.opencvFindAll(screenshotPath + "super-check.png", 0.8, true).empty();
        }

        //判断是否需要开启强力追击
        if (param.strongPursuit)
        {
            //开启
            if (!currentStrongPursuitStatus)
            {
                //只有关闭状态下需要开启
                if (actions.ocrRecognizesAndClick("2.5倍伤害", 0.65, false, QRectF(81.0, 68.3, 19.0, 13.3), ocr::Enhance::Upscale).isEmpty())
                {
                    Logger::log(QString("开启【强力追击】失败"));
                }
            }
            Logger::log(QString("【强力追击】已开启"));
        } else
        {
            //关闭
            if (currentStrongPursuitStatus)
            {
                //只有开启状态下需要关闭
                if (actions.opencvRecognizesAndClick(screenshotPath + "super-check.png", 0.65, true, false).isEmpty())
                {
                    Logger::log(QString("关闭【强力追击】失败"));
                    return false;
                }
            }
            Logger::log(QString("【强力追击】已关闭"));
        }

        waitRandomWithEventProcessing(2000,200);

        bool isKill = false;
        //不设置战斗次数上限
        for (int num = 0; num < 999; ++num)
        {
            //TODO 此处缺少逻辑，判断体力是否足够

            //点击战斗
            if (actions.opencvRecognizesAndClick(screenshotPath + "anniversary-challenge.png", 0.65, true, false).isEmpty())
            {
                Logger::log(QString("点击【战斗】失败"));
                return false;
            }

            waitRandomWithEventProcessing(3500,300);

            //判断是否进入战斗
            bool isEngageInBattle = false;
            for (int i = 0; i < 5 && TaskRunner::instance().isRunning(); ++i)
            {
                //先等1秒
                waitRandomWithEventProcessing(1000,200);
                if (!actions.yoloRecognizesAndClick(0.50, true, "battle-ready").isEmpty())
                {
                    Logger::log(QString("已进入【超鬼王】战斗场景"));
                    isEngageInBattle = true;
                    break;
                }
            }
            if (!isEngageInBattle)
            {
                if (!actions.hasCollaboration())
                {
                    Logger::log(QString("等待进入战斗场景超时"));
                    return false;
                }
            }

            waitRandomWithEventProcessing(2000,200);

            // 循环等待战斗结束 3分钟
            bool isBattleOfEnd = false;
            for (int i = 0; i < 36 && TaskRunner::instance().isRunning(); ++i)
            {
                Logger::log(QString("等待战斗结束...(%1/36)").arg(i+1));
                waitWithEventProcessing(5000);
                //是否有【造成伤害】文字
                if (actions.ocrContainsText("本次伤害", 0.7, QRectF(40.2, 39.0, 19.0, 11.6)))
                {
                    Logger::log(QString("战斗已经结束"));
                    isBattleOfEnd = true;
                    break;
                }
            }
            if (!isBattleOfEnd)
            {
                if (!actions.hasCollaboration())
                {
                    Logger::log(QString("等待战斗结束超时"));
                    return false;
                }else
                {
                    //此时无需重试 直接判定为战斗完成
                }
            }

            waitRandomWithEventProcessing(2000,200);

            //随机点击退出页面
            QString savePathReceiveRewards;

            // 随机生成 0 或 1
            if (QRandomGenerator::global()->bounded(2) == 0)
            {
                savePathReceiveRewards = actions.clickInRoi(QRectF(81, 15, 18, 70), true);
            }else
            {
                savePathReceiveRewards = actions.clickInRoi(QRectF(7, 10, 16, 79), true);
            }

            //点击退出伤害页面
            if (!savePathReceiveRewards.isEmpty())
            {
                Logger::log(QString("退出【伤害页面】成功"));
            }else
            {
                //这里不需要重试
                Logger::log(QString("退出【伤害页面】失败"));
                return false;
            }

            waitRandomWithEventProcessing(3000,200);

            //判断是否击杀鬼王
            for (int i = 0; i < 5; ++i)
            {
                waitWithEventProcessing(1000);
                if (actions.ocrContainsText("结算中", 0.65, QRectF(40.2, 39.0, 19.0, 11.6), ocr::Enhance::AutoInvert))
                {
                    Logger::log(QString("鬼王已击杀，正在结算中"));
                    //进入循环等待结算模式
                    for (int j = 0; j < 10; ++j)
                    {
                        waitWithEventProcessing(1000);
                        if (actions.opencvContains(screenshotPath + "super-call.png", 0.65, true))
                        {
                            Logger::log(QString("结算完成"));
                            break;
                        }
                    }
                    isKill = true;
                    break;
                }

                waitWithEventProcessing(200);

                //判断是否有【召唤鬼王】按钮
                if (actions.opencvContains(screenshotPath + "super-call.png", 0.65, true))
                {
                    isKill = true;
                    Logger::log(QString("鬼王已击杀"));
                    break;
                }

                waitWithEventProcessing(200);

                //判断是否有【战斗】按钮
                if (actions.opencvContains(screenshotPath + "anniversary-challenge.png", 0.65, true))
                {
                    isKill = false;
                    break;
                }
            }

            if (isKill)
            {
                break;
            }
            Logger::log(QString("鬼王未击杀，继续战斗"));
        }

        waitRandomWithEventProcessing(2000, 200);

        Logger::log(QString("超鬼王方案已完成"));
        return true;
    }

    // 获取当前体力值：识别逻辑待实现，暂时返回 0
    int getStamina()
    {
        return 0;
    }

    // 注册为系统方案：TaskRunner 按注册表调度（注册方式见 ScenarioRegistry.h）
    REGISTER_SCENARIO("SYSTEM_SUPER_GHOST_KING", executeSuperGhostKing, resetSuperGhostKingStatus, false)

} // namespace scenarios
