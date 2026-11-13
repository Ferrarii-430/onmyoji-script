#include "SuperGhostKingCoop.h"

#include "ScenarioRegistry.h"
#include "SuperGhostKing.h" // 复用方案级配置读取与鬼王列表查表

#include <algorithm>

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QRandomGenerator>
#include <QRegularExpression>
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

    // 等待疲劳值回落到 100 以下再继续战斗（逻辑与 SuperGhostKing.cpp 保持一致）。
    // 未勾选自动喝茶时，疲劳值 >= 100 无法进入战斗，只能等它自然降低；
    // 勾选自动喝茶时由游戏内喝茶弹窗处理，无需等待。
    // 返回 false 表示等待期间任务已被停止，调用方应直接结束流程。
    static bool waitFatigueBelowBattleThreshold(bool autoDrinkTea)
    {
        if (autoDrinkTea)
        {
            return TaskRunner::instance().isRunning();
        }

        int fatigue = getFatigue();
        while (fatigue >= 100 && TaskRunner::instance().isRunning())
        {
            Logger::log(QString("当前疲劳值【%1】，等待疲劳值降低中...").arg(fatigue));
            // 带停止检查的等待：每个时间片都会检查 isRunning，点停止后可立即退出
            waitWithEventProcessing(60000, [] { return TaskRunner::instance().isRunning(); });
            fatigue = getFatigue();
        }
        return TaskRunner::instance().isRunning();
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
        const QString screenshotPath = AppPaths::instance().screenshotPath();

        //识别是否鬼王邀请，没有则循环等待 10分钟
        Logger::log(QString("等待鬼王邀请"));
        bool inviteFound = false;
        for (int i = 0; i < 200 && TaskRunner::instance().isRunning() && !inviteFound; ++i)
        {
            QJsonArray bossData = actions.ocrRecognizes(QRectF(2.8, 49.2, 23.8, 37.2), ocr::Enhance::Upscale, 0.6);
            for (const QJsonValue& val : bossData) {
                const QJsonObject item = val.toObject();
                const QString text = item["text"].toString();
                // 匹配「已分享：x人」，兼容中英文冒号、冒号缺失及空格
                static const QRegularExpression sharedRe(QStringLiteral("已分享[：:]?\\s*(\\d+)\\s*人"));
                const QRegularExpressionMatch match = sharedRe.match(text);
                if (!match.hasMatch()) {
                    continue;
                }
                const int sharedCount = match.captured(1).toInt();
                Logger::log(QString("识别到鬼王邀请：%1（已分享 %2 人）").arg(text).arg(sharedCount));
                // 已分享人数达到 1 人，视为收到邀请，跳出等待循环
                if (sharedCount == 1) {
                    const cv::Rect rect = utils::boxToRect(item["box"].toArray());
                    if (rect.width > 0 && rect.height > 0)
                    {
                        GameWindow::instance().clickInWindow(utils::makeClickPoint(rect, true));
                        inviteFound = true;
                        Logger::log(QString("已点击分享的鬼王"));
                        break;
                    }
                }
            }
            if (inviteFound) {
                break;
            }
            waitWithEventProcessing(3000);
            Logger::log(QString("等待鬼王邀请中...(%1/200)").arg(i+1));
        }
        if (!inviteFound)
        {
            Logger::log(QStringLiteral("未识别到鬼王邀请，流程超时"));
            return false;
        }

        waitRandomWithEventProcessing(1500, 300);

        //获取鬼王名称
        QJsonArray ocrData = actions.ocrRecognizes(QRectF(33.6, 18.4, 15.1, 27.1), ocr::Enhance::Upscale, 0.6);
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
            qDebug() << "[debug] bossName:" << bossName;

            // 每条识别结果先回到默认配置，避免上一条命中的鬼王配置串到这一条
            param.lineupName = config.base.defaultLineupName;
            param.greenMark = config.base.defaultGreenLabel;
            param.markBoss = config.base.defaultMarkBoss;
            param.strongPursuit = config.base.defaultStrongPursuit;

            //从表单中寻找对应鬼王的配置，命中才覆盖默认值
            if (const BossEntry *entry = findBossEntry(config, bossName))
            {
                // 该行没填阵容时沿用方案里的通用阵容
                param.lineupName = entry->lineupName.isEmpty() ? config.base.defaultLineupName : entry->lineupName;
                param.greenMark = entry->greenMark;
                param.markBoss = entry->markBoss;
                param.strongPursuit = entry->strongPursuit;
            }

            Logger::log(QString("识别到鬼王【%1】: 阵容=%2 绿标=%3 标记BOSS=%4 强力追击=%5")
                            .arg(bossName,
                                 param.lineupName.isEmpty() ? QStringLiteral("(未填)") : param.lineupName)
                            .arg(param.greenMark)
                            .arg(param.markBoss ? QStringLiteral("是") : QStringLiteral("否"), param.strongPursuit ? QStringLiteral("是") : QStringLiteral("否")));
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
                QJsonArray groupData = actions.ocrRecognizes(QRectF(37.7, 29.7, 11.1, 56.1), ocr::Enhance::Upscale, 0.6);
                qDebug() << "[debug] groupData:" << groupData;
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
                waitRandomWithEventProcessing(2500,200);
            }

            //寻找目标分组列表
            Logger::log(QString("开始寻找目标分组列表"));
            bool foundGrouping = false;
            utils::OcrTextTracker groupingTracker;
            for (int i = 0; i < 10; ++i)
            {
                QJsonArray groupingData = actions.ocrRecognizes(QRectF(37.7, 29.7, 11.1, 56.1), ocr::Enhance::Upscale, 0.6);

                //文本内容与上次一致（忽略 box/score 波动），说明滑动未改变列表内容，已到底部，结束循环
                if (groupingTracker.unchangedSinceLast(groupingData)) {
                    Logger::log(QString("分组列表识别结果与上次一致，已滑动到底部"));
                    break;
                }

                for (const QJsonValue& val : groupingData) {
                    const QJsonObject item = val.toObject();
                    if (item["text"].toString() == config.base.schemeName) {
                        const cv::Rect rect = utils::boxToRect(item["box"].toArray());
                        if (rect.width > 0 && rect.height > 0) {
                            GameWindow::instance().clickInWindow(utils::makeClickPoint(rect, true));
                            foundGrouping = true;
                            Logger::log(QString("已点击分组: %1").arg(config.base.schemeName));
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
                waitRandomWithEventProcessing(2500,200);
            }
            if (!foundGrouping) {
                Logger::log(QString("未找到分组: %1").arg(config.base.schemeName));
                return false;
            }

            waitWithEventProcessing(1500);

            //先回到列表顶部，避免目标预设在上方时漏检
            Logger::log(QString("开始滑动到预设列表顶部"));
            utils::OcrTextTracker presetTopTracker;
            //确定有多少个预设可选择，防止识别到错误的数据
            auto switchNumberArray = actions.opencvFindAll(screenshotPath + "super-switch-soul.png", 0.6, false);
            qDebug() << "[debug] switchNumberArray:" << switchNumberArray.size();
            constexpr QRectF presetsRoiArray[5] = {
                {48.5, 30.4, 30.8, 14.6}, //0或者1个
                {48.5, 30.4, 30.8, 28.1}, //2个
                {48.5, 30.4, 30.8, 41.6}, //3个
                {48.5, 30.4, 30.8, 47.2}};//大于3个
            QRectF presetsRoi = {};
            switch (switchNumberArray.size())
            {
                case 0:
                case 1:
                    presetsRoi = presetsRoiArray[0];
                    break;

                case 2:
                    presetsRoi = presetsRoiArray[1];
                    break;
                case 3:
                    presetsRoi = presetsRoiArray[2];
                    break;
                default:
                    presetsRoi = presetsRoiArray[3];
            }

            for (int i = 0; i < 10; ++i)
            {
                QJsonArray presetData = actions.ocrRecognizes(presetsRoi, ocr::Enhance::Upscale | ocr::Enhance::AutoInvert, 0.6);
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
                QJsonArray presetData = actions.ocrRecognizes(presetsRoi, ocr::Enhance::Upscale | ocr::Enhance::AutoInvert, 0.6);
                qDebug() << "[debug] presetData:" << presetData;

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
                    //此处不一定需要切换
                }
            }

            //切换契灵 可能需要再点击一次确定
            if (actions.yoloContainsLabels(0.60, {"common-popup-confirm"}, false))
            {
                if (actions.yoloRecognizesAndClick(0.80, false, "common-btn-yellow_confirm").isEmpty())
                {
                    Logger::log(QString("无法识别到【确定】按钮"));
                    //此处不一定需要切换
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
            currentStrongPursuitStatus = actions.opencvContains(screenshotPath + "super-check.png", 0.75, true);
            Logger::log(QString("当前【强力追击】状态：%1").arg(currentStrongPursuitStatus.value() ? QStringLiteral("是") : QStringLiteral("否")));
        }

        //判断是否需要开启强力追击
        if (param.strongPursuit)
        {
            //开启
            if (!currentStrongPursuitStatus.value())
            {
                //只有关闭状态下需要开启
                if (actions.ocrRecognizesAndClick("2.5倍伤害", 0.60, true, QRectF(81.0, 66.3, 19.0, 13.3), ocr::Enhance::Upscale | ocr::Enhance::AutoInvert).isEmpty())
                {
                    Logger::log(QString("开启【强力追击】失败"));
                    return false;
                }
            }
            Logger::log(QString("【强力追击】已开启"));
        } else
        {
            //关闭
            if (currentStrongPursuitStatus.value())
            {
                //只有开启状态下需要关闭
                if (actions.opencvRecognizesAndClick(screenshotPath + "super-check.png", 0.65, false, true).isEmpty())
                {
                    Logger::log(QString("关闭【强力追击】失败"));
                    return false;
                }
            }
            Logger::log(QString("【强力追击】已关闭"));
        }

        waitRandomWithEventProcessing(2000,200);

        //勾选锁定
        if (!isLock.has_value())
        {
            if (!actions.opencvRecognizesAndClick(screenshotPath + "super-lock.png", 0.75, false, true).isEmpty())
            {
                Logger::log(QString("已锁定阵容"));
                isLock = true;
            } else
            {
                Logger::log(QString("锁定阵容失败"));
            }
        }

        bool isKill = false;
        //战斗次数上限10次
        for (int num = 0; num < 10; ++num)
        {
            //判断疲劳值是否满足进入战斗（需小于100）
            if (!waitFatigueBelowBattleThreshold(config.base.autoDrinkTea))
            {
                Logger::log(QString("等待疲劳值降低超时"));
                return false;
            }

            //点击战斗
            if (actions.opencvRecognizesAndClick(screenshotPath + "anniversary-challenge.png", 0.65, true, false, ClickExclude{0.2, 0.2, 0.2, 0.2}).isEmpty())
            {
                Logger::log(QString("点击【战斗】失败"));
                return false;
            }

            //这里可能会触发喝茶
            //TODO 自动喝茶的逻辑还没写

            waitRandomWithEventProcessing(3500,300);

            //有准备就点一下，没有则不用管
            if (!actions.yoloRecognizesAndClick(0.50, true, "battle-ready").isEmpty())
            {
                Logger::log(QString("点击【准备】"));
            }

            //判断是否进入战斗
            bool isEngageInBattle = false;
            for (int i = 0; i < 5 && TaskRunner::instance().isRunning(); ++i)
            {
                //先等1秒
                waitRandomWithEventProcessing(1000,200);
                if (actions.yoloContainsLabels(0.45, {"common-exit-battle"}, false))
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

            //是否红标boss
            if (param.markBoss)
            {
                waitRandomWithEventProcessing(1000,200);
                if (!actions.clickInRoi(QRectF(81, 15, 18, 70), true).isEmpty())
                {
                    Logger::log(QString("已红标BOSS"));
                }
            }

            //是否需要绿标队友
            if (param.greenMark > 0)
            {
                waitRandomWithEventProcessing(2000,200);
                QString savePathGreenMark;
                const QRectF roiArray[5] = {
                    {14.9, 66.4, 5.1, 8.4},
                    {30.7, 59.7, 5.1, 8.4},
                    {45.0, 55.3, 5.1, 8.4},
                    {56.1, 56.3, 5.1, 8.4},
                    {71.6, 64.3, 5.1, 8.4}};
                switch (param.greenMark)
                {
                    case 1:
                        savePathGreenMark = actions.clickInRoi(roiArray[0], false);
                        break;
                    case 2:
                        savePathGreenMark = actions.clickInRoi(roiArray[1], false);
                        break;
                    case 3:
                        savePathGreenMark = actions.clickInRoi(roiArray[2], false);
                        break;
                    case 4:
                        savePathGreenMark = actions.clickInRoi(roiArray[3], false);
                        break;
                    case 5:
                        savePathGreenMark = actions.clickInRoi(roiArray[4], false);
                        break;
                    default:
                        Logger::log(QString("绿标数据异常"));
                        return false;
                }
                if (!savePathGreenMark.isEmpty())
                {
                    Logger::log(QString("已绿标【%1】号位").arg(param.greenMark));
                }
            }

            // 循环等待战斗结束 3分钟
            bool isBattleOfEnd = false;
            for (int i = 0; i < 36 && TaskRunner::instance().isRunning(); ++i)
            {
                Logger::log(QString("等待战斗结束...(%1/36)").arg(i+1));
                waitWithEventProcessing(5000);
                //是否有【本次伤害】文字
                if (actions.ocrContainsText("本次伤害", 0.55, QRectF(40.2, 39.0, 19.0, 11.6)))
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
                    Logger::log(QString("鬼王未击杀，继续挑战"));
                    break;
                }
            }

            if (isKill)
            {
                break;
            }

            if (num == 9)
            {
                Logger::log(QString("超出单次鬼王战斗上限"));
            }
        }

        waitRandomWithEventProcessing(2000, 200);

        Logger::log(QString("超鬼王协同方案(击杀方)已完成"));

        return true;
    }

    //小号流程
    bool executeCoopProvider(const SuperGhostKingCoopConfig &config)
    {
        // TODO: 小号(提供方)流程待实现。可用参数：
        //   config.mainAccountName      要协同的大号名称
        //   config.inviteStarLevels     只处理这些星级（isInviteStarLevelEnabled 判断）
        //   config.base.bosses          鬼王列表
        Q_UNUSED(config);
        const QString screenshotPath = AppPaths::instance().screenshotPath();

        //点击查询是否有对应鬼王
        if (actions.ocrRecognizesAndClick("全部队列", 0.65, true, QRectF(61.9, 50.4, 38.1, 49.6), ocr::Enhance::Upscale).isEmpty())
        {
            Logger::log(QString("点击【全部队列】失败"));
            return false;
        }
        waitRandomWithEventProcessing(2000, 200);

        QJsonArray ticketsData = actions.ocrRecognizes(QRectF(24.9, 38.1, 51.28, 23.2), ocr::Enhance::Upscale, 0.75);

        // 提取「持有:x」，按 box 左边界从左到右排序，第 1~6 个分别对应 1~6 星
        struct TicketEntry
        {
            int left = 0;
            int count = 0;
            cv::Rect rect;
        };
        QList<TicketEntry> entries;
        static const QRegularExpression holdRe(QStringLiteral("持有[：:]?\\s*(\\d+)"));
        for (const QJsonValue &value : ticketsData)
        {
            const QJsonObject item = value.toObject();
            const QRegularExpressionMatch match = holdRe.match(item.value("text").toString());
            if (!match.hasMatch())
            {
                continue;
            }

            // box 格式 [[x1,y1],[x2,y2],[x3,y3],[x4,y4]]，ocrRecognizes 已映射回窗口坐标
            const QJsonArray box = item.value("box").toArray();
            if (box.size() != 4)
            {
                continue;
            }
            int minX = 0;
            int minY = 0;
            int maxX = 0;
            int maxY = 0;
            bool firstPoint = true;
            for (const QJsonValue &pointValue : box)
            {
                const QJsonArray point = pointValue.toArray();
                if (point.size() < 2)
                {
                    continue;
                }
                const int px = point[0].toInt();
                const int py = point[1].toInt();
                if (firstPoint)
                {
                    minX = maxX = px;
                    minY = maxY = py;
                    firstPoint = false;
                    continue;
                }
                minX = std::min(minX, px);
                minY = std::min(minY, py);
                maxX = std::max(maxX, px);
                maxY = std::max(maxY, py);
            }
            if (firstPoint)
            {
                continue;
            }

            entries.append({minX, match.captured(1).toInt(),
                            cv::Rect(minX, minY, maxX - minX, maxY - minY)});
        }
        std::sort(entries.begin(), entries.end(), [](const TicketEntry &a, const TicketEntry &b)
        {
            return a.left < b.left;
        });

        // 星级 -> 持有数量/点击区域（后续判断直接用 starHoldings.value(star, 0)）
        QMap<int, int> starHoldings;
        QMap<int, cv::Rect> starRects;
        for (int i = 0; i < entries.size() && i < 6; ++i)
        {
            starHoldings.insert(i + 1, entries.at(i).count);
            starRects.insert(i + 1, entries.at(i).rect);
        }

        QStringList holdParts;
        for (auto it = starHoldings.cbegin(); it != starHoldings.cend(); ++it)
        {
            holdParts << QString("%1星=%2").arg(it.key()).arg(it.value());
        }
        Logger::log(QString("各星级持有数量: %1").arg(holdParts.isEmpty() ? QStringLiteral("(未识别到)") : holdParts.join(QStringLiteral(", "))));

        // 按勾选星级升序（加载配置时已排序）找第一个持有数 > 0 的星级；
        // 若勾选的星级全部持有为 0，则停止任务
        int targetStar = 0;
        for (int star : config.inviteStarLevels)
        {
            if (starHoldings.value(star, 0) > 0)
            {
                targetStar = star;
                break;
            }
        }

        if (targetStar == 0)
        {
            Logger::log(QStringLiteral("勾选星级的鬼王持有数量均为 0，任务停止"));
            return false;
        }

        // 点击对应星级的「持有:x」box
        const cv::Rect targetRect = starRects.value(targetStar);
        Logger::log(QString("%1星鬼王持有数量 %2，点击对应条目").arg(targetStar).arg(starHoldings.value(targetStar)));
        GameWindow::instance().clickInWindow(utils::makeClickPoint(targetRect, false),
                                             QString("选中 %1 星鬼王").arg(targetStar));

        waitRandomWithEventProcessing(1200, 200);

        //点击立即召唤
        if (actions.ocrRecognizesAndClick("立即召唤", 0.65, true, QRectF(20.0, 58.4, 59.6, 17.0), ocr::Enhance::Upscale).isEmpty())
        {
            Logger::log(QString("点击【立即召唤】失败"));
            return false;
        }

        waitRandomWithEventProcessing(4000, 300);

        //开始邀请队友
        if (actions.opencvRecognizesAndClick(screenshotPath + "super-invite.png", 0.65, true, false, ClickExclude{0.2, 0.2, 0.2, 0.2}).isEmpty())
        {
            Logger::log(QString("点击【集结】失败"));
            return false;
        }

        waitRandomWithEventProcessing(2000, 300);

        //开始寻找大号ID
        Logger::log(QString("开始寻找大号ID"));
        bool foundFriends = false;
        utils::OcrTextTracker groupingTracker;
        for (int i = 0; i < 10; ++i)
        {
            QJsonArray friendsData = actions.ocrRecognizes(QRectF(26.6, 24.6, 46.1, 50.8), ocr::Enhance::Upscale, 0.70);

            //文本内容与上次一致（忽略 box/score 波动），说明滑动未改变列表内容，已到底部，结束循环
            if (groupingTracker.unchangedSinceLast(friendsData)) {
                Logger::log(QString("好友列表识别结果与上次一致，已滑动到底部"));
                break;
            }

            for (const QJsonValue& val : friendsData) {
                const QJsonObject item = val.toObject();
                if (item["text"].toString() == config.mainAccountName) {
                    const cv::Rect rect = utils::boxToRect(item["box"].toArray());
                    if (rect.width > 0 && rect.height > 0) {
                        GameWindow::instance().clickInWindow(utils::makeClickPoint(rect, true));
                        foundFriends = true;
                        Logger::log(QString("已点击好友: %1").arg(config.mainAccountName));
                    }
                    break;
                }
            }

            if (foundFriends) {
                //找到数据结束循环
                break;
            }

            //向下滑动
            if (actions.swipeBetweenPoints(QPointF(49.1, 64.4),QPointF(49.1, 31.2), SETTING_CONFIG.getMouseControlMode(), 500).isEmpty())
            {
                Logger::log(QString("滑动失败"));
                return false;
            }
            //等列表滚动动画结束再识别，否则可能读到滑动前的旧画面
            waitRandomWithEventProcessing(2000,200);
        }
        if (!foundFriends) {
            Logger::log(QString("未找到好友: %1").arg(config.mainAccountName));
            return false;
        }

        waitWithEventProcessing(1500);

        //点击邀请
        if (actions.ocrRecognizesAndClick("邀请", 0.65, true, QRectF(26.6, 73.24, 47.1, 14.5), ocr::Enhance::Upscale).isEmpty())
        {
            Logger::log(QString("点击【邀请】失败"));
            return false;
        }

        waitWithEventProcessing(1500);

        //等待大号击杀鬼王后继续邀请
        bool isKill = false; //等30分钟
        for (int i = 0; i < 180 && TaskRunner::instance().isRunning(); ++i)
        {
            Logger::log(QString("等待大号击杀鬼王...(%1/180)").arg(i+1));
            waitWithEventProcessing(10000);
            if (actions.opencvContains(screenshotPath + "super-call.png", 0.65, true))
            {
                isKill = true;
                Logger::log(QString("大号已经击杀鬼王"));
                break;
            }
        }
        if (!isKill)
        {
            Logger::log(QString("等待大号击杀鬼王超时"));
            return false;
        }

        waitWithEventProcessing(2000);

        Logger::log(QString("超鬼王协同方案(提供方)已完成"));
        return true;
    }

    // 注册为系统方案：TaskRunner 按注册表调度（注册方式见 ScenarioRegistry.h）
    REGISTER_SCENARIO("SYSTEM_SUPER_GHOST_KING_COOP", executeSuperGhostKingCoop, resetSuperGhostKingCoopStatus, false)

} // namespace scenarios
