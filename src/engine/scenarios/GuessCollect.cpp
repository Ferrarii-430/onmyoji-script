//
// Created by 蔡国师 on 26-10-1.
//
#include "GuessCollect.h"

#include <algorithm>

#include <QDate>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QTime>
#include <QUrl>
#include <QUrlQuery>

#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/engine/ScriptActions.h"
#include "src/engine/TaskRunner.h"
#include "ScenarioRegistry.h"

using core::waitWithEventProcessing;
using core::waitRandomWithEventProcessing;

namespace scenarios
{
    namespace
    {
        // 每日触发时刻：活动 10:00 开始、24:00 结束，每 2h 一局共 7 局。
        // 每局整点出题/揭晓答案，统一延后 1 分钟采集，确保界面已刷新：
        //   10:00 出第一局题     → 10:01 采集题目（无上一局，不采集答案）
        //   12:00~22:00 揭晓上一局并出新题 → HH:01 先采集答案，再采集新题
        //   24:00 活动结束       → 次日 00:01 仅采集最后一局答案，不再采集题目
        struct ScheduleEvent
        {
            int hour;             // 触发小时（24 小时制）
            int minute;           // 触发分钟
            bool collectAnswer;   // 是否采集上一局答案
            bool collectQuestion; // 是否采集本局新题
        };

        constexpr ScheduleEvent kDailySchedule[] = {
            { 0,  1, true,  false }, // 00:01 收 22:00-24:00 局答案，当日活动已结束
            {10,  1, false, true  }, // 10:01 第一局出题采集
            {12,  1, true,  true  }, // 12:01 起每局：先收上一局答案，再收新局题目
            {14,  1, true,  true  },
            {16,  1, true,  true  },
            {18,  1, true,  true  },
            {20,  1, true,  true  },
            {22,  1, true,  true  },
        };

        // 长等待（如 00:01 收尾后要等到 10:01）期间按墙钟周期性重算剩余时长，
        // 避免系统休眠/时钟调整后按单调时钟睡过头
        constexpr int kClockRecheckMs = 30 * 1000;

        // 计算下一次触发：取 kDailySchedule 中最早晚于 now 的触发项及其触发时间。
        // 今天已过点的项自动顺延到明天；00:01 收尾事件跨天后自然衔接当日 10:01。
        void findNextEvent(const QDateTime& now, const ScheduleEvent*& event, QDateTime& at)
        {
            event = nullptr;
            for (const ScheduleEvent& ev : kDailySchedule) {
                QDateTime candidate(now.date(), QTime(ev.hour, ev.minute));
                if (candidate <= now) {
                    candidate = candidate.addDays(1);
                }
                if (event == nullptr || candidate < at) {
                    event = &ev;
                    at = candidate;
                }
            }
        }

        // 等待到指定时刻，期间保持事件循环响应；返回 false 表示任务被手动停止
        bool waitUntilTime(const QDateTime& target)
        {
            while (TaskRunner::instance().isRunning()) {
                const qint64 remaining = QDateTime::currentDateTime().msecsTo(target);
                if (remaining <= 0) {
                    return true;
                }
                const int slice = static_cast<int>(std::min<qint64>(remaining, kClockRecheckMs));
                waitWithEventProcessing(slice, []() { return TaskRunner::instance().isRunning(); });
            }
            return false;
        }

        // 通用表单 POST 上报（Spring @RequestParam 风格）：阻塞等待响应（最长 15s）期间
        // 持续处理 Qt 事件保持 UI 响应；手动停止任务时提前中断。成功返回 true，
        // responseOut 带回响应体（成功）或错误信息（失败）
        bool postForm(const QString& apiPath, const QList<QPair<QString, QString>>& fields, QString& responseOut)
        {
            constexpr int kTimeoutMs = 15 * 1000;

            QNetworkAccessManager manager;
            QNetworkRequest request(QUrl(QStringLiteral("https://frp-hat.com:55740") + apiPath));
            request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
            request.setTransferTimeout(kTimeoutMs);

            QUrlQuery form;
            for (const auto& field : fields) {
                form.addQueryItem(field.first, field.second);
            }

            QNetworkReply* reply = manager.post(request, form.toString(QUrl::FullyEncoded).toUtf8());
            // frp 内网穿透链路证书链可能不完整，跳过证书校验保证上报可达
            reply->ignoreSslErrors();

            // 同步等待响应：手动停止任务时提前中断并放弃本次上报
            QElapsedTimer timer;
            timer.start();
            while (!reply->isFinished() && timer.elapsed() < kTimeoutMs && TaskRunner::instance().isRunning()) {
                waitWithEventProcessing(50);
            }
            if (!reply->isFinished()) {
                reply->abort();
                reply->deleteLater();
                responseOut = QStringLiteral("超时或任务已停止");
                return false;
            }

            if (reply->error() != QNetworkReply::NoError) {
                responseOut = reply->errorString();
                reply->deleteLater();
                return false;
            }
            responseOut = QString::fromUtf8(reply->readAll());
            reply->deleteLater();
            return true;
        }

