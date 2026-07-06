#include "src/engine/scenarios/BorderBreakthrough.h"

#include <windows.h>
#include <QDebug>
#include <QString>
#include <vector>

#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/engine/ScriptActions.h"
#include "src/game/GameWindow.h"
#include "src/vision/Geometry.h"

using core::waitWithEventProcessing;

namespace scenarios {

void executeBorderBreakthrough()
{
    ScriptActions& actions = ScriptActions::instance();
    GameWindow& window = GameWindow::instance();

    std::vector<Detection> vec;
    Logger::log(QString("开始执行结界突破"));

    // 识别当前界面状态
    auto detections = actions.yoloRecognizes(0.5, 0.0, 0.3);

    if (detections.empty()) {
        Logger::log(QString("识别失败，无法继续执行"));
        return;
    }

    bool hasPenetrated = ScriptActions::hasDetectionWithLabel(detections, "realm_raid-realm-penetrated");
    bool hasNormal = ScriptActions::hasDetectionWithLabel(detections, "realm_raid-realm-normal");

    if (hasPenetrated == false && hasNormal == false)
    {
        Logger::log(QString("当前不在结界突破场景，无法继续执行"));
        return;
    }

    // 检查是否需要刷新
    if (hasPenetrated) {
        Logger::log(QString("检测到已挑战结界，执行刷新"));

        // 点击刷新按钮
        if (actions.clickDetectionByLabel("common-btn-yellow_confirm", 0.5, 0.0, 0.0)) {
            waitWithEventProcessing(1500); // 等待刷新确认界面出现

            // 点击确认刷新
            if (actions.clickDetectionByLabel("common-btn-yellow_confirm", 0.5, 0.0, 0.0)) {
                Logger::log(QString("刷新成功"));
                waitWithEventProcessing(3000); // 等待刷新完成

                // 刷新后重新执行
                executeBorderBreakthrough(); //此时会进入投4逻辑
                return;
            } else {
                Logger::log(QString("未找到确认刷新按钮"));
            }
        } else {
            Logger::log(QString("未找到刷新按钮"));
        }
        return;
    }

    //开始进行投4
    Logger::log(QString("结界突破-开始进行投4"));
    for (auto& det : detections) {
        if (comparesEqual(det.className, "realm_raid-realm-normal"))
        {
            //正常会有9个
            vec.push_back(det);
        }
    }
    int surrenderIndex[4] = {1,3,5,7};
    for (int i : surrenderIndex)
    {
        cv::Rect matchRect = vec[i].bbox;
        cv::Point clickPt = vision::randomPointInRectExcludeWidth(matchRect, 0.0, 0.3, 10);
        window.clickInWindow(clickPt);
        waitWithEventProcessing(3000);

        Logger::log(QString("开始点击进攻"));
        //点击攻击
        if (actions.clickDetectionByLabel("common-btn-yellow_confirm",0.55,0.0,0.0))
        {
            Logger::log(QString("准备退出战斗"));
            waitWithEventProcessing(5000);

            //按下esc 再按下enter
            window.postKey(VK_ESCAPE);
            waitWithEventProcessing(200);
            window.postKey(VK_RETURN);
            waitWithEventProcessing(5000);

            Logger::log(QString("识别失败并点击"));
            //识别战斗失败 并点击
            QString savePath = actions.ocrRecognizesAndClick("失败", 0.5, true);
            actions.processAndShowImage(savePath);
            waitWithEventProcessing(5000);
        }else
        {
            //找不到 可按下的攻击按钮 话基本就是没有券了，直接结束
            Logger::log(QString("找不到 可按下的攻击按钮，票已清空"));
            return;
        }
    }

    Logger::log(QString("结界突破-开始进行清票操作"));
    for (const Detection det : vec)
    {
        waitWithEventProcessing(3000);

        //点击突破框
        cv::Rect matchRect = det.bbox;
        Logger::log(QString("开始点击突破框"));
        cv::Point clickPt = vision::randomPointInRectExcludeWidth(matchRect, 0.0, 0.3, 10);
        window.clickInWindow(clickPt);
        waitWithEventProcessing(3000);

        //点击攻击
        Logger::log(QString("开始点击进攻"));
        if (actions.clickDetectionByLabel("common-btn-yellow_confirm",0.50,0.0,0.0))
        {
            // 循环等待直到找到战斗结束框，最多等待1分钟
            const int MAX_ATTEMPTS = 6;  // 6次 * 10秒 = 1分钟
            int attempts = 0;

            while (attempts < MAX_ATTEMPTS) {
                waitWithEventProcessing(10000);  // 每次循环前等待10秒
                attempts++;

                qDebug() << "第" << attempts << "次尝试OpenCV识别...";

                QString screenshotPath = AppPaths::instance().screenshotPath();
                QString savePath = actions.opencvRecognizesAndClick(screenshotPath + "battle_end.png", 0.6, true);
                if (!savePath.isEmpty())
                {
                    qDebug() << "成功找到目标文本，退出循环";
                    actions.processAndShowImage(savePath);
                    break;  // 找到目标后跳出外层循环
                }

                if (attempts >= MAX_ATTEMPTS) {
                    qWarning() << "达到最大尝试次数" << MAX_ATTEMPTS << "，未找到目标文本，退出循环";
                    break;
                }
            }
        }else
        {
            //找不到 可按下的攻击按钮 话基本就是没有券了，直接结束
            Logger::log(QString("找不到 可按下的攻击按钮，票已清空"));
            return;
        }
    }

    Logger::log(QString("结界突破执行完成"));
}

} // namespace scenarios
