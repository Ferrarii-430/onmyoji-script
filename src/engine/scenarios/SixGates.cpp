//
// Created by CZY on 2026/10/17.
//
#include "SixGates.h"

#include "ScenarioRegistry.h"

#include "src/utils/ScenarioHelpers.h"

#include <QJsonObject>
#include <QString>

#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/core/SettingManager.h"
#include "src/engine/ScriptActions.h"
#include "src/engine/TaskRunner.h"
#include "src/game/GameWindow.h"

using core::waitWithEventProcessing;
using core::waitRandomWithEventProcessing;

namespace scenarios {

    // 跨轮次状态示例：仅在单次任务运行的多次循环内保持的标记，
    // 需要更多跨轮次状态时在此追加，并同步在 resetSixGatesStatus 中重置
    bool sixGates_isRoundInitialized = false;
    int skillLevel = 0; //技能等级
    int roundNum = 0; //当前回合数
    int ROUND_TOTAL = 25; //总回合数
    static bool isLock = false; //是否锁定，仅本文件内使用

    // 六道之门内部共享实例：本文件内所有处理方法统一复用
    static ScriptActions& actions = ScriptActions::instance();

    // 截图目录必须在 QCoreApplication 创建后再取：文件级静态初始化时 applicationDirPath()
    // 还是空串，拼出来的路径无效，OpenCV 会报「无法加载图片」，所以用函数在调用时取值
    static QString screenshotPath()
    {
        return AppPaths::instance().screenshotPath();
    }

    // 重置跨轮次状态：任务结束/停止后由 TaskRunner 调用，下次启动任务重新走初始化流程
    void resetSixGatesStatus()
    {
        sixGates_isRoundInitialized = false;
        skillLevel = 0;
        roundNum = 0;
        isLock = false;
    }

    namespace
    {
        // 六道之门地图场景
        enum class SixGatesScene
        {
            None,
            Mystery,
            Chaos,
            Fighting,
            Rest,
            HeartIsland
        };

        struct SceneTemplate
        {
            SixGatesScene scene;
            const char* fileName;
            const char* sceneName;
        };

        // 数组顺序即进入优先级：神秘 > 混沌 > 鏖战 > 宁息 > 心之岛
        constexpr SceneTemplate kSceneTemplates[] = {
            {SixGatesScene::Mystery, "six-gates-mystery.png", "神秘"},
            {SixGatesScene::Chaos, "six-gates-chaos.png", "混沌"},
            {SixGatesScene::Fighting, "six-gates-fighting.png", "鏖战"},
            {SixGatesScene::Rest, "six-gates-rest.png", "宁息"},
            {SixGatesScene::HeartIsland, "six-gates-heart-island.png", "心之岛"},
        };

        constexpr double kSceneMatchThreshold = 0.7;

        // 按优先级依次匹配场景模板，命中后点击匹配框中心（关闭随机点击），返回该场景
        SixGatesScene detectSixGatesScene()
        {
            for (const SceneTemplate& templ : kSceneTemplates)
            {
                const std::vector<OpenCvMatch> matches =
                    actions.opencvFindAll(screenshotPath() + templ.fileName, kSceneMatchThreshold, false);
                if (matches.empty())
                {
                    continue;
                }

                // opencvFindAll 按分数降序返回，取最优匹配；randomClick=false 表示点击匹配框中心
                const OpenCvMatch& best = matches.front();
                GameWindow::instance().clickInWindow(utils::makeClickPoint(best.rect, false));
                // 等待场景加载完成后再返回，继续执行对应场景的处理步骤
                Logger::log(QString("识别到六道之门场景: %1，已点击匹配框中心 (%2,%3)")
                            .arg(templ.sceneName).arg(best.center.x).arg(best.center.y));
                waitRandomWithEventProcessing(2000, 200);
                return templ.scene;
            }
            return SixGatesScene::None;
        }

