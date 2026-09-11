#include "src/engine/scenarios/Budokai.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <climits>

#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/engine/ScriptActions.h"
#include "src/game/GameWindow.h"
#include "src/game/capture/CaptureService.h"
#include "src/vision/Geometry.h"
#include "src/vision/ImageIo.h"

using core::waitWithEventProcessing;

namespace scenarios {

namespace {

// 武道大会界面/流程相关参数，集中在此便于按实际游戏调整。
constexpr double kOcrScore = 0.6;   // OCR 命中阈值
constexpr double kYoloScore = 0.55; // YOLO 命中阈值

// 是否随机点击
constexpr bool randomClick = true;

//0->未知状态   1->单体状态     2->群体状态
int status = 0;

// 从 OCR box（4 个角点）计算外接矩形，坐标已在 DX11 捕获坐标系
cv::Rect boxToRect(const QJsonArray& box)
{
    if (box.size() < 4) return {};
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (const QJsonValue& ptVal : box) {
        const QJsonArray pt = ptVal.toArray();
        if (pt.size() < 2) continue;
        const int x = pt[0].toInt();
        const int y = pt[1].toInt();
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }
    if (minX > maxX || minY > maxY) return {};
    return {minX, minY, maxX - minX, maxY - minY};
}

// 计算点击点：randomClick 时在矩形内随机，否则取中心
cv::Point makeClickPoint(const cv::Rect& rect)
{
    if (randomClick) {
        return vision::randomPointInRect(rect);
    }
    return {rect.x + rect.width / 2, rect.y + rect.height / 2};
}

} // namespace

bool executeBudokai()
{
    ScriptActions& actions = ScriptActions::instance();
    GameWindow& window = GameWindow::instance();
    const QString screenshotPath = AppPaths::instance().screenshotPath();
    QString monsterName;
    Logger::log(QString("开始执行武道大会"));

    // 读取当前运行方案的自定义配置
    const QString configId = currentItem.id;
    const QString schemeName = getSystemConfigValue(configId, "schemeName", "").toString();
    const QString singleLineupName = getSystemConfigValue(configId, "singleLineupName", "").toString();
    const QString groupLineupName = getSystemConfigValue(configId, "groupLineupName", "").toString();
    Logger::log(QString("武道大会运行配置: 方案=%1 | 单体阵容=%2 | 群体阵容=%3")
                .arg(schemeName, singleLineupName, groupLineupName));

    // ---- 切换阵容的通用流程，lineupName 为目标阵容名称 ----
    auto switchLineup = [&](const QString& lineupName) -> bool {
        // 1. 点击式神录（排除边框区域取点）
        constexpr ClickExclude yoloShikigamiRecordExclude{0.3, 0.2, 0.1, 0.1};
        if (actions.opencvRecognizesAndClick(screenshotPath + "shikigami-record.png", 0.65, randomClick, false, yoloShikigamiRecordExclude).isEmpty()) {
            Logger::log(QString("无法识别点击【式神录】"));
            return false;
        }
        waitWithEventProcessing(2000);

        // 2. 点击预设
        if (actions.ocrRecognizesAndClick("预设", kOcrScore, randomClick, QRectF(19, 6, 21, 15), ocr::Enhance::All).isEmpty()) {
            Logger::log(QString("无法识别点击【预设】"));
            return false;
        }
        waitWithEventProcessing(1500);

        // 3. 切换分组：识别分组列表，点击匹配 schemeName 的项
        QJsonArray groupingData = actions.ocrRecognizes(QRectF(83, 9, 16, 81), ocr::Enhance::Upscale);
        bool foundGrouping = false;
        for (const QJsonValue& val : groupingData) {
            const QJsonObject item = val.toObject();
            if (item["text"].toString() == schemeName) {
                const cv::Rect rect = boxToRect(item["box"].toArray());
                if (rect.width > 0 && rect.height > 0) {
                    GameWindow::instance().clickInWindow(makeClickPoint(rect));
                    foundGrouping = true;
                    Logger::log(QString("已点击分组: %1").arg(schemeName));
                }
                break;
            }
        }
        if (!foundGrouping) {
            Logger::log(QString("未找到分组: %1").arg(schemeName));
            return false;
        }
        waitWithEventProcessing(1500);

        // 4. 识别队伍预设名称，记录目标预设的 box
        QJsonArray presetData = actions.ocrRecognizes(QRectF(40, 18, 27, 62), ocr::Enhance::Upscale);
        QJsonArray targetPresetBox; // {"box":[[47,120],[77,120],[77,136],[47,136]]}
        for (const QJsonValue& val : presetData) {
            const QJsonObject item = val.toObject();
            if (item["text"].toString() == lineupName) {
                targetPresetBox = item["box"].toArray();
                break;
            }
        }
        if (targetPresetBox.isEmpty()) {
            Logger::log(QString("未找到预设阵容: %1").arg(lineupName));
            return false;
        }
        const cv::Rect presetRect = boxToRect(targetPresetBox);
        if (presetRect.width <= 0 || presetRect.height <= 0) {
            Logger::log(QString("预设阵容 box 解析失败: %1").arg(lineupName));
            return false;
        }

        // 5. 识别所有预设队伍切换按钮
        auto detections = actions.opencvFindAll(screenshotPath + "switch-soul.png", 0.6, false);
        if (detections.empty()) {
            Logger::log(QString("未找到预设队伍切换按钮"));
            return false;
        }

        // 6. 在 detections 中找出 Y 轴最接近预设 box 中心的项并点击
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
            GameWindow::instance().clickInWindow(makeClickPoint(best->rect));
            Logger::log(QString("点击预设队伍切换: score=%1 位置(%2,%3) 目标Y=%4 偏差=%5")
                        .arg(best->score, 0, 'f', 2).arg(best->center.x).arg(best->center.y)
                        .arg(targetY).arg(minDY));
        }

        waitWithEventProcessing(2000);

        // 7. 点击确认切换御魂
        if (actions.yoloContainsLabels(0.60, {"common-popup-confirm"}, false))
        {
            if (actions.yoloRecognizesAndClick(0.80, false, "common-btn-yellow_confirm").isEmpty())
            {
                Logger::log(QString("无法识别到【确定】按钮"));
                return false;
            }
        }


        waitWithEventProcessing(2000);

        // 8. 点击返回
        if (actions.yoloRecognizesAndClick(0.60, randomClick, "common-exit-yellow").isEmpty())
        {
            Logger::log(QString("无法识别到【返回】按钮"));
            return false;
        }
        return true;
    };

