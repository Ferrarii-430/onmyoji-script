#include "src/engine/scenarios/Anniversary999.h"

#include <QJsonObject>
#include <QString>
#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/engine/ScriptActions.h"

using core::waitWithEventProcessing;
using core::waitRandomWithEventProcessing;

namespace scenarios {

bool hasCollaboration();

bool executeAnniversary999()
{
    ScriptActions& actions = ScriptActions::instance();
    Logger::log(QString("开始执行十周年999"));
    QString screenshotPath = AppPaths::instance().screenshotPath();

    //步骤1 点击挑战进入战斗
    constexpr ClickExclude openCvAnniversaryChallengeRecordExclude{0.1, 0.1, 0.1, 0.1};
    QString savePathAnniversaryChallenge = actions.opencvRecognizesAndClick(screenshotPath + "anniversary-challenge.png", 0.65, true, false, openCvAnniversaryChallengeRecordExclude);
    if (!savePathAnniversaryChallenge.isEmpty())
    {
        Logger::log(QString("点击【挑战】"));
    }else
    {
        if (!hasCollaboration())
        {
            Logger::log(QString("点击【挑战】失败"));
            return false;
        }else
        {
            //重试执行步骤1
            QString savePathAnniversaryChallengeRe = actions.opencvRecognizesAndClick(screenshotPath + "anniversary-challenge.png", 0.65, true, false);
            if (savePathAnniversaryChallengeRe.isEmpty())
            {
                Logger::log(QString("重试点击【挑战】失败"));
                return false;
            }
        }
    }

    waitWithEventProcessing(500);

    //步骤2 判断是否进入战斗
    bool isEngageInBattle = false;
    for (int i = 0; i < 4; ++i)
    {
        //先等1秒
        waitRandomWithEventProcessing(1000,200);
        if (actions.yoloContainsLabels(0.45, {"common-exit-battle"}, false))
        {
            Logger::log(QString("已进入周年庆999战斗场景"));
            isEngageInBattle = true;
            break;
        }
    }
    if (!isEngageInBattle)
    {
        if (!hasCollaboration())
        {
            Logger::log(QString("等待进入战斗场景超时"));
            return false;
        }else
        {
            //此时无需重试 直接进入步骤3 循环等待战斗结束
        }
    }

    //先等待1秒
    waitWithEventProcessing(1000);

    //步骤3 循环等待战斗结束
    bool isBattleOfEnd = false;
    for (int i = 0; i < 5; ++i)
    {
        waitRandomWithEventProcessing(1000,200);
        if (actions.yoloContainsLabels(0.55, {"common-popup-reward"}, false))
        {
            Logger::log(QString("战斗已经结束，等待领取奖励"));
            isBattleOfEnd = true;
            break;
        }
    }
    if (!isBattleOfEnd)
    {
        if (!hasCollaboration())
        {
            Logger::log(QString("等待战斗结束超时"));
            return false;
        }else
        {
            //此时无需重试 直接判定为战斗完成 进入步骤4
        }
    }

    //步骤4 点击跳过获得奖励
    waitRandomWithEventProcessing(2000,500);
    if (actions.ocrContainsText("获得奖励", 0.75, QRectF(11, 9, 79, 83)))
    {
        QString savePathReceiveRewards = actions.clickInRoi(QRectF(80, 15, 19, 70), true);
        if (!savePathReceiveRewards.isEmpty())
        {
            Logger::log(QString("跳过获得奖励"));
        }else
        {
            if (!hasCollaboration())
            {
                Logger::log(QString("跳过获得奖励失败"));
                return false;
            }else
            {
                //需要重试 判断是否正确跳过获得奖励
                if (actions.ocrContainsText("获得奖励", 0.75, QRectF(11, 9, 79, 83)))
                {
                    QString savePathReceiveRewardsRe = actions.clickInRoi(QRectF(80, 15, 19, 70), true);
                    if (!savePathReceiveRewardsRe.isEmpty())
                    {
                        Logger::log(QString("重试跳过获得奖励"));
                    }else
                    {
                        Logger::log(QString("重试跳过获得奖励失败"));
                        return false;
                    }
                }
            }
        }
    }

    waitRandomWithEventProcessing(2000,1000);

    Logger::log(QString("999活动执行完成"));
    return true;
}


//判断是否有协作
bool hasCollaboration()
{
    ScriptActions& actions = ScriptActions::instance();
    QString path = actions.yoloRecognizesAndClick(0.50, false, "common-btn-red_x_transparent");
    waitWithEventProcessing(2000);
    if (!path.isEmpty())
    {
        Logger::log(QString("点击取消协作"));
        return true;
    }
    return false;
}

} // namespace scenarios
