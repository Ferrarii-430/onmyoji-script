//
// Created by CZY on 2026/8/3.
//

#include "Mitama.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/engine/ScriptActions.h"

using core::waitWithEventProcessing;

namespace scenarios
{

    namespace {}

    void executeMitama()
    {
        ScriptActions& actions = ScriptActions::instance();
        Logger::log(QString("开始执行御魂副本"));

        const QString configId = currentItem.id;
        const bool teamPlay    = getSystemConfigValue(configId, "teamPlay", true).toBool(true);
        Logger::log(QString("自动御魂运行配置: 组队模式=%1").arg(teamPlay));

        //先判断目前在哪一个场景
        //1.御魂副本选择界面 2.队伍界面 3.战斗界面 4.奖励领取界面 5.其他界面(直接停止任务)
        QJsonArray interfaceData = actions.ocrRecognizes(QRectF(0, 0, 0, 0));
        for (int i = 0; i < interfaceData.size(); ++i)
        {
            QJsonObject item = interfaceData[i].toObject();
            QString text = item["text"].toString();
        }
    }

} // namespace scenarios
