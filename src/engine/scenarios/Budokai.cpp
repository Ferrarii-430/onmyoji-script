#include "src/engine/scenarios/Budokai.h"

#include <QJsonObject>
#include <QString>

#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/engine/ScriptActions.h"
#include "src/game/GameWindow.h"
#include "src/vision/Geometry.h"

using core::waitWithEventProcessing;

namespace scenarios {

namespace {

// 武道大会界面/流程相关参数，集中在此便于按实际游戏调整。
constexpr double kOcrScore = 0.6;   // OCR 命中阈值
constexpr double kYoloScore = 0.55; // YOLO 命中阈值

// 是否随机点击
constexpr bool randomClick = true;

} // namespace

void executeBudokai()
{
    ScriptActions& actions = ScriptActions::instance();
    GameWindow& window = GameWindow::instance();

    Logger::log(QString("开始执行武道大会"));

    // 读取当前运行方案的自定义配置
    const QString configId = currentItem.id;
    // 示例：读取 systemConfig 中的配置项
    // const bool autoStop = getSystemConfigValue(configId, "automaticStop", true).toBool(true);
    // Logger::log(QString("武道大会运行配置: 自动停止=%1").arg(autoStop));

    // TODO: 在此实现武道大会的具体流程
    // 1. 识别当前界面状态
    // 2. 匹配对手 / 进入战斗
    // 3. 自动战斗
    // 4. 结算并返回

    Logger::log(QString("武道大会执行完毕"));
}

} // namespace scenarios