        // 各场景处理逻辑留空，待补充
        // 神秘场景处理（优先级 1，识别到 six-gates-mystery.png 时进入）
        bool handleMysteryScene()
        {
            //1.背包仿造
            if (actions.ocrContainsText("背包仿造", 0.60, QRectF(59.5, 0, 40.5, 100), ocr::Enhance::Upscale))
            {
                //直接退出
                if (skillLevel >= 5)
                {
                    Logger::log(QString("技能已满级，退出仿造"));
                    //点击返回
                    if (!actions.yoloRecognizesAndClick(0.50, true, "common-exit-blue").isEmpty())
                    {
                        Logger::log(QString("点击【返回】失败"));
                        return false;
                    }

                    waitRandomWithEventProcessing(1200, 200);

                    //有确定就点一下，没有则不用管
                    if (actions.yoloRecognizesAndClick(0.50, true, "common-exit-yellow").isEmpty())
                    {
                        Logger::log(QString("点击【确定】"));
                    }

                    waitRandomWithEventProcessing(1200, 200);
                    return true;
                }

                //点击柔风抱暖进行仿造
                if (actions.opencvRecognizesAndClick(screenshotPath() + "six-gates-fighting-skill.png", 0.55, false, true).isEmpty())
                {
                    Logger::log(QString("点击【柔风抱暖】失败"));
                    return false;
                }

                waitRandomWithEventProcessing(1200, 200);

                //确定是否是柔风抱暖
                if (!actions.ocrContainsText("柔风抱暖", 0.60, QRectF(0, 0, 100, 100), ocr::Enhance::Upscale))
                {
                    //找不到则取消选择 直接退出
                    //点击返回
                    if (!actions.yoloRecognizesAndClick(0.50, true, "common-exit-blue").isEmpty())
                    {
                        Logger::log(QString("点击【返回】失败"));
                        return false;
                    }

                    waitRandomWithEventProcessing(1200, 200);

                    //有确定就点一下，没有则不用管
                    if (actions.yoloRecognizesAndClick(0.50, true, "common-exit-yellow").isEmpty())
                    {
                        Logger::log(QString("点击【确定】"));
                    }

                    waitRandomWithEventProcessing(1200, 200);
                }

                //点击仿造
                if (actions.opencvRecognizesAndClick(screenshotPath() + "six-gates-imitation.png", 0.55, false, true).isEmpty())
                {
                    Logger::log(QString("点击【仿造】失败"));
                    return false;
                }

                waitRandomWithEventProcessing(1200, 200);

                if (!actions.yoloRecognizesAndClick(0.50, true, "common-exit-yellow").isEmpty())
                {
                    Logger::log(QString("点击【确定】"));
                }

                waitRandomWithEventProcessing(2000, 200);

                if (!actions.ocrContainsText("仿造成功", 0.55 ,QRectF(0, 0, 100, 100), ocr::Enhance::Upscale))
                {
                    Logger::log(QString("识别【仿造成功】失败"));
                    return false;
                }

                waitRandomWithEventProcessing(1000, 200);

                if (actions.clickInRoi(QRectF(81.3, 22.1, 16.3, 58.8), false).isEmpty())
                {
                    Logger::log(QString("roi点击失败"));
                    return false;
                }
            }

            //2.其他

            return true;
        }

        // 混沌场景处理（优先级 2，识别到 six-gates-chaos.png 时进入）
        bool handleChaosScene()
        {
            QJsonArray ocrData = actions.ocrRecognizes(QRectF(31.5, 26.4, 42.9, 38.0), ocr::Enhance::Upscale);

            //混沌可能有多种
            for (const QJsonValue& val : ocrData) {
                const QJsonObject item = val.toObject();
                //1.幸运宝箱
                if (item["text"].toString() == "幸运宝箱") {
                    const cv::Rect rect = utils::boxToRect(item["box"].toArray());
                    if (rect.width > 0 && rect.height > 0) {
                        GameWindow::instance().clickInWindow(utils::makeClickPoint(rect, false));
                        Logger::log(QString("已点击【幸运宝箱】"));
                    }

                    waitRandomWithEventProcessing(2000,200);

                    //点击开启
                    if (actions.opencvRecognizesAndClick(screenshotPath() + "turn-on.png", 0.75, false, true).isEmpty())
                    {
                        Logger::log(QString("点击【开启】失败"));
                        return false;
                    }

                    waitRandomWithEventProcessing(1200,200);

                    //识别是否有“获得奖励”
                    if (actions.ocrRecognizesAndClick("获得奖励", 0.55, false, QRectF(15.6, 10.4, 67.2, 72.4), ocr::Enhance::Upscale).isEmpty())
                    {
                        Logger::log(QString("点击【获得奖励】失败"));
                        return false;
                    }
                    break;
                }

                //2.其他
            }


            waitRandomWithEventProcessing(3000,200);
            return true;
        }