    // 1. 点击搜寻
    if (actions.ocrRecognizesAndClick("搜寻", kOcrScore, randomClick, QRectF(83, 79.5, 12, 15), ocr::Enhance::Upscale).isEmpty())
    {
        Logger::log(QString("无法点击搜寻"));
        return false;
    }

    waitWithEventProcessing(5000);

    // 2. 识别怪物名称
    QJsonArray monsterData = actions.ocrRecognizes(QRectF(26.8, 21, 5.5, 30), ocr::Enhance::Upscale | ocr::Enhance::Grayscale | ocr::Enhance::AutoInvert);
    if (monsterData.size() == 1) {
        monsterName = monsterData[0].toObject()["text"].toString();
        Logger::log(QString("当前怪物名称: %1").arg(monsterName));
    } else {
        Logger::log(QString("无法识别怪物种类"));
        qDebug() << "识别到的数据：" << monsterData;
        return false;
    }

    // 3. 判断是否要切换阵容
    const bool needSingle = monsterName.contains("炽火") || monsterName.contains("合魂");
    const int targetStatus = needSingle ? 1 : 2;
    const QString& lineupName = needSingle ? singleLineupName : groupLineupName;

    if (status == targetStatus) {
        Logger::log(QString("无需切换，当前已是%1阵容").arg(needSingle ? "单体" : "群体"));
    } else {
        if (switchLineup(lineupName)) {
            status = targetStatus;
        } else {
            Logger::log(QString("切换阵容失败"));
            return false;
        }
    }

    waitWithEventProcessing(2000);

    // 4. 判断是否是锁定状态， 如果是则改为未锁定，没有则默认视为已锁定
    if (!actions.opencvRecognizesAndClick(screenshotPath + "common-lock.png", 0.65, false, true).isEmpty())
    {
        Logger::log(QString("解开阵容锁定"));
    }

    // 5. 进入战斗
    if (actions.ocrRecognizesAndClick("挑战", kOcrScore, randomClick, QRectF(83, 70, 12, 16), ocr::Enhance::Upscale).isEmpty())
    {
        Logger::log(QString("无法进入战斗"));
        return false;
    }

    waitWithEventProcessing(3000);