        // 上报对局结果到后端：POST /api/guess/round/result。失败/超时仅记日志，不中断采集流程
        void reportRoundResult(const QString& roundDate, int roundNo, const QString& winSide)
        {
            QString response;
            if (postForm(QStringLiteral("/api/guess/round/result"),
                         {{QStringLiteral("roundDate"), roundDate},
                          {QStringLiteral("roundNo"), QString::number(roundNo)},
                          {QStringLiteral("winSide"), winSide}},
                         response)) {
                Logger::log(QString("对弈竞猜 已录入结果: %1 第%2局 %3方胜，响应: %4")
                                .arg(roundDate, QString::number(roundNo), winSide, response));
            } else {
                Logger::log(QString("对弈竞猜结果录入失败: %1（%2 第%3局 %4方胜）")
                                .arg(response, roundDate, QString::number(roundNo), winSide));
            }
        }

        // 上报某局双方式神数据：POST /api/guess/shikigami，red/blue 为 JSON 数组字符串。
        // 失败/超时仅记日志，不中断采集流程
        void reportShikigamiData(const QString& roundDate, int roundNo, const QJsonArray& redTeam, const QJsonArray& blueTeam)
        {
            QString response;
            if (postForm(QStringLiteral("/api/guess/shikigami"),
                         {{QStringLiteral("roundDate"), roundDate},
                          {QStringLiteral("roundNo"), QString::number(roundNo)},
                          {QStringLiteral("red"), QString::fromUtf8(QJsonDocument(redTeam).toJson(QJsonDocument::Compact))},
                          {QStringLiteral("blue"), QString::fromUtf8(QJsonDocument(blueTeam).toJson(QJsonDocument::Compact))}},
                         response)) {
                Logger::log(QString("对弈竞猜 已录入式神数据: %1 第%2局（红%3名/蓝%4名），响应: %5")
                                .arg(roundDate, QString::number(roundNo),
                                     QString::number(redTeam.size()), QString::number(blueTeam.size()), response));
            } else {
                Logger::log(QString("对弈竞猜式神数据录入失败: %1（%2 第%3局）")
                                .arg(response, roundDate, QString::number(roundNo)));
            }
        }

        // 把单个式神的 OCR 结果数组转成结构化对象。
        // 数组顺序为属性面板从上到下 + 御魂页识别（OCR 失败缺失时按空字符串兜底）:
        //   [0]名称 [1]攻击 [2]生命 [3]防御 [4]速度 [5]暴击 [6]暴击伤害 [7]命中效果 [8]效果抵抗 [9]御魂
        QJsonObject buildShikigami(int no, const QJsonArray& ocrData)
        {
            auto textAt = [&ocrData](int index) -> QString {
                if (index >= ocrData.size()) {
                    return QString();
                }
                return ocrData.at(index).toObject()[QStringLiteral("text")].toString();
            };
            QJsonObject shikigami;
            shikigami[QStringLiteral("no")] = no;
            shikigami[QStringLiteral("name")] = textAt(0);
            shikigami[QStringLiteral("attack")] = textAt(1);
            shikigami[QStringLiteral("hp")] = textAt(2);
            shikigami[QStringLiteral("defense")] = textAt(3);
            shikigami[QStringLiteral("speed")] = textAt(4);
            shikigami[QStringLiteral("critRate")] = textAt(5);
            shikigami[QStringLiteral("critDamage")] = textAt(6);
            shikigami[QStringLiteral("effectHit")] = textAt(7);
            shikigami[QStringLiteral("effectResist")] = textAt(8);
            shikigami[QStringLiteral("mitama")] = textAt(9);
            return shikigami;
        }