        // 鏖战场景处理（优先级 3，识别到 six-gates-fighting.png 时进入）
        bool handleFightingScene()
        {
            //只打技能怪物
            bool hasSkill = false;
            for (int i = 0; i < 5; ++i)
            {
                //点击技能
                if (!actions.opencvRecognizesAndClick(screenshotPath() + "six-gates-skill.png", 0.60, false, false).isEmpty())
                {
                    Logger::log(QString("点击【技能】"));
                    hasSkill = true;
                    break;
                }
                waitWithEventProcessing(1000);
            }
            if (!hasSkill)
            {
                Logger::log(QString("无法识别到【技能】"));
                return false;
            }

            waitRandomWithEventProcessing(2000,200);

            //点击挑战
            if (actions.opencvRecognizesAndClick(screenshotPath() + "six-gates-challenge.png", 0.75, false, true).isEmpty())
            {
                Logger::log(QString("点击【挑战】失败"));
                return false;
            }

            //有准备就点一下，没有则不用管
            if (!actions.yoloRecognizesAndClick(0.50, true, "battle-ready").isEmpty())
            {
                Logger::log(QString("点击【准备】"));
            }

            //等待战斗结束 总共30秒
            bool isEndOfBattle = false;
            for (int i = 0; i < 15; ++i)
            {
                //识别是否有“获得奖励”
                QJsonArray ocrData = actions.ocrRecognizes(QRectF(15.6, 10.4, 67.2, 72.4), ocr::Enhance::Upscale);
                for (const QJsonValue& val : ocrData) {
                    const QJsonObject item = val.toObject();
                    if (item["text"].toString() == "获得奖励") {
                        const cv::Rect rect = utils::boxToRect(item["box"].toArray());
                        if (rect.width > 0 && rect.height > 0) {
                            waitWithEventProcessing(2000);
                            GameWindow::instance().clickInWindow(utils::makeClickPoint(rect, true));
                            isEndOfBattle = true;
                            Logger::log(QString("点击【获得奖励】"));
                        }
                        break;
                    }
                }

                waitWithEventProcessing(2000);
            }
            if (!isEndOfBattle)
            {
                Logger::log(QString("等待战斗结束超时"));
            }

            //开始选择技能
            if (!selectSixGatesSkill("柔风抱暖"))
            {
                Logger::log(QString("技能选择异常"));
                return false;
            }

            //识别是否有“获得奖励”
            if (actions.ocrRecognizesAndClick("获得奖励", 0.55, false, QRectF(15.6, 10.4, 67.2, 72.4), ocr::Enhance::Upscale).isEmpty())
            {
                Logger::log(QString("点击【获得奖励】失败"));
                return false;
            }

            waitRandomWithEventProcessing(1200, 200);

            return true;
        }

        // 宁息场景处理（优先级 4，识别到 six-gates-rest.png 时进入）
        bool handleRestScene()
        {
            return true;
        }

        // 心之岛场景处理（优先级 5，识别到 six-gates-heart-island.png 时进入）
        bool handleHeartIslandScene()
        {
            //点击左侧招福达摩，不打星之子
            if (actions.ocrRecognizesAndClick("招福达摩", 0.55, false, QRectF(27.0, 34.3, 46.9, 39.3), ocr::Enhance::Upscale).isEmpty())
            {
                Logger::log(QString("点击【招福达摩】失败"));
                return false;
            }

            waitRandomWithEventProcessing(2000,200);

            //点击挑战
            if (actions.opencvRecognizesAndClick(screenshotPath() + "six-gates-challenge.png", 0.75, false, true).isEmpty())
            {
                Logger::log(QString("点击【挑战】失败"));
                return false;
            }

            waitRandomWithEventProcessing(2000,200);

            //有准备就点一下，没有则不用管
            if (actions.yoloRecognizesAndClick(0.50, true, "battle-ready").isEmpty())
            {
                Logger::log(QString("点击【准备】"));
            }

            //等待战斗结束 总共30秒
            bool isEndOfBattle = false;
            for (int i = 0; i < 15; ++i)
            {
                //识别是否有“获得奖励”
                QJsonArray ocrData = actions.ocrRecognizes(QRectF(15.6, 10.4, 67.2, 72.4), ocr::Enhance::Upscale);
                for (const QJsonValue& val : ocrData) {
                    const QJsonObject item = val.toObject();
                    if (item["text"].toString() == "获得奖励") {
                        const cv::Rect rect = utils::boxToRect(item["box"].toArray());
                        if (rect.width > 0 && rect.height > 0) {
                            waitWithEventProcessing(2000);
                            GameWindow::instance().clickInWindow(utils::makeClickPoint(rect, true));
                            isEndOfBattle = true;
                            Logger::log(QString("点击【获得奖励】"));
                        }
                        break;
                    }
                }

                waitWithEventProcessing(2000);
            }
            if (!isEndOfBattle)
            {
                Logger::log(QString("等待战斗结束超时"));
            }

            waitRandomWithEventProcessing(3000,200);
            return true;
        }
    } // namespace

