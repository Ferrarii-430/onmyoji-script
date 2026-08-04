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
    int MAX_WAIT_NUM = 14;
    int MAX_WAIT_TIME = 2000;
    bool autoSendRequestEnable = false; //是否开启自动发送 组队/入队 请求
    bool isCaptain = false; //是否为队长

    void executeMitama()
    {
        Logger::log(QString("开始执行御魂副本"));

        QString screenshotPath = AppPaths::instance().screenshotPath();
        const QString configId = currentItem.id;
        const bool teamPlay = getSystemConfigValue(configId, "teamPlay", true).toBool(true);
        Logger::log(QString("自动御魂运行配置: 组队模式=%1").arg(teamPlay));
        currentInterface = getCurrentInterface();
        Logger::log(QString("当前处于场景: %1").arg(currentInterface));

        if (currentInterface == 6)
        {
            Logger::log(QString("无法处理目前的场景，请从选择御魂副本、战斗、组队界面启动"));
            return;
        }

        // 根据当前场景跳转到对应流程节点继续执行
        // 场景: 1=御魂副本选择 2=队伍界面 3=战斗界面 4=奖励领取界面
        // 流程: settle(点击挑战) -> ready(点击准备) -> battle(等待战斗结束) -> team(组队邀请/收尾)
        switch (currentInterface)
        {
            case 1: // 御魂副本选择界面（单人模式起点）
                if (teamPlay)
                {
                    Logger::log(QString("组队模式无法从当前场景: %1 启动").arg(currentInterface));
                    return;
                }
                goto settle;
            case 2: // 队伍界面（组队模式起点）
                goto settle;
            case 3: // 战斗界面，跳过挑战/准备，直接等待战斗结束
                goto battle;
            case 4: // 奖励领取界面，战斗已结束，进入组队/收尾流程
                goto team;
            default:
                return;
        }

    settle:
        // 点击右下角挑战按钮，进入战斗
        // 单人模式默认从御魂副本界面开始，组队模式默认从组队界面开始
        // 但都可以用同一个逻辑，判断右下角的挑战按钮
        {
            if (teamPlay)
            {
                //组队模式
                //此时需要判断能否点击 组队模式下要等人齐才能点挑战

                //先判断是不是队长
                QJsonArray teamData = actions.ocrRecognizes(QRectF(80, 65, 100, 100));
                for (int i = 0; i < teamData.size(); ++i)
                {
                    QJsonObject item = teamData[i].toObject();
                    QString text = item["text"].toString();

                    if (text.contains("战"))
                    {
                        isCaptain = true;
                        break;
                    }
                    isCaptain = false;
                }

                if (isCaptain)
                {
                    // 队长状态
                    for (int i = 0; i < MAX_WAIT_NUM; ++i)
                    {
                        //循环等待队友进入
                        waitWithEventProcessing(MAX_WAIT_TIME);
                    }
                } else
                {
                    // 队友状态
                    for (int i = 0; i < MAX_WAIT_NUM; ++i)
                    {
                        //循环等待队长邀请
                        waitWithEventProcessing(MAX_WAIT_TIME);
                    }
                }
            } else
            {
                //单人模式
                const QString settled = actions.ocrRecognizesAndClickAny({"挑战"}, 0.8, true, QRectF(80, 65, 100, 100));
                if (!settled.isEmpty())
                {
                    Logger::log(QString("点击挑战"));
                    waitWithEventProcessing(4000);
                    isCaptain = true;
                } else
                {
                    isCaptain = false;
                }
            }
        }

    ready:
        // 如果有准备按钮则点击，否则无视，因为正常会手动点锁定
        actions.yoloRecognizesAndClick(0.60, false, "battle-ready", 0, 0);
        waitWithEventProcessing(4000);

    battle:
        // 判断是否已经进入战斗界面
        if (!actions.yoloContainsLabels(0.55, {"common-exit-battle"}, false))
        {
            //未进入
            Logger::log(QString("未进入战斗，任务结束"));
            return;
        }

        //循环等待战斗结束
        {
            bool isError = true;
            for (int i = 0; i < MAX_WAIT_NUM; ++i)
            {
                QString savePathBattleEnd = actions.opencvRecognizesAndClick(screenshotPath + "battle_end.png", 0.65, true);
                waitWithEventProcessing(MAX_WAIT_TIME);
                if (!savePathBattleEnd.isEmpty())
                {
                    if (isCaptain)
                    {
                        if (!autoSendRequestEnable)
                        {
                            waitWithEventProcessing(2000);
                            //未开启自动邀请时 这里会弹出是否自动邀请
                            actions.yoloRecognizesAndClick(0.60, false, "battle-ready", 0, 0);
                        }
                    }
                    isError = false;
                    break;
                }
            }

            if (isError)
            {
                Logger::log(QString("等待战斗完成超时"));
                return;
            }
        }

        waitWithEventProcessing(4000);

    team:
        //组队模式下 要进行区分 队长要发送邀请 队员则要等待接受邀请
        //单人模式下 会直接退到御魂副本界面 则无需额外判断
        if (teamPlay)
        {
            //组队模式
            bool isError = true;
            for (int i = 0; i < MAX_WAIT_NUM; ++i)
            {
                if (isCaptain)
                {
                    // 队长状态 要发送邀请队友入队
                    if (autoSendRequestEnable)
                    {
                        //自动发送邀请 等待队友入队 进入战斗
                        //需要添加识别战斗按钮 按钮有两个状态 【可点击】【不可点击】
                    } else
                    {
                        //正常都是开启的，如果未开启需要手动邀请
                        Logger::log(QString("未开启自动邀请，需手动邀请队友"));
                    }
                } else
                {
                    //队友状态 等待队长发送邀请
                    if (autoSendRequestEnable)
                    {
                        //1. 第一次会退到主界面等待邀请
                        // 循环等待队长发送邀请
                        QString savePath = actions.opencvRecognizesAndClick(screenshotPath + "accept_invitation.png", 0.8, false);
                        if (!savePath.isEmpty())
                        {
                            autoSendRequestEnable = true;
                            isError = false;
                            Logger::log(QString("自动入队已开启"));
                            break;
                        }
                    } else
                    {
                        // 2. 处于自动入队 此时会在等待入队界面 等待队长邀请进入队伍 进入战斗
                    }
                }
                waitWithEventProcessing(MAX_WAIT_TIME);
            }
            if (isError)
            {
                Logger::log(QString("组队等待超时"));
            }
        }
        //单机模式下到这里就可以停止逻辑，会自动退到御魂副本界面，从头开始执行逻辑进入战斗即可
    }

    /**
     * 判断目前在哪一个场景
     * @return currentInterface
     */
    int getCurrentInterface()
    {
        //1.御魂副本选择界面 2.队伍界面 3.战斗界面 4.奖励领取界面 5.收到悬赏封印 6.其他界面(直接停止任务)
        QJsonArray interfaceData = actions.ocrRecognizes(QRectF(0, 0, 20, 15));
        for (int i = 0; i < interfaceData.size(); ++i)
        {
            QJsonObject item = interfaceData[i].toObject();
            QString text = item["text"].toString();

            if (text.contains("协战队伍"))
            {
                return 2; //队伍界面
            }

            if (text.contains("御魂"))
            {
                return 1; //御魂副本选择界面
            }
        }

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
                //收到悬赏封印
                processingPopUpWindow(); // 直接处理掉悬赏封印
                return getCurrentInterface(); // 再重新返回一个新的场景
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

    /**
     * 处理悬赏封印弹窗
     * @return
     */
    bool processingPopUpWindow()
    {
        QString path = actions.yoloRecognizesAndClick(0.60, false, "common-btn-red_x_solid", 0, 0);
        waitWithEventProcessing(2000);
        return !path.isEmpty();
    }

} // namespace scenarios