        // 采集一方（红/蓝）5 名式神的详细数据：属性面板逐列 OCR + 逐个式神御魂页 OCR。
        // sideEntryBtn 为该方详情入口按钮区域；返回 5 个式神对象（入口未点开时返回空数组）
        QJsonArray collectSideShikigami(const QRectF& sideEntryBtn, const QString& sideName)
        {
            QJsonArray team;
            if (ScriptActions::instance().clickInRoi(sideEntryBtn, true).isEmpty()) {
                Logger::log(QString("%1方详细数据入口未点开，跳过该方采集").arg(sideName));
                return team;
            }
            waitRandomWithEventProcessing(2000, 300);
            Logger::log(QString("点击查看 %1方式神详细数据").arg(sideName));

            // 5 列属性面板 ROI、5 个御魂页签 ROI、5 个御魂信息 ROI（第 3~5 个页签页 y 坐标略有偏移，按实测坐标）
            const QRectF attrRoi[5]      = {{18, 22, 14, 57}, {33, 22, 14, 57}, {48, 22, 14, 57}, {63, 22, 14, 57}, {78, 22, 14, 57}};
            const QRectF mitamaTabRoi[5] = {{22, 79, 5, 8},   {37, 79, 5, 8},   {52, 79, 5, 8},   {67, 79, 5, 8},   {82, 79, 5, 8}};
            const QRectF mitamaRoi[5]    = {{23, 39, 25, 18}, {38, 39, 25, 18}, {25, 41, 25, 18}, {40, 41, 25, 18}, {55, 41, 25, 18}};

            for (int i = 0; i < 5; ++i) {
                QJsonArray item = ScriptActions::instance().ocrRecognizes(attrRoi[i], ocr::Enhance::Upscale);
                ScriptActions::instance().clickInRoi(mitamaTabRoi[i], false);
                waitRandomWithEventProcessing(1500, 300);
                QJsonArray mitama = ScriptActions::instance().ocrRecognizes(mitamaRoi[i], ocr::Enhance::Upscale);
                if (!mitama.empty()) {
                    item.append(mitama.at(0));
                }
                // 返回属性面板，继续下一名式神
                ScriptActions::instance().clickInRoi(QRectF(7, 12, 10, 16), true);
                waitRandomWithEventProcessing(1500, 300);
                team.append(buildShikigami(i + 1, item));
            }

            ScriptActions::instance().yoloRecognizesAndClick(0.60, false, "common-btn-red_x_solid");
            Logger::log(QString("%1方式神详细数据采集完成").arg(sideName));
            return team;
        }

        // 题目采集逻辑（每局开始后 1 分钟触发）。
        // triggerTime 用于推断场次：10:01~22:01 → 第 hour/2-4 局（10:01→第1局 ... 22:01→第7局），
        // 采集双方式神数据后默认下注红方，并把数据上报后端保存
        void collectGuessQuestion(const QDateTime& triggerTime)
        {
            const QString roundDate = triggerTime.date().toString(QStringLiteral("yyyy-MM-dd"));
            const int roundNo = triggerTime.time().hour() / 2 - 4;

            QJsonArray redTeam = collectSideShikigami(QRectF(24, 16, 28, 17), QStringLiteral("红"));

            waitRandomWithEventProcessing(1500, 300);

            QJsonArray blueTeam = collectSideShikigami(QRectF(65, 16, 28, 17), QStringLiteral("蓝"));

            waitRandomWithEventProcessing(1000, 300);

            //默认下注红方
            waitRandomWithEventProcessing(1500,300);
            ScriptActions::instance().clickInRoi(QRectF(22, 43, 9, 13), true);
            waitRandomWithEventProcessing(1500,300);
            ScriptActions::instance().clickInRoi(QRectF(79, 57, 9, 13), true);
            waitRandomWithEventProcessing(1500,300);
            ScriptActions::instance().yoloRecognizesAndClick(0.60, false, "common-btn-yellow_confirm");
            Logger::log(QString("完成下注【红方】"));

            // 发送式神数据录入请求
            reportShikigamiData(roundDate, roundNo, redTeam, blueTeam);
        }