    bool selectSixGatesSkill(const QString& skillName)
    {
        // 三张技能卡的「选择」按钮中，点击与技能标题水平位置最近的一项；
        // 随机点击并排除按钮边框（0.1/0.1/0.1/0.1），避免点到描边等无效区域
        constexpr ClickExclude chooseExclude{0.1, 0.1, 0.1, 0.1};
        if (skillLevel >= 5)
        {
            if (actions.ocrRecognizesAndClickNearestX("万相之赐", skillName, 0.55, true,
                                                  QRectF(0, 0, 100, 100), ocr::Enhance::Upscale,
                                                  chooseExclude).isEmpty())
            {
                Logger::log(QString("无法识别到【万相之赐】按钮"));
                return false;
            }
            Logger::log(QString("选择【万相之赐】"));
            return true;
        }
        if (actions.ocrRecognizesAndClickNearestX("选择", skillName, 0.55, true,
                                                  QRectF(0, 0, 100, 100), ocr::Enhance::Upscale,
                                                  chooseExclude).isEmpty())
        {
            Logger::log(QString("未找到并点击技能【%1】的【选择】").arg(skillName));
            //寻找刷新按钮
            if (actions.opencvRecognizesAndClick(screenshotPath() + "six-gates-refresh.png", 0.65, false, true).isEmpty())
            {
                Logger::log(QString("点击【刷新】失败"));
                return false;
            }

            //点击确定
            if (!actions.yoloRecognizesAndClick(0.80, false, "common-btn-yellow_confirm").isEmpty())
            {
                Logger::log(QString("点击【确定】"));
                // return false;
            }

            //再次查看 柔风抱暖
            if (actions.ocrRecognizesAndClickNearestX("选择", skillName, 0.55, true,
                                                  QRectF(0, 0, 100, 100), ocr::Enhance::Upscale,
                                                  chooseExclude).isEmpty())
            {
                Logger::log(QString("未找到并点击技能【%1】的【选择】").arg(skillName));
                if (actions.ocrRecognizesAndClickNearestX("万相之赐", skillName, 0.55, true,
                                                  QRectF(0, 0, 100, 100), ocr::Enhance::Upscale,
                                                  chooseExclude).isEmpty())
                {
                    Logger::log(QString("无法识别到【万相之赐】按钮"));
                    return false;
                }
                Logger::log(QString("选择【万相之赐】"));
            }
        }

        skillLevel++;
        Logger::log(QString("已选择技能【%1】，当前技能等级: %2").arg(skillName).arg(skillLevel));
        waitRandomWithEventProcessing(2000, 200);
        return true;
    }

