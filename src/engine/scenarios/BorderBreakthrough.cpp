#include "src/engine/scenarios/BorderBreakthrough.h"

#include <windows.h>
#include <QDebug>
#include <QJsonObject>
#include <QString>
#include <vector>

#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/engine/ScriptActions.h"
#include "src/game/GameWindow.h"
#include "src/vision/Geometry.h"

using core::waitWithEventProcessing;

namespace scenarios {

namespace {

// 结界正常为 3x3 共 9 个框
constexpr int kRealmBoxCount = 9;
// 投4策略固定对下标 1/3/5/7 的结界投降以保留当前等级
constexpr int kSurrenderIndices[] = {1, 3, 5, 7};

// 随机点击结界框内的可点区域（避开左侧守备阵容区域与四周边框）
void clickRealmBox(const Detection& det)
{
    const cv::Rect& matchRect = det.bbox;
    const std::vector<cv::Rect> excludes = {
        vision::widthExcludeRect(matchRect, 0.0, 0.4),   // 左侧 40%
        vision::heightExcludeRect(matchRect, 0.0, 0.1),  // 上边框 10%
        vision::heightExcludeRect(matchRect, 0.9, 1.0),  // 下边框 10%
        vision::widthExcludeRect(matchRect, 0.9, 1.0),   // 右侧 10%
    };
    GameWindow::instance().clickInWindow(
        vision::randomPointInRectExcludeAreas(matchRect, excludes, 5));
}

// 点击结界框并点击“进攻”，返回 false 表示未找到可按下的进攻按钮
bool openRealmAndAttack(const Detection& det)
{
    clickRealmBox(det);
    waitWithEventProcessing(2000);

    Logger::log(QString("开始点击进攻"));
    if (ScriptActions::instance()
            .ocrRecognizesAndClick("进攻", 0.55, true, QRectF(), ocr::Enhance::Upscale)
            .isEmpty()) {
        Logger::log(QString("找不到可按下的攻击按钮"));
        return false;
    }
    return true;
}

// 投4用：等待进入战斗后 ESC+回车 立即投降，并点击“失败”结算
bool surrenderBattle()
{
    constexpr int kEnterBattleAttempts = 5;  // 5次 * 1秒 = 5秒
    for (int attempts = 0; attempts < kEnterBattleAttempts; ++attempts) {
        waitWithEventProcessing(1000);

        if (ScriptActions::instance().yoloContainsLabels(0.45, {"common-exit-battle"}, false)) {
            Logger::log(QString("准备退出战斗"));
            waitWithEventProcessing(500);

            GameWindow::instance().postKey(VK_ESCAPE);
            waitWithEventProcessing(200);
            GameWindow::instance().postKey(VK_RETURN);

            Logger::log(QString("识别失败并点击"));
            bool isEnd = false;
            for (int i = 0; i < 4; ++i)
            {
                waitWithEventProcessing(2000);
                if (!ScriptActions::instance().ocrRecognizesAndClick("失败", 0.5, true, QRectF(), ocr::Enhance::Upscale).isEmpty()) {
                    isEnd = true;
                    break;
                }
            }
            if (!isEnd)
            {
                Logger::log(QString("未识别到【失败】，结束任务"));
                return false;
            }
            waitWithEventProcessing(8000);
            return true;
        }
    }
    qWarning() << "等待" << kEnterBattleAttempts << "秒未进入战斗，结束任务";
    return false;
}

// 清票用：等待战斗结束并点击结算框（含 3/6/9 次连胜的额外奖励确认）
bool waitForBattleEnd()
{
    constexpr int kBattleEndAttempts = 36;  // 36次 * 5秒 = 3分钟
    const QString screenshotPath = AppPaths::instance().screenshotPath();
    for (int attempts = 0; attempts < kBattleEndAttempts; ++attempts) {
        waitWithEventProcessing(5000);

        if (!ScriptActions::instance()
                 .opencvRecognizesAndClick(screenshotPath + "battle_end.png", 0.65, true, true)
                 .isEmpty()) {
            qDebug() << "成功找到识别目标，完成战斗";
            waitWithEventProcessing(6000);

            // 3/6/9 次连胜可能出现额外奖励，需要再次确认结算
            if (!ScriptActions::instance()
                     .opencvRecognizesAndClick(screenshotPath + "battle_end.png", 0.65, true, true)
                     .isEmpty()) {
                waitWithEventProcessing(3000);
            }
            return true;
        }
    }
    qWarning() << "达到最大尝试次数" << kBattleEndAttempts << "，未找到识别目标，结束任务";
    return false;
}

} // namespace

int getNumberOfTickets()
{
    ScriptActions& actions = ScriptActions::instance();
    QJsonArray ticketsData = actions.ocrRecognizes(QRectF(82, 0, 100, 8), ocr::Enhance::Upscale);
    if (ticketsData.isEmpty())
    {
        qWarning() << "门票检测异常：" << ticketsData;
        return 0;
    }
    //正常来说只会有一个文字。at() 是 const 版本，越界返回 Undefined 而非触发
    // Q_ASSERT_X 直接 abort（已通过上面的 isEmpty() 校验，这里双重保险）
    const QJsonObject item = ticketsData.at(0).toObject();
    const QString text = item["text"].toString();
    const QString tickets = text.split("/").value(0);
    return tickets.toInt();
}

bool executeBorderBreakthrough()
{
    ScriptActions& actions = ScriptActions::instance();
    Logger::log(QString("开始执行结界突破"));

    const QString configId = currentItem.id;
    const bool retentionLevel = getSystemConfigValue(configId, "retentionLevel", true).toBool(true);
    Logger::log(QString("结界突破运行配置: 保留当前等级=%1").arg(retentionLevel));

    // 识别当前界面状态
    std::vector<Detection> detections = actions.yoloRecognizes(0.5);
    if (detections.empty()) {
        Logger::log(QString("识别失败，无法继续执行"));
        return false;
    }

    const bool hasPenetrated = ScriptActions::hasDetectionWithLabel(detections, "realm_raid-realm-penetrated");
    const bool hasNormal = ScriptActions::hasDetectionWithLabel(detections, "realm_raid-realm-normal");
    if (!hasPenetrated && !hasNormal) {
        Logger::log(QString("当前不在结界突破场景，无法继续执行"));
        return false;
    }

    // 存在已挑战结界时执行一次刷新，刷新后重新识别界面（游戏机制决定刷新只需一次）
    if (hasPenetrated) {
        Logger::log(QString("检测到已挑战结界，执行刷新"));
        if (!actions.clickDetectionByLabel("common-btn-yellow_confirm", 0.5, false)) {
            Logger::log(QString("未找到刷新按钮"));
            return false;
        }
        waitWithEventProcessing(2000);  // 等待刷新确认界面出现

        if (!actions.clickDetectionByLabel("common-btn-yellow_confirm", 0.5, false)) {
            Logger::log(QString("未找到确认刷新按钮"));
            return false;
        }
        Logger::log(QString("刷新成功"));
        waitWithEventProcessing(2000);  // 等待刷新完成

        // 刷新后重新识别，此时会进入投4逻辑
        detections = actions.yoloRecognizes(0.5);
        if (detections.empty()) {
            Logger::log(QString("刷新后识别失败，无法继续执行"));
            return false;
        }
    }

    // 查看门票数量
    int tickets = getNumberOfTickets();
    Logger::log("门票剩余:" + std::to_string(tickets));
    if (tickets == 0) {
        // 门票耗尽属正常结束，继续循环等待门票恢复
        return true;
    }

    // 收集可挑战的结界框
    std::vector<Detection> vec;
    for (const auto& det : detections) {
        if (comparesEqual(det.className, "realm_raid-realm-normal")) {
            vec.push_back(det);
        }
    }

    // 投4：保留等级，对固定下标的结界投降
    if (retentionLevel) {
        Logger::log(QString("结界突破-等级保留-开始进行投4"));
        if (static_cast<int>(vec.size()) < kRealmBoxCount) {
            Logger::log(QString("可挑战结界数量异常: %1，结束任务").arg(vec.size()));
            return false;
        }
        for (int index : kSurrenderIndices) {
            if (!openRealmAndAttack(vec[index]) || !surrenderBattle()) {
                return false;
            }
        }
    }

    waitWithEventProcessing(5000);

    Logger::log(QString("结界突破-开始进行清票操作"));
    for (const auto & index : vec) {
        // 查看门票数量
        tickets = getNumberOfTickets();
        Logger::log("门票剩余:" + std::to_string(tickets));
        if (tickets == 0) {
            // 门票耗尽属正常结束，继续循环等待门票恢复
            return true;
        }

        waitWithEventProcessing(500);
        if (!openRealmAndAttack(index)) {
            // 可能是上一次战斗的奖励画面未关闭导致找不到进攻按钮，先领取遗留奖励再重试
            Logger::log(QString("进攻失败，尝试领取遗留奖励后重试"));
            if (!waitForBattleEnd()) {
                return false;
            }
            // 领完遗留奖励后重新打当前框，避免丢失这一次战斗次数
            if (!openRealmAndAttack(index) || !waitForBattleEnd()) {
                return false;
            }
        } else if (!waitForBattleEnd()) {
            return false;
        }
    }

    Logger::log(QString("结界突破执行完成"));
    return true;
}

} // namespace scenarios