    // 6. 切换阵容
    constexpr ClickExclude yoloBattlePresetExclude{0.1, 0.1, 0.3, 0.1};
    if (actions.yoloRecognizesAndClick(0.50, randomClick, "battle-preset", yoloBattlePresetExclude).isEmpty())
    {
        Logger::log(QString("无法切换分组"));
        return false;
    }

    //7. 切换分组：识别分组列表，点击匹配 schemeName 的项
    QJsonArray groupingData = actions.ocrRecognizes(QRectF(1, 28, 14, 72), ocr::Enhance::Upscale);
    bool foundGrouping = false;
    for (const QJsonValue& val : groupingData) {
        const QJsonObject item = val.toObject();
        if (item["text"].toString() == schemeName) {
            const cv::Rect rect = boxToRect(item["box"].toArray());
            if (rect.width > 0 && rect.height > 0) {
                GameWindow::instance().clickInWindow(makeClickPoint(rect));
                foundGrouping = true;
                Logger::log(QString("已点击分组: %1").arg(schemeName));
            }
            break;
        }
    }
    if (!foundGrouping) {
        Logger::log(QString("未找到分组: %1").arg(schemeName));
        return false;
    }

    waitWithEventProcessing(2000);

    //8. 点击对应的阵容预设
    const QString presetName = (status == 1) ? singleLineupName
                             : (status == 2) ? groupLineupName
                             : QString();
    if (presetName.isEmpty()) {
        Logger::log(QString("无法识别当前的怪物状态: status=%1").arg(status));
        return false;
    }
    QJsonArray presetData = actions.ocrRecognizes(QRectF(14, 30, 32, 60), ocr::Enhance::Upscale);
    bool foundPreset = false;
    for (const QJsonValue& val : presetData) {
        const QJsonObject item = val.toObject();
        if (item["text"].toString() == presetName) {
            const cv::Rect rect = boxToRect(item["box"].toArray());
            if (rect.width > 0 && rect.height > 0) {
                GameWindow::instance().clickInWindow(makeClickPoint(rect));
                foundPreset = true;
                Logger::log(QString("已点击阵容预设: %1").arg(presetName));
            }
            break;
        }
    }
    if (!foundPreset) {
        Logger::log(QString("未找到阵容预设: %1").arg(presetName));
        return false;
    }

    waitWithEventProcessing(2000);

    //8. 点击出战
    if (actions.yoloRecognizesAndClick(0.8, false, "common-btn-yellow_confirm").isEmpty())
    {
        Logger::log(QString("未识别到【出战】按钮"));
        return false;
    }

    waitWithEventProcessing(1500);

    //9. 点击准备
    constexpr ClickExclude yoloBattleReadyExclude{0.1, 0.1, 0.3, 0.1};
    if (actions.yoloRecognizesAndClick(0.5, randomClick, "battle-ready", yoloBattleReadyExclude).isEmpty())
    {
        Logger::log(QString("未识别到【准备】按钮"));
        return false;
    }

    waitWithEventProcessing(2000);

    //10. 判断左下角是否开启自动，有就点 没有直接跳过
    if (!actions.ocrRecognizesAndClick("手动", kOcrScore, false, QRectF(0, 85, 9, 15), ocr::Enhance::All).isEmpty())
    {
        Logger::log(QString("开启自动战斗"));
    }

    waitWithEventProcessing(2000);

    //11. 识别是否完成战斗，最长等待8min，每10s刷新一次缩略图
    constexpr int MAX_WAIT_NUM = 96;
    constexpr int MAX_WAIT_TIME = 5000;
    bool awaitingBattle = false;
    for (int i = 0; i < MAX_WAIT_NUM; ++i)
    {
        waitWithEventProcessing(MAX_WAIT_TIME);
        if (actions.yoloContainsLabels(0.45, {"battle-victory"}, false))
        {
            //再多等一等，防止无效点击
            waitWithEventProcessing(2000);
            constexpr ClickExclude yoloBattleVictoryExclude{0.1, 0.1, 0.1, 0.2};
            if (!actions.yoloRecognizesAndClick(0.45, randomClick, "battle-victory", yoloBattleVictoryExclude).isEmpty())
            {
                awaitingBattle = true;
                break;
            }
        }
    }
    if (!awaitingBattle) {
        Logger::log(QString("等待战斗超时"));
        return false;
    }

    Logger::log(QString("武道大会执行完毕"));
    waitWithEventProcessing(4000);
    return true;
}

void resetBudokaiStatus()
{
    status = 0;
    Logger::log(QString("武道大会状态已重置"));
}

} // namespace scenarios