    bool executeSixGates()
    {
        const QString configId = currentItem.id;
        const QString schemeName = getSystemConfigValue(configId, "schemeName", "").toString();
        const QString lineupName = getSystemConfigValue(configId, "lineupName", "").toString();
        Logger::log(QString("六道之门运行配置: 方案=%1 | 阵容=%2")
                    .arg(schemeName, lineupName));

        Logger::log(QString("开始执行六道之门"));

        //检测是否在月之海
        if (!actions.ocrContainsText("月之海", 0.5, QRectF(6.2, 0, 11.8, 11.6), ocr::Enhance::Upscale))
        {
            Logger::log(QString("不在月之海界面"));
            return false;
        }

        //判断是否选中椒图
        auto detections = actions.opencvFindAll(screenshotPath() + "jiaotu.png", 0.75, false);
        if (detections.empty()) {
            Logger::log(QString("需要切换至椒图"));
            waitRandomWithEventProcessing(1200,200);
            if (actions.clickInRoi(QRectF(3.1, 84.9, 5.9, 9.8), false).isEmpty())
            {
                Logger::log(QString("roi点击失败"));
                return false;
            }
            waitRandomWithEventProcessing(2000,200);
            if (!actions.ocrContainsText("椒图", 0.55, QRectF(41.9, 25.9, 16.6, 44), ocr::Enhance::Upscale | ocr::Enhance::AutoInvert))
            {
                Logger::log(QString("未识别到[椒图]"));
                return false;
            }
            if (actions.clickInRoi(QRectF(43.3, 54.7, 14.2, 20.7), true).isEmpty())
            {
                Logger::log(QString("选中椒图失败"));
                return false;
            }
        }
        Logger::log(QString("已选中椒图"));
        waitRandomWithEventProcessing(1200,200);

        //点击开启
        if (actions.opencvRecognizesAndClick(screenshotPath() + "turn-on.png", 0.75, false, true).isEmpty())
        {
            Logger::log(QString("点击【开启】失败"));
            return false;
        }

        waitRandomWithEventProcessing(5000,500);

        //点击式神录切换御魂
        if (actions.ocrRecognizesAndClick("式神录", 0.55, false, QRectF(86.1, 60.2, 13.2, 19.1), ocr::Enhance::Upscale | ocr::Enhance::AutoInvert).isEmpty())
        {
            Logger::log(QString("点击【式神录】失败"));
            return false;
        }

        waitRandomWithEventProcessing(1200,200);

        //切换御魂流程
        //点击预设
        if (actions.ocrRecognizesAndClick("预设", 0.55, true, QRectF(25.5, 11.4, 7.7, 8.8), ocr::Enhance::Upscale).isEmpty())
        {
            Logger::log(QString("点击【预设】失败"));
            return false;
        }

        waitRandomWithEventProcessing(1200,200);

        //滑动到分组列表顶部
        utils::OcrTextTracker groupTopTracker;
        for (int i = 0; i < 10; ++i)
        {
            QJsonArray groupData = actions.ocrRecognizes(QRectF(84.7, 13.7, 12.5, 72.5), ocr::Enhance::Upscale);

            //文本内容与上次一致（忽略 box/score 波动），说明滑动未改变列表内容，已到顶部，结束循环
            if (groupTopTracker.unchangedSinceLast(groupData)) {
                Logger::log(QString("分组列表别结果与上次一致，已滑动到顶部"));
                break;
            }

            //向上滑动
            if (actions.swipeBetweenPoints(QPointF(91.5, 27.5), QPointF(91.1, 67.8),SETTING_CONFIG.getMouseControlMode(), 500).isEmpty())
            {
                Logger::log(QString("滑动失败"));
                return false;
            }
            //等列表滚动动画结束再识别，否则可能读到滑动前的旧画面
            waitRandomWithEventProcessing(2000,200);
        }

        //寻找目标分组列表
        bool foundGrouping = false;
        utils::OcrTextTracker groupingTracker;
        for (int i = 0; i < 10; ++i)
        {
            QJsonArray groupingData = actions.ocrRecognizes(QRectF(84.7, 13.7, 12.5, 72.5), ocr::Enhance::Upscale);

            //文本内容与上次一致（忽略 box/score 波动），说明滑动未改变列表内容，已到底部，结束循环
            if (groupingTracker.unchangedSinceLast(groupingData)) {
                Logger::log(QString("分组列表识别结果与上次一致，已滑动到底部"));
                break;
            }

            for (const QJsonValue& val : groupingData) {
                const QJsonObject item = val.toObject();
                if (item["text"].toString() == schemeName) {
                    const cv::Rect rect = utils::boxToRect(item["box"].toArray());
                    if (rect.width > 0 && rect.height > 0) {
                        GameWindow::instance().clickInWindow(utils::makeClickPoint(rect, true));
                        foundGrouping = true;
                        Logger::log(QString("已点击分组: %1").arg(schemeName));
                    }
                    break;
                }
            }

            //向下滑动
            if (actions.swipeBetweenPoints(QPointF(91.1, 67.8),QPointF(91.5, 27.5), SETTING_CONFIG.getMouseControlMode(), 500).isEmpty())
            {
                Logger::log(QString("滑动失败"));
                return false;
            }
            waitRandomWithEventProcessing(2000,200);

            if (foundGrouping) {
                //找到数据结束循环
                break;
            }
        }
        if (!foundGrouping) {
            Logger::log(QString("未找到分组: %1").arg(schemeName));
            return false;
        }

        waitWithEventProcessing(1500);

        //先回到列表顶部，避免目标预设在上方时漏检
        utils::OcrTextTracker presetTopTracker;
        for (int i = 0; i < 10; ++i)
        {
            QJsonArray presetData = actions.ocrRecognizes(QRectF(40, 20.3, 28.4, 59.7), ocr::Enhance::Upscale);

            //文本内容与上次一致（忽略 box/score 波动），说明滑动未改变列表内容，已到顶部，结束循环
            if (presetTopTracker.unchangedSinceLast(presetData)) {
                Logger::log(QString("预设列表识别结果与上次一致，已滑动到顶部"));
                break;
            }

            //向上滑动
            if (actions.swipeBetweenPoints(QPointF(58, 21), QPointF(58, 63),SETTING_CONFIG.getMouseControlMode(), 500).isEmpty())
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
        for (int i = 0; i < 10; ++i)
        {
            QJsonArray presetData = actions.ocrRecognizes(QRectF(40, 20.3, 28.4, 59.7), ocr::Enhance::Upscale);
            qDebug() << "presetData:" << presetData;

            //文本内容与上次一致（忽略 box/score 波动），说明滑动未改变列表内容，已到底部，结束循环
            if (presetSearchTracker.unchangedSinceLast(presetData)) {
                Logger::log(QString("预设列表识别结果与上次一致，已滑动到底部"));
                break;
            }

            for (const QJsonValue& val : presetData) {
                const QJsonObject item = val.toObject();
                if (item["text"].toString() == lineupName) {
                    targetPresetBox = item["box"].toArray();
                    break;
                }
            }

            if (targetPresetBox.size() > 1) {
                Logger::log(QString("找到预设阵容: %1").arg(lineupName));
                foundPresets = true;
                break;
            }

            //向下滑动
            if (actions.swipeBetweenPoints(QPointF(58, 63),QPointF(58, 21),SETTING_CONFIG.getMouseControlMode(), 500).isEmpty())
            {
                Logger::log(QString("滑动失败"));
                return false;
            }
            waitRandomWithEventProcessing(2000,200);
        }
        if (!foundPresets) {
            Logger::log(QString("未找到预设: %1").arg(lineupName));
            return false;
        }

        const cv::Rect presetRect = utils::boxToRect(targetPresetBox);
        if (presetRect.width <= 0 || presetRect.height <= 0) {
            Logger::log(QString("预设阵容 box 解析失败: %1").arg(lineupName));
            return false;
        }

        // 识别所有预设队伍切换按钮
        detections = actions.opencvFindAll(screenshotPath() + "switch-soul.png", 0.6, false);
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

        waitRandomWithEventProcessing(2000,200);

        // 点击返回
        if (actions.yoloRecognizesAndClick(0.60, false, "common-exit-yellow").isEmpty())
        {
            Logger::log(QString("无法识别到【返回】按钮"));
            return false;
        }

        waitRandomWithEventProcessing(2000,200);

        //点击确认
        if (actions.opencvRecognizesAndClick(screenshotPath() + "confirm.png", 0.75, false, true).isEmpty())
        {
            Logger::log(QString("点击【确认】失败"));
            return false;
        }

        waitRandomWithEventProcessing(2000,200);

        //点击开启
        if (actions.opencvRecognizesAndClick(screenshotPath() + "turn-on.png", 0.75, false, true).isEmpty())
        {
            Logger::log(QString("点击【开启】失败"));
            return false;
        }

        waitRandomWithEventProcessing(2000,200);

        //点击确认开启副本
        if (actions.yoloContainsLabels(0.60, {"common-popup-confirm"}, false))
        {
            if (actions.yoloRecognizesAndClick(0.80, false, "common-btn-yellow_confirm").isEmpty())
            {
                Logger::log(QString("无法识别到【确定】按钮"));
                return false;
            }
        }

        waitRandomWithEventProcessing(2000,200);

        //点击跳过
        if (!actions.ocrRecognizesAndClick("跳过", 0.55, true, QRectF(25.5, 11.4, 7.7, 8.8), ocr::Enhance::Upscale).isEmpty())
        {
            Logger::log(QString("点击【跳过】"));

            waitRandomWithEventProcessing(2000,200);

            //点击确认跳过
            if (actions.yoloContainsLabels(0.60, {"common-popup-confirm"}, false))
            {
                if (actions.yoloRecognizesAndClick(0.80, false, "common-btn-yellow_confirm").isEmpty())
                {
                    Logger::log(QString("无法识别到【确定】按钮"));
                    return false;
                }
            }
        }else
        {
            Logger::log(QString("无需【跳过】"));
        }

        waitRandomWithEventProcessing(3000,200);

        //选择技能：柔风抱暖
        if (!selectSixGatesSkill("柔风抱暖"))
        {
            Logger::log(QString("技能选择异常"));
            return false;
        }

        //开始识别场景
        for (int i = 0; i < ROUND_TOTAL; ++i)
        {
            if (roundNum == 24 && skillLevel < 4)
            {
                if (actions.ocrRecognizesAndClick("唤息", 0.55, false, QRectF(78.9, 71.2, 21.1, 28.8), ocr::Enhance::Upscale | ocr::Enhance::AutoInvert).isEmpty())
                {
                    Logger::log(QString("无法识别到【唤息】按钮"));
                    return false;
                }

                waitRandomWithEventProcessing(1200,200);

                if (actions.yoloRecognizesAndClick(0.80, false, "common-btn-yellow_confirm").isEmpty())
                {
                    Logger::log(QString("无法识别到【确定】按钮"));
                    return false;
                }

                waitRandomWithEventProcessing(1200,200);
            }
            // 同一画面可能同时出现多种场景，按优先级进入第一个命中的场景；
            // 心之岛只会单独出现，仍放在最低优先级兜底
            bool handled = false;
            // 优先级顺序：1.神秘 → 2.混沌 → 3.鏖战 → 4.宁息 → 5.心之岛
            switch (detectSixGatesScene())
            {
                case SixGatesScene::Mystery:
                    handled = handleMysteryScene();
                    break;
                case SixGatesScene::Chaos:
                    handled = handleChaosScene();
                    break;
                case SixGatesScene::Fighting:
                    handled = handleFightingScene();
                    break;
                case SixGatesScene::Rest:
                    handled = handleRestScene();
                    break;
                case SixGatesScene::HeartIsland:
                    handled = handleHeartIslandScene();
                    break;
                case SixGatesScene::None:
                default:
                    Logger::log(QString("未识别到六道之门场景"));
                    return false;
            }
            if (!handled)
            {
                return false;
            }
            roundNum++;
            Logger::log(QString("当前回合数 (%1/%2)").arg(i+1).arg(ROUND_TOTAL));
        }

        //进入boss战
        Logger::log(QString("当前技能等级: %1, 准备进入boss战").arg(skillLevel));
        if (skillLevel < 4)
        {
            Logger::log(QString("注意！当前技能等级过低"));
        }
        waitRandomWithEventProcessing(3000,500);

        //点击挑战
        if (actions.opencvRecognizesAndClick(screenshotPath() + "six-gates-challenge.png", 0.60, false, true).isEmpty())
        {
            Logger::log(QString("点击【挑战】失败"));
            return false;
        }

        waitWithEventProcessing(6000);

        //有准备就点一下，没有则不用管
        if (!actions.yoloRecognizesAndClick(0.50, true, "battle-ready").isEmpty())
        {
            Logger::log(QString("点击【准备】"));
        }

        //等待战斗结束 总共180秒
        bool isEndOfBattle = false;
        for (int i = 0; i < 60; ++i)
        {
            //TODO 待修改

            waitWithEventProcessing(3000);
        }
        if (!isEndOfBattle)
        {
            Logger::log(QString("等待战斗结束超时"));
        }

        waitWithEventProcessing(3000);

        Logger::log(QString("六道之门方案已完成"));
        return true;
    }

    // 注册为系统方案：TaskRunner 按注册表调度（注册方式见 ScenarioRegistry.h）。
    // 当前 config.json 未包含该方案，处于休眠状态；恢复使用只需把方案条目加回 config.json。
    REGISTER_SCENARIO("SYSTEM_SIX_GATES", executeSixGates, resetSixGatesStatus, false)

} // namespace scenarios
