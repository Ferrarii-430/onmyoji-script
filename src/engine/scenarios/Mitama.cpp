//
// Created by CZY on 2026/8/3.
//

#include "Mitama.h"
#include "ScenarioRegistry.h"

#include <QJsonObject>
#include <QRandomGenerator>
#include <QString>
#include <QStringList>

#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/engine/ScriptActions.h"
#include "src/engine/TaskRunner.h"

using core::waitWithEventProcessing;
using core::waitRandomWithEventProcessing;

namespace scenarios
{

    namespace {}
    // 按线程隔离：每个会话绑定自己的 ScriptActions（内部持有该线程的截图与点击上下文）
    static thread_local ScriptActions& actions = ScriptActions::instance();
    static thread_local int currentInterface = 0;
    static thread_local int MAX_WAIT_NUM = 15;
    static thread_local int MAX_WAIT_TIME = 2000; //最长25秒
    static thread_local bool autoSendRequestEnable = false; //是否开启自动发送 组队/入队 请求
    static thread_local bool isCaptain = false; //是否为队长
    static thread_local bool mitama_isCastingLocked = false; //阵容是否锁定
    static thread_local int roomVacancy = -1; //上一轮记录的队伍空位数量，-1表示首次执行、暂无参考数据

    // 重置跨轮次状态：阵容锁定标记只在单次任务运行内保持，
    // 任务结束/停止后重置，下次启动任务重新执行锁定流程
    void resetMitamaStatus()
    {
        mitama_isCastingLocked = false;
        roomVacancy = -1; // 空位数据只在单次任务运行内有效，下次启动任务重新视为首次执行
    }

