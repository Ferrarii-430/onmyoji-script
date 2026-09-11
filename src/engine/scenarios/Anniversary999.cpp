#include "Anniversary999.h"

#include <QJsonObject>
#include <QRandomGenerator>
#include <QString>
#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/engine/ScriptActions.h"

using core::waitWithEventProcessing;
using core::waitRandomWithEventProcessing;

namespace scenarios {

    static bool isCastingLocked = false;

    bool executeAnniversary999()
    {
        ScriptActions& actions = ScriptActions::instance();
        Logger::log(QString("开始执行十周年999"));
        QString screenshotPath = AppPaths::instance().screenshotPath();

        // //步骤0 判断是否锁定
        // if (!isCastingLocked)
        // {
        //     QString savePathCommonUnlock = actions.opencvRecognizesAndClick(screenshotPath + "common-unlock.png", 0.65, false, true);
        //     if (!savePathCommonUnlock.isEmpty())
        //     {
        //         Logger::log(QString("阵容已锁定"));
        //     }else
        //     {
        //         if (!hasCollaboration())
        //         {
        //             Logger::log(QString("【阵容锁定】失败"));
        //             return false;
        //         }else
        //         {
        //             QString savePathCommonUnlockRe = actions.opencvRecognizesAndClick(screenshotPath + "common-unlock.png", 0.65, false, true);
        //             if (!savePathCommonUnlockRe.isEmpty())
        //             {
        //                 Logger::log(QString("重试 阵容已锁定"));
        //             }else
        //             {
        //                 {
        //                     Logger::log(QString("重试 【阵容锁定】失败"));
        //                     return false;
        //                 }
        //             }
        //         }
        //     }
        //     isCastingLocked = true;
        //     waitRandomWithEventProcessing(1000,200);
        // }

        //步骤1 点击挑战进入战斗
        constexpr ClickExclude openCvAnniversaryChallengeRecordExclude{0.1, 0.1, 0.1, 0.1};
        QString savePathAnniversaryChallenge = actions.opencvRecognizesAndClick(screenshotPath + "anniversary-challenge.png", 0.65, true, false, openCvAnniversaryChallengeRecordExclude);
        if (!savePathAnniversaryChallenge.isEmpty())
        {
            Logger::log(QString("点击【挑战】成功"));
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
                if (!savePathAnniversaryChallengeRe.isEmpty())
                {
                    Logger::log(QString("重试 点击【挑战】成功"));

                }else
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

        //先等待2秒
        waitWithEventProcessing(2000);

        //步骤3 循环等待战斗结束
        bool isBattleOfEnd = false;
        for (int i = 0; i < 5; ++i)
        {
            Logger::log(QString("等待战斗结束...(%1/5)").arg(i+1));
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

        //步骤4 判断是否有 获得奖励
        waitRandomWithEventProcessing(2200,300);
        if (actions.ocrContainsText("获得奖励", 0.7, QRectF(11, 9, 79, 83)))
        {
            Logger::log(QString("已识别到【获得奖励】"));
        }else
        {
            hasCollaboration();
        }

        //步骤5 跳过获得奖励
        QString savePathReceiveRewards;

        // 随机生成 0 或 1
        if (QRandomGenerator::global()->bounded(2) == 0)
        {
            savePathReceiveRewards = actions.clickInRoi(QRectF(80, 15, 19, 70), true);
        }else
        {
            savePathReceiveRewards = actions.clickInRoi(QRectF(7, 10, 16, 79), true);
        }

        if (!savePathReceiveRewards.isEmpty())
        {
            Logger::log(QString("点击【获得奖励】成功"));
        }else
        {
            //这里不需要重试
            return false;
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
        if (!path.isEmpty())
        {
            Logger::log(QString("点击取消协作"));
            waitWithEventProcessing(2000);
            return true;
        }
        waitWithEventProcessing(1000);
        return false;
    }

} // namespace scenarios
