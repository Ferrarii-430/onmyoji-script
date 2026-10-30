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
    int skillLevel = 0;

    // 重置跨轮次状态：任务结束/停止后由 TaskRunner 调用，下次启动任务重新走初始化流程
    void resetSixGatesStatus()
    {
        sixGates_isRoundInitialized = false;
        skillLevel = 0;
    }

    bool executeSixGates()
    {
        const QString configId = currentItem.id;
        ScriptActions& actions = ScriptActions::instance();
        QString screenshotPath = AppPaths::instance().screenshotPath();
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
        auto detections = actions.opencvFindAll(screenshotPath + "jiaotu.png", 0.75, false);
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
        if (actions.opencvRecognizesAndClick(screenshotPath + "turn-on.png", 0.75, false, true).isEmpty())
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
        detections = actions.opencvFindAll(screenshotPath + "switch-soul.png", 0.6, false);
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
        if (actions.opencvRecognizesAndClick(screenshotPath + "confirm.png", 0.75, false, true).isEmpty())
        {
            Logger::log(QString("点击【确认】失败"));
            return false;
        }

        waitRandomWithEventProcessing(2000,200);

        //点击开启
        if (actions.opencvRecognizesAndClick(screenshotPath + "turn-on.png", 0.75, false, true).isEmpty())
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

        //选择柔风抱暖：三张卡片的「选择」按钮中，点击与标题水平位置最近的一项；
        //随机点击并排除按钮边框（0.3/0.2/0.1/0.1），避免点到描边等无效区域
        constexpr ClickExclude chooseExclude{0.1, 0.1, 0.1, 0.1};
        if (actions.ocrRecognizesAndClickNearestX("选择", "柔风抱暖", 0.55, true,
                                                  QRectF(0, 0, 100, 100), ocr::Enhance::Upscale,
                                                  chooseExclude).isEmpty())
        {
            Logger::log(QString("未找到并点击【选择】"));
            return false;
        }

        waitRandomWithEventProcessing(2000,200);

        Logger::log(QString("六道之门方案已完成"));
        return true;
    }

    // 注册为系统方案：TaskRunner 按注册表调度（注册方式见 ScenarioRegistry.h）。
    // 当前 config.json 未包含该方案，处于休眠状态；恢复使用只需把方案条目加回 config.json。
    REGISTER_SCENARIO("SYSTEM_SIX_GATES", executeSixGates, resetSixGatesStatus, false)

} // namespace scenarios
