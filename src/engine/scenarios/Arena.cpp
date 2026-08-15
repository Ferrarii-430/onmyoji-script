#include "src/engine/scenarios/Arena.h"

#include <limits>

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/engine/ScriptActions.h"
#include "src/game/GameWindow.h"
#include "src/vision/Geometry.h"

using core::waitWithEventProcessing;

namespace scenarios {

namespace {

// 斗技界面/流程相关文案与标签，集中在此便于按实际游戏调整。
constexpr double kOcrScore = 0.6;   // OCR 命中阈值
constexpr double kYoloScore = 0.55; // YOLO 命中阈值

// 匹配对手最长等待：kMatchPollCount 次，每次 kMatchPollInterval 毫秒
constexpr int kMatchPollInterval = 3000;
constexpr int kMatchPollCount = 15;

// 战斗结束最长等待：kBattlePollCount 次，每次 kBattlePollInterval 毫秒
constexpr int kBattlePollInterval = 5000;
constexpr int kBattlePollCount = 240;

//是否随机点击
constexpr bool randomClick = true;

} // namespace

bool executeArena()
{
    ScriptActions& actions = ScriptActions::instance();
    Logger::log(QString("开始执行自动挂机斗技"));

    // 示例代码：读取当前运行方案(currentItem.id 为正在运行的方案 id)的自定义配置，
    // 取不到时用常量兜底。后续增删配置项时按同样方式取值即可。
    // const QString configId = currentItem.id;
    // const double ocrScore     = getSystemConfigValue(configId, "ocrScore", kOcrScore).toDouble(kOcrScore);
    // const int matchPollCount  = getSystemConfigValue(configId, "matchPollCount", kMatchPollCount).toInt(kMatchPollCount);
    // const bool randomClick    = getSystemConfigValue(configId, "randomClick", true).toBool(true);
    // const QString arenaType   = getSystemConfigValue(configId, "arenaType", QStringLiteral("个人斗技")).toString();
    // Logger::log(QString("斗技运行配置: ocrScore=%1, matchPollCount=%2, randomClick=%3, 类型=%4").arg(ocrScore).arg(matchPollCount).arg(randomClick).arg(arenaType));

    const QString configId = currentItem.id;
    const bool automaticStop    = getSystemConfigValue(configId, "automaticStop", true).toBool(true);
    Logger::log(QString("自动斗技运行配置: 刷满信誉值自动停止=%1").arg(automaticStop));

    // 0. 判断斗技荣誉值是否已刷满
    // actions.ocrRecognizes(QRectF(80, 0, 100, 10));
    bool fraction = getNumberOfFraction();
    if (fraction && automaticStop)
    {
        Logger::log(QString("已刷满荣誉值，退出自动斗技任务"));
        return false;
    }

    // 1. 点击「开始匹配」（部分界面按钮文案仅为「匹配」，两者都尝试）
    QString matchClick = actions.ocrRecognizesAndClick("战", kOcrScore, randomClick, QRectF(87.5, 72, 100, 100), ocr::Enhance::All);
    if (matchClick.isEmpty()) {
        Logger::log(QString("未找到匹配按钮，可能不在斗技界面，结束本次执行"));
        return false;
    }

    waitWithEventProcessing(5000); // 等待进入战斗

    // 2. 等待匹配到对手后出现[准备]或者[自动]按钮并点击
    bool readied = false;
    for (int i = 0; i < kMatchPollCount; ++i) {
        waitWithEventProcessing(kMatchPollInterval);

        // 优先用 YOLO 标签识别准备按钮（一次识别，多标签按优先级择一点击）；回退到 OCR 文字「自动」
        if (!actions.clickFirstDetectionByLabels(QStringList{"battle-ready"}, kYoloScore, 0.0, 0.0).isEmpty()) {
            readied = true;
            break;
        }
        if (!actions.ocrRecognizesAndClick("上阵", kOcrScore, randomClick, QRectF(0, 13, 13, 28)).isEmpty()) {
            readied = true;
            break;
        }
    }
    if (!readied) {
        Logger::log(QString("等待匹配/准备超时，结束本次执行"));
        return false;
    }

    // 3. 进入战斗后确保开启「自动」,如果对面是手动上场，那就不管了，挂满30s会开自动的
    Logger::log(QString("等待进入斗技战斗"));
    waitWithEventProcessing(8000);
    bool actionPosition = false;
    for (int i = 40 - 1; i >= 0; --i)
    {
        waitWithEventProcessing(5000);
        QJsonArray isAutoData = actions.ocrRecognizes(QRectF(0, 13, 13, 28));
        //正常来说只会有一个文字
        QJsonObject item = isAutoData[0].toObject();//TODO 此处会异常闪退
        const QString text = item["text"].toString();
        if (text == "手动")
        {
            // box 格式: [[x1,y1],[x2,y2],[x3,y3],[x4,y4]]，ocrRecognizes 已映射回窗口坐标
            QJsonArray box = item["box"].toArray();
            if (box.size() == 4)
            {
                int minX = std::numeric_limits<int>::max();
                int minY = std::numeric_limits<int>::max();
                int maxX = std::numeric_limits<int>::min();
                int maxY = std::numeric_limits<int>::min();
                for (const QJsonValue& ptVal : box)
                {
                    QJsonArray pt = ptVal.toArray();
                    if (pt.size() < 2) continue;
                    const int px = pt[0].toInt();
                    const int py = pt[1].toInt();
                    minX = std::min(minX, px);
                    minY = std::min(minY, py);
                    maxX = std::max(maxX, px);
                    maxY = std::max(maxY, py);
                }
                const cv::Rect matchRect(minX, minY, maxX - minX, maxY - minY);
                const cv::Point clickPt = randomClick
                                              ? vision::randomPointInRect(matchRect)
                                              : cv::Point(matchRect.x + matchRect.width / 2,
                                                          matchRect.y + matchRect.height / 2);
                Logger::log(QString("已经进入战斗，点击切换自动: (%1, %2)").arg(clickPt.x).arg(clickPt.y));
                GameWindow::instance().clickInWindow(clickPt);
                actionPosition = true;
                break;
            }
        }
    }
    if (!actionPosition)
    {
        Logger::log(QString("超时未进入战斗状态，自动斗技任务结束"));
    }

    // 4. 轮询等待战斗结束（胜利/失败结算界面），出现后点击结算继续
    bool isEnd = false;
    for (int i = 0; i < kBattlePollCount; ++i) {
        waitWithEventProcessing(kBattlePollInterval);

        // 战斗结束结算：一次 OCR 识别，命中「生利(胜利)」或「失败」哪个就点哪个
        const QString settled = actions.ocrRecognizesAndClickAny(QStringList{"胜利", "失败"}, kOcrScore, randomClick,
                                                                 QRectF(16, 0, 69, 37));
        if (!settled.isEmpty()) {
            waitWithEventProcessing(2000);
            Logger::log(QString("斗技本轮完成（结算：%1）").arg(settled == QStringLiteral("胜利") ? "胜利" : "失败"));
            isEnd = true;
            waitWithEventProcessing(5000);
            break;
        }
    }
    if (!isEnd) {
        Logger::log(QString("等待战斗结束超时，结束本次执行"));
        return false;
    }

    return true;
}

bool getNumberOfFraction()
{
    ScriptActions& actions = ScriptActions::instance();
    // 荣誉值区域极小（约窗口高度 3%）且为暗底亮字，开启全部增强提高识别率
    QJsonArray fractionData = actions.ocrRecognizes(QRectF(11, 87.8, 12, 3.2), ocr::Enhance::Upscale);
    if (fractionData.empty())
    {
        return false;
    }
    //正常来说只会有一个文字
    QJsonObject item = fractionData[0].toObject();
    const QString text = item["text"].toString();
    Logger::log(QString("当前斗技荣誉值：") + text);
    const QString fraction = text.split("/")[0];
    const QString total = text.split("/")[1];
    return comparesEqual(fraction, total);
    // return fraction.toInt();
}

} // namespace scenarios
