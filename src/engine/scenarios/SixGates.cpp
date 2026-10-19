//
// Created by CZY on 2026/10/17.
//
#include "SixGates.h"

#include "ScenarioRegistry.h"

#include <QString>

#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/engine/ScriptActions.h"
#include "src/engine/TaskRunner.h"

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
        ScriptActions& actions = ScriptActions::instance();
        QString screenshotPath = AppPaths::instance().screenshotPath();
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
            if (!actions.ocrContainsText("椒图", 0.55, QRectF(41.9, 25.9, 16.6, 44), ocr::Enhance::Upscale))
            {
                Logger::log(QString("未识别到[椒图]"));
                return false;
            }
            if (!actions.clickInRoi(QRectF(43.3, 54.7, 14.2, 20.7), true).isEmpty())
            {
                Logger::log(QString("选中椒图失败"));
                return false;
            }
        }
        Logger::log(QString("已选中椒图"));
        waitRandomWithEventProcessing(1200,200);

        //点击式神录切换御魂

        // TODO: 六道之门单轮流程待实现，参考 Anniversary999.cpp 的步骤骨架：
        //   1. 首轮初始化（用 sixGates_isRoundInitialized 判断是否为当次任务的第一局）
        //   2. 识别并点击进入活动/挑战
        //   3. 等待进入战斗（yoloContainsLabels 等）
        //   4. 循环等待战斗结束
        //   5. 领取奖励 / 结算，本局完成后 return true 进入下一轮
        // 失败路径 return false 终止任务循环；各步骤间穿插 hasCollaboration() 处理协作弹窗

        Logger::log(QString("六道之门方案已完成"));
        return true;
    }

    // 注册为系统方案：TaskRunner 按注册表调度（注册方式见 ScenarioRegistry.h）。
    // 当前 config.json 未包含该方案，处于休眠状态；恢复使用只需把方案条目加回 config.json。
    REGISTER_SCENARIO("SYSTEM_SIX_GATES", executeSixGates, resetSixGatesStatus, false)

} // namespace scenarios
