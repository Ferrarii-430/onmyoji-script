#include "src/engine/scenarios/Arena.h"

#include <initializer_list>

#include <QString>

#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/engine/ScriptActions.h"

using core::waitWithEventProcessing;

namespace scenarios {

namespace {

// 斗技界面/流程相关文案与标签，集中在此便于按实际游戏调整。
constexpr double kOcrScore = 0.6;   // OCR 命中阈值
constexpr double kYoloScore = 0.55; // YOLO 命中阈值

// 匹配对手最长等待：kMatchPollCount 次，每次 kMatchPollInterval 毫秒
constexpr int kMatchPollInterval = 2000;
constexpr int kMatchPollCount = 15;

// 战斗结束最长等待：kBattlePollCount 次，每次 kBattlePollInterval 毫秒
constexpr int kBattlePollInterval = 5000;
constexpr int kBattlePollCount = 24;

} // namespace

void executeArena()
{
    ScriptActions& actions = ScriptActions::instance();
    Logger::log(QString("开始执行自动挂机斗技"));

    // 依次尝试候选标签，命中哪个就点哪个，返回被点击的标签（都没命中则返回空）。
    auto clickFirstPresentLabel = [&actions](std::initializer_list<const char*> labels) -> QString {
        for (const char* label : labels) {
            if (actions.clickDetectionByLabel(QString::fromLatin1(label), kYoloScore, 0.0, 0.0)) {
                return QString::fromLatin1(label);
            }
        }
        return QString();
    };

    // 1. 点击「开始匹配」（部分界面按钮文案仅为「匹配」，两者都尝试）
    QString matchClick = actions.ocrRecognizesAndClick("战", kOcrScore, true, QRectF(80, 65, 100, 100));
    if (matchClick.isEmpty()) {
        Logger::log(QString("未找到匹配按钮，可能不在斗技界面，结束本次执行"));
        return;
    }

    waitWithEventProcessing(5000); // 等待进入战斗

    // 2. 等待匹配到对手后出现[准备]或者[自动]按钮并点击
    bool readied = false;
    for (int i = 0; i < kMatchPollCount; ++i) {
        waitWithEventProcessing(kMatchPollInterval);

        // 优先用 YOLO 标签识别准备/结算按钮，命中哪个就点哪个；回退到 OCR 文字「自动」
        if (!clickFirstPresentLabel({"battle-ready", "battle-victory"}).isEmpty()) {
            readied = true;
            break;
        }
        if (!actions.ocrRecognizesAndClick("自动", kOcrScore, true).isEmpty()) {
            readied = true;
            break;
        }
    }
    if (!readied) {
        Logger::log(QString("等待匹配/准备超时，结束本次执行"));
        return;
    }

    // 3. 进入战斗后确保开启「自动」
    waitWithEventProcessing(8000);
    if (actions.yoloContainsLabels(kYoloScore, {"battle-auto"}, false)) {
        actions.clickDetectionByLabel("battle-auto", kYoloScore, 0.0, 0.0);
    }

    // 4. 轮询等待战斗结束（胜利/失败结算界面），出现后点击结算继续
    for (int i = 0; i < kBattlePollCount; ++i) {
        waitWithEventProcessing(kBattlePollInterval);

        // 战斗结束结算：命中 battle-victory 或 battle-loss 都点击继续
        const QString settled = clickFirstPresentLabel({"battle-victory", "battle-loss"});
        if (!settled.isEmpty()) {
            waitWithEventProcessing(2000);
            Logger::log(QString("斗技本轮完成（结算：%1）").arg(settled));
            return;
        }
    }

    Logger::log(QString("等待战斗结束超时，结束本次执行"));
}

} // namespace scenarios
