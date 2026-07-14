#include "src/engine/scenarios/Arena.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/engine/ScriptActions.h"

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
constexpr int kBattlePollInterval = 30000;
constexpr int kBattlePollCount = 40;

} // namespace

void executeArena()
{
    ScriptActions& actions = ScriptActions::instance();
    Logger::log(QString("开始执行自动挂机斗技"));

    // 示例：读取当前运行方案(currentItem.id 为正在运行的方案 id)的自定义配置，
    // 取不到时用常量兜底。后续增删配置项时按同样方式取值即可。
    const QString configId = currentItem.id;
    const double ocrScore     = getSystemConfigValue(configId, "ocrScore", kOcrScore).toDouble(kOcrScore);
    const int matchPollCount  = getSystemConfigValue(configId, "matchPollCount", kMatchPollCount).toInt(kMatchPollCount);
    const bool randomClick    = getSystemConfigValue(configId, "randomClick", true).toBool(true);
    const QString arenaType   = getSystemConfigValue(configId, "arenaType", QStringLiteral("个人斗技")).toString();
    Logger::log(QString("斗技运行配置: ocrScore=%1, matchPollCount=%2, randomClick=%3, 类型=%4")
                    .arg(ocrScore).arg(matchPollCount).arg(randomClick).arg(arenaType));

    // 0. 判断斗技荣誉值是否已刷满
    // actions.ocrRecognizes(QRectF(80, 0, 100, 10));
    // bool fraction = getNumberOfFraction();

    // 1. 点击「开始匹配」（部分界面按钮文案仅为「匹配」，两者都尝试）
    QString matchClick = actions.ocrRecognizesAndClick("战", ocrScore, randomClick, QRectF(80, 65, 100, 100));
    if (matchClick.isEmpty()) {
        Logger::log(QString("未找到匹配按钮，可能不在斗技界面，结束本次执行"));
        return;
    }

    waitWithEventProcessing(5000); // 等待进入战斗

    // 2. 等待匹配到对手后出现[准备]或者[自动]按钮并点击
    bool readied = false;
    for (int i = 0; i < matchPollCount; ++i) {
        waitWithEventProcessing(kMatchPollInterval);

        // 优先用 YOLO 标签识别准备按钮（一次识别，多标签按优先级择一点击）；回退到 OCR 文字「自动」
        if (!actions.clickFirstDetectionByLabels(QStringList{"battle-ready"}, kYoloScore, 0.0, 0.0).isEmpty()) {
            readied = true;
            break;
        }
        if (!actions.ocrRecognizesAndClick("自动", ocrScore, randomClick, QRectF(0, 13, 13, 28)).isEmpty()) {
            readied = true;
            break;
        }
    }
    if (!readied) {
        Logger::log(QString("等待匹配/准备超时，结束本次执行"));
        return;
    }

    // 3. 进入战斗后确保开启「自动」,如果对面是手动上场，那就不管了，挂满30s会开自动的
    waitWithEventProcessing(8000);
    if (actions.yoloContainsLabels(kYoloScore, {"battle-auto"}, false)) {
        actions.clickDetectionByLabel("battle-auto", kYoloScore, 0.0, 0.0);
    }

    // 4. 轮询等待战斗结束（胜利/失败结算界面），出现后点击结算继续
    bool isEnd = false;
    for (int i = 0; i < kBattlePollCount; ++i) {
        waitWithEventProcessing(kBattlePollInterval);

        // 战斗结束结算：一次 OCR 识别，命中「生利(胜利)」或「失败」哪个就点哪个
        const QString settled = actions.ocrRecognizesAndClickAny(QStringList{"胜利", "失败"}, ocrScore, randomClick,
                                                                 QRectF(16, 0, 69, 37));
        if (!settled.isEmpty()) {
            waitWithEventProcessing(2000);
            Logger::log(QString("斗技本轮完成（结算：%1）").arg(settled == QStringLiteral("胜利") ? "胜利" : "失败"));
            isEnd = true;
            break;
        }
    }
    if (!isEnd) {
        return;
    }

    Logger::log(QString("等待战斗结束超时，结束本次执行"));
}

bool getNumberOfFraction()
{
    ScriptActions& actions = ScriptActions::instance();
    QJsonArray fractionData = actions.ocrRecognizes(QRectF(8, 87, 10, 5));
    if (fractionData.empty())
    {
        return 0;
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
