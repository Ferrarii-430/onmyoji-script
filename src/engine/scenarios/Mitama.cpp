//
// Created by CZY on 2026/8/3.
//

#include "Mitama.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/engine/ScriptActions.h"

using core::waitWithEventProcessing;

namespace scenarios
{

    namespace {}
    ScriptActions& actions = ScriptActions::instance();
    int currentInterface = 0;

    void executeMitama()
    {
        Logger::log(QString("开始执行御魂副本"));

        const QString configId = currentItem.id;
        const bool teamPlay = getSystemConfigValue(configId, "teamPlay", true).toBool(true);
        Logger::log(QString("自动御魂运行配置: 组队模式=%1").arg(teamPlay));
        currentInterface = getCurrentInterface();
        Logger::log(QString("当前处于场景: %1").arg(currentInterface));

        QString screenshotPath = AppPaths::instance().screenshotPath();
        QString savePath = actions.opencvRecognizesAndClick(screenshotPath + "accept_invitation.png", 0.8, false);
        if (!savePath.isEmpty())
        {
            Logger::log(QString("<UNK>: %1").arg(savePath));
        }
    }

    /**
     * 判断目前在哪一个场景
     * @return currentInterface
     */
    int getCurrentInterface()
    {
        //1.御魂副本选择界面 2.队伍界面 3.战斗界面 4.奖励领取界面 5.收到悬赏封印 6.其他界面(直接停止任务)
        // QJsonArray interfaceData = actions.ocrRecognizes(QRectF(0, 0, 20, 15));
        // for (int i = 0; i < interfaceData.size(); ++i)
        // {
        //     QJsonObject item = interfaceData[i].toObject();
        //     QString text = item["text"].toString();
        //
        //     if (text.contains("协战队伍"))
        //     {
        //         return 2; //队伍界面
        //     }
        //
        //     if (text.contains("御魂"))
        //     {
        //         return 1; //御魂副本选择界面
        //     }
        // }

        const auto detections = actions.yoloRecognizes(0.5, 0, 0);
        for (const auto& det : detections) {
            if (comparesEqual(det.className, "common-exit-battle")) {
                return 3; //战斗界面
            }

            if (comparesEqual(det.className, "battle-ready")) {
                return 3; //战斗界面
            }

            if (comparesEqual(det.className, "common-btn-red_x_solid"))
            {
                return 5; //收到悬赏封印
            }
        }

        QString screenshotPath = AppPaths::instance().screenshotPath();
        QString savePath = actions.opencvRecognizesAndClick(screenshotPath + "battle_end.png", 0.65, true);
        if (!savePath.isEmpty())
        {
            return 4; //奖励领取界面
        }

        return 6; //其他界面(直接停止任务)
    }

} // namespace scenarios