        // 答案采集逻辑（每局结束后 1 分钟触发）。
        // triggerTime 用于推断场次：00:01 收尾属于前一天的第 7 局（22:00-24:00）；
        // 12:01~22:01 → 第 hour/2-5 局（12:01→第1局 ... 22:01→第6局）
        void collectGuessAnswer(const QDateTime& triggerTime)
        {
            QString screenshotPath = AppPaths::instance().screenshotPath();

            QDate roundDate = triggerTime.date();
            int roundNo;
            if (triggerTime.time().hour() == 0) {
                roundNo = 7;
                roundDate = roundDate.addDays(-1);
            } else {
                roundNo = triggerTime.time().hour() / 2 - 5;
            }

            //默认押注（左）红色
            QString Winner = "RED";
            // TODO: 采集上一局揭晓的答案
            if (ScriptActions::instance().ocrContainsText("竞猜失败", 0.6, QRectF(17, 14, 83, 84), ocr::Enhance::Upscale))
            {
                Winner = "BLUE";
            }else if (ScriptActions::instance().ocrContainsText("竞猜成功", 0.6, QRectF(17, 14, 83, 84), ocr::Enhance::Upscale))
            {
                Winner = "RED";
                ScriptActions::instance().clickInRoi(QRectF(56, 55, 6, 7), true);
                waitRandomWithEventProcessing(3000,500);
                QString savePathBattleEnd = ScriptActions::instance().opencvRecognizesAndClick(screenshotPath + "battle_end.png", 0.65, true);
            }else
            {
                Logger::log(QString("对弈竞猜结果数据采集异常 "));
                // return false;
            }

            waitRandomWithEventProcessing(2000,500);

            if (!ScriptActions::instance().ocrRecognizesAndClick("下一局", 0.6, false, QRectF(43, 64, 30, 20), ocr::Enhance::Upscale).isEmpty())
            {
                Logger::log(QString("对弈竞猜数据采集进入下一局"));
            }

            waitRandomWithEventProcessing(2000,500);

            //发送对局结果录入请求
            reportRoundResult(roundDate.toString("yyyy-MM-dd"), roundNo, Winner);
        }

        // 按计划项执行采集：先答案后题目（整点先揭晓上一局答案、再出新题）
        void executeScheduledEvent(const ScheduleEvent& ev, const QDateTime& triggerTime)
        {
            Logger::log(QString("对弈竞猜触发采集: %1").arg(triggerTime.toString("yyyy-MM-dd HH:mm")));
            if (ev.collectAnswer) {
                Logger::log(QString("开始采集 对弈竞猜答案"));
                collectGuessAnswer(triggerTime);
            }
            if (ev.collectQuestion) {
                Logger::log(QString("开始采集 对弈竞猜题目"));
                collectGuessQuestion(triggerTime);
            }
        }
    }

    void resetGuessCollectStatus()
    {
        // 常驻定时方案无跨轮次状态，无需重置
    }

    bool executeGuessCollect()
    {
        Logger::log(QString("开始执行 对弈竞猜数据采集（常驻定时：每天 10:01~22:01 每隔 2h 采集，00:01 收尾，手动停止前不退出）"));

        // 常驻循环：到点触发采集，直到手动点击停止
        while (TaskRunner::instance().isRunning()) {
            const ScheduleEvent* event = nullptr;
            QDateTime at;
            findNextEvent(QDateTime::currentDateTime(), event, at);

            QString actionsDesc;
            if (event->collectAnswer) {
                actionsDesc += QStringLiteral("采集答案");
            }
            if (event->collectAnswer && event->collectQuestion) {
                actionsDesc += QStringLiteral(" + ");
            }
            if (event->collectQuestion) {
                actionsDesc += QStringLiteral("采集题目");
            }
            Logger::log(QString("下一次采集: %1（%2）").arg(at.toString("yyyy-MM-dd HH:mm"), actionsDesc));

            if (!waitUntilTime(at)) {
                break; // 手动停止
            }
            if (!TaskRunner::instance().isRunning()) {
                break;
            }
            executeScheduledEvent(*event, at);
        }

        Logger::log(QString("对弈竞猜数据采集已停止"));
        // 手动停止属于正常退出，返回 true 避免 TaskRunner 误报「系统方案执行失败」
        return true;
    }

    // 注册为系统方案：TaskRunner 按注册表调度（注册方式见 ScenarioRegistry.h）。
    // 当前 config.json 未包含该方案，处于休眠状态；恢复使用只需把方案条目加回 config.json。
    REGISTER_SCENARIO("SYSTEM_GUESS_COLLECT", executeGuessCollect, resetGuessCollectStatus, false)
}