    bool executeMitama()
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
            return false;
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
                    return false;
                }
                goto settle;
            case 2: // 队伍界面（组队模式起点）
                goto settle;
            case 3: // 战斗界面，跳过挑战/准备，直接等待战斗结束
                goto battle;
            case 4: // 奖励领取界面，战斗已结束，进入组队/收尾流程
                goto team;
            default:
                return false;
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

                //先开锁定
                if (!mitama_isCastingLocked)
                {
                    QString savePathCommonUnlock = actions.opencvRecognizesAndClick(screenshotPath + "super-lock.png", 0.75, true, true);
                    if (!savePathCommonUnlock.isEmpty())
                    {
                        mitama_isCastingLocked = true;
                        Logger::log(QString("锁定阵容"));
                    }
                }

                waitWithEventProcessing(2000);

                //先判断是不是队长
                isCaptain = actions.opencvContains(screenshotPath + "challenge-btn.png", 0.65, false);
                Logger::log(QString("当前为【%1】状态").arg(isCaptain ? QStringLiteral("队长") : QStringLiteral("队友")));

                if (isCaptain)
                {
                    // 队长状态：首次执行（roomVacancy < 0）没有上一轮空位数据，直接进入挑战点击流程；
                    // 后续轮次（roomVacancy >= 0）等待队友全部进入后再进入战斗。
                    // 注意：上一轮能开战说明当时队伍是满的，记录的空位数为 0，因此不能用 > 0 作为判断，
                    // 否则满人结算后队友离开、空位重新出现时不会进入等待逻辑
                    const int prevVacancy = roomVacancy;

                    if (prevVacancy >= 0)
                    {
                        // 本轮进入战斗前识别一次队伍空位（队伍最多3人，即2个空位）
                        roomVacancy = static_cast<int>(actions.opencvFindAll(screenshotPath + "team-vacancy.png", 0.65, true).size());
                        Logger::log(QString("上一轮空位: %1，本轮等待队友进入，当前空位: %2").arg(prevVacancy).arg(roomVacancy));

                        // 空位清零（队友全部进入）后才进入战斗；等待次数有显示和上限，
                        // 超时后不再等待，继续执行后续逻辑
                        // 循环条件检查任务状态：点「停止」后立即中断等待，而非傻等满全部轮询
                        for (int i = 0; roomVacancy > 0 && i < MAX_WAIT_NUM && TaskRunner::instance().isRunning(); ++i)
                        {
                            Logger::log(QString("等待队友进入，剩余空位: %1 (%2/%3)").arg(roomVacancy).arg(i + 1).arg(MAX_WAIT_NUM));
                            waitWithEventProcessing(MAX_WAIT_TIME);

                            roomVacancy = static_cast<int>(actions.opencvFindAll(screenshotPath + "team-vacancy.png", 0.65, true).size());
                        }

                        if (roomVacancy > 0)
                        {
                            Logger::log(QString("等待队友超时，继续执行后续逻辑"));
                        }
                    }

                    // 挑战按钮可点击即进入战斗，同时记录本轮空位供下一轮参考
                    bool enteredBattle = false;
                    // 循环条件检查任务状态：点「停止」后立即中断等待，而非傻等满全部轮询
                    for (int i = 0; i < MAX_WAIT_NUM && TaskRunner::instance().isRunning(); ++i)
                    {
                        roomVacancy = static_cast<int>(actions.opencvFindAll(screenshotPath + "team-vacancy.png", 0.65, true).size());
                        qDebug() << "[debug] roomVacancy:" << roomVacancy;

                        if (!actions.opencvRecognizesAndClick(screenshotPath + "challenge-btn-active.png", 0.65, true, true, ClickExclude{0.2, 0.2, 0.2, 0.2}).isEmpty())
                        {
                            //进入战斗
                            Logger::log(QString("点击进入御魂副本战斗"));
                            enteredBattle = true;
                            break;
                        }
                        waitWithEventProcessing(MAX_WAIT_TIME);
                    }
                    if (!enteredBattle)
                    {
                        Logger::log(QString("等待队友超时"));
                        return false;
                    }
                } else
                {
                    //队友模式不需要点击，只需要等待队长发送邀请
                    //如果已经开启了自动接受邀请，那么则全程不需要动
                    //理论上直接等待几秒即可，等待自动进入战斗
                    Logger::log(QString("等待房主进入战斗"));
                    waitWithEventProcessing(3000);
                }
            } else
            {
                //先开锁定
                if (!mitama_isCastingLocked)
                {
                    QString savePathCommonUnlock = actions.opencvRecognizesAndClick(screenshotPath + "common-unlock.png", 0.65, true, true);
                    if (!savePathCommonUnlock.isEmpty())
                    {
                        mitama_isCastingLocked = true;
                        Logger::log(QString("锁定阵容"));
                    }
                }

                //单人模式
                isCaptain = true;
                const QString settled = actions.ocrRecognizesAndClickAny({"挑战"}, 0.8, true, QRectF(80, 65, 100, 100), ocr::Enhance::Upscale);
                if (!settled.isEmpty())
                {
                    Logger::log(QString("点击挑战"));
                    waitWithEventProcessing(4000);
                }
                //单人模式 不会有人用的 所以不写锁定逻辑
            }
        }

    ready:
        waitWithEventProcessing(500);
        // 如果有准备按钮则点击，否则无视，因为正常会手动点锁定
        if (!mitama_isCastingLocked)
        {
            actions.yoloRecognizesAndClick(0.55, false, "battle-ready");
            waitWithEventProcessing(500);
        }

    battle:
        // 判断是否已经进入战斗界面
        {
            bool enteredBattleScene = false;
            // 循环条件检查任务状态：点「停止」后立即中断等待，而非傻等满全部轮询
            for (int i = 0; i < MAX_WAIT_NUM && TaskRunner::instance().isRunning(); ++i)
            {
                //有准备就点一下
                if (!mitama_isCastingLocked)
                {
                    actions.yoloRecognizesAndClick(0.55, false, "battle-ready");
                    waitWithEventProcessing(500);
                }

                if (actions.yoloContainsLabels(0.50, {"common-exit-battle"}, false))
                {
                    Logger::log(QString("已进入御魂战斗场景"));
                    enteredBattleScene = true;
                    break;
                }
                waitWithEventProcessing(1000);
                Logger::log(QString("等待进入战斗场景...(%1/%2)").arg(i+1).arg(MAX_WAIT_NUM));
            }
            if (!enteredBattleScene)
            {
                Logger::log(QString("等待进入战斗场景超时"));
                return false;
            }
        }

        waitWithEventProcessing(1000);

        Logger::log(QString("等待获取战斗结果"));
        //判断战斗结果
        {
            bool battleResult = false;
            for (int i = 0; i < MAX_WAIT_NUM && TaskRunner::instance().isRunning(); ++i)
            {
                if (actions.opencvContains(screenshotPath + "battle-victory.png", 0.65, true))
                {
                    waitWithEventProcessing(1000);
                    actions.opencvRecognizesAndClick(screenshotPath + "battle-victory.png", 0.65, true, true);
                    Logger::log(QString("战斗胜利"));
                    battleResult = true;
                    break;
                }
                waitWithEventProcessing(200);

                if (actions.ocrContainsText("可以按照以下方式提升自己战斗力", 0.70, QRectF(15.2, 44.5, 70.4, 49.4), ocr::Enhance::Upscale))
                {
                    Logger::log(QString("识别到战斗结束"));
                    battleResult = false;
                    waitWithEventProcessing(2000);
                    break;
                }
                waitWithEventProcessing(1000);
            }
            if (!battleResult)
            {
                Logger::log(QString("等待战斗结果超时 或者 战斗失败"));
                return false;
            }
        }

        waitRandomWithEventProcessing(3000, 300);

        //进入领取御魂奖励状态
        {
            bool isError = false;
            // 循环条件检查任务状态：点「停止」后立即中断等待，而非傻等满全部轮询
            for (int i = 0; i < MAX_WAIT_NUM && TaskRunner::instance().isRunning(); ++i)
            {
                Logger::log(QString("等待领取觉醒奖励...(%1/%2)").arg(i+1).arg(MAX_WAIT_NUM));
                //判断是否获取到超鬼王门票
                if (!actions.ocrRecognizesAndClickAny(QStringList{"今日已获得裂变之眼：", "今日已获得裂变之眼", "今日已获得裂变之眼:"}, 0.65, true, QRectF(26.5, 42.4, 47.9, 23.1), ocr::Enhance::Upscale).isEmpty())
                {
                    Logger::log(QString("领取超鬼王门票"));
                    waitRandomWithEventProcessing(2000, 200);
                }

                if (actions.ocrContainsText("点击屏幕继续", 0.70, QRectF(36.3, 89.0, 27.5, 11.0), ocr::Enhance::Upscale))
                {
                    waitRandomWithEventProcessing(1500, 200);

                    //随机点击退出页面
                    QString savePathReceiveRewards;

                    // 随机生成 0 或 1
                    if (QRandomGenerator::global()->bounded(2) == 0)
                    {
                        savePathReceiveRewards = actions.clickInRoi(QRectF(5.9, 19.0, 11.1, 62.3), true);
                    }else
                    {
                        savePathReceiveRewards = actions.clickInRoi(QRectF(86.3, 19.6, 11.1, 62.3), true);
                    }

                    //点击退出伤害页面
                    if (!savePathReceiveRewards.isEmpty())
                    {
                        Logger::log(QString("退出【奖励页面】成功"));
                        isError = true;
                        break;
                    }else
                    {
                        //这里不需要重试
                        Logger::log(QString("退出【奖励页面】失败"));
                    }
                }
                waitWithEventProcessing(MAX_WAIT_TIME);
            }
            if (!isError)
            {
                Logger::log(QString("等待战斗完成超时"));
                return false;
            }
        }

        waitWithEventProcessing(2000);

    team:
        //组队模式下 要进行区分 队长要发送邀请 队员则要等待接受邀请或点击接受邀请
        //单人模式下 会直接退到御魂副本界面 则无需额外判断
        if (teamPlay)
        {
            //组队模式
            bool teamIsError = true;
            // 循环条件检查任务状态：点「停止」后立即中断等待，而非傻等满全部轮询
            for (int i = 0; i < MAX_WAIT_NUM && TaskRunner::instance().isRunning(); ++i)
            {
                if (isCaptain)
                {
                    // 队长状态 要发送邀请队友入队
                    if (autoSendRequestEnable)
                    {
                        //等待队友进入房间
                        Logger::log(QString("等待队友进入房间"));
                        waitWithEventProcessing(3000);
                        teamIsError = false;
                        break;
                    } else
                    {
                        //自动发送邀请 等待队友入队 进入战斗
                        QString savePathAutoSendRequestEnable = actions.ocrRecognizesAndClick("默认邀请队友", 0.55, true, QRectF(), ocr::Enhance::Upscale);
                        if (!savePathAutoSendRequestEnable.isEmpty())
                        {
                            waitWithEventProcessing(1000); // 等待刷新完成
                            if (actions.clickDetectionByLabel("common-btn-yellow_confirm", 0.5, false)) {
                                Logger::log(QString("已开启自动邀请"));
                                teamIsError = false;
                                autoSendRequestEnable = true;
                                break;
                            }else
                            {
                                Logger::log(QString("未检测到自动邀请确认按钮"));
                                return false;
                            }
                        }else
                        {
                            Logger::log(QString("未检测到自动邀请发起"));
                            return false;
                        }
                    }
                } else
                {
                    //队友状态 等待队长发送邀请
                    if (autoSendRequestEnable)
                    {
                        // 1. 处于自动入队 此时会在等待入队界面 等待队长邀请进入队伍 进入战斗
                        // 不用什么操作，会自动进入战斗，检测是否进入战斗即可
                        Logger::log(QString("等待队长发送邀请"));
                        waitWithEventProcessing(3000);
                        teamIsError = false;
                        break;
                    } else
                    {
                        // 2. 第一次会退到主界面等待邀请
                        // 优先扫描 turn-on-auto.png（开启自动入队），命中则点击并开启自动模式；
                        // 未命中再扫描 accept_invitation.png（单次接受邀请）。
                        // 只有 turn-on-auto.png 才会开启 autoSendRequestEnable。
                        QString savePathAuto = actions.opencvRecognizesAndClick(screenshotPath + "turn-on-auto.png", 0.7, false, true);
                        if (!savePathAuto.isEmpty())
                        {
                            autoSendRequestEnable = true;
                            teamIsError = false;
                            Logger::log(QString("自动入队已开启"));
                            break;
                        }

                        QString savePathAccept = actions.opencvRecognizesAndClick(screenshotPath + "accept_invitation.png", 0.8, false);
                        if (!savePathAccept.isEmpty())
                        {
                            teamIsError = false;
                            Logger::log(QString("已接受组队邀请"));
                            break;
                        }
                    }
                }
                waitWithEventProcessing(MAX_WAIT_TIME);
            }
            if (teamIsError)
            {
                Logger::log(QString("组队等待超时"));
                return false;
            }
        }else
        {
            //单机模式下到这里就可以停止逻辑，会自动退到御魂副本界面，从头开始执行逻辑进入战斗即可
        }
        return true;
    }

    /**
     * 判断目前在哪一个场景
     * @return currentInterface
     */
    int getCurrentInterface()
    {
        //1.御魂副本选择界面 2.队伍界面 3.战斗界面 4.奖励领取界面 5.收到悬赏封印 6.其他界面(直接停止任务)
        QJsonArray interfaceData = actions.ocrRecognizes(QRectF(0, 0, 20, 15), ocr::Enhance::Upscale);
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

        const auto detections = actions.yoloRecognizes(0.5);
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

        waitWithEventProcessing(200);

        //有时候可能battle_end会被奖励遮挡 额外添加一个备用的
        if (actions.ocrContainsText("点击屏幕继续", 0.70, QRectF(36.3, 89.0, 27.5, 11.0), ocr::Enhance::Upscale))
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
        QString path = actions.yoloRecognizesAndClick(0.60, false, "common-btn-red_x_solid");
        waitWithEventProcessing(2000);
        return !path.isEmpty();
    }

    // 注册为系统方案：TaskRunner 按注册表调度（注册方式见 ScenarioRegistry.h）
    REGISTER_SCENARIO("SYSTEM_MITAMA", executeMitama, resetMitamaStatus, true)

} // namespace scenarios