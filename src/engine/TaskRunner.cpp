#include "src/engine/TaskRunner.h"

#include <functional>

#include <QJsonObject>
#include <QMap>
#include <QRandomGenerator>
#include <QThread>

#include "src/core/AppPaths.h"
#include "src/core/ConfigTypeEnum.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/SettingManager.h"
#include "src/engine/ScriptActions.h"
#include "src/engine/scenarios/ScenarioRegistry.h"
#include "src/game/GameWindow.h"
#include "src/game/capture/CaptureService.h"

thread_local TaskRunner* TaskRunner::t_current = nullptr;
std::atomic<int> TaskRunner::s_activeTasks{0};

TaskRunner::TaskRunner(QObject* parent)
    : QObject(parent)
{
    // 内存回显模式要把 cv::Mat 通过队列信号送到 UI 线程，先注册元类型
    qRegisterMetaType<cv::Mat>("cv::Mat");
}

TaskRunner::~TaskRunner()
{
    // 停止请求 + 等待线程真正退出：会话关闭时本对象先于线程销毁，
    // 直接 delete 会让仍在执行的 runLoop 访问已释放的 this。
    m_stopRequested.store(true);
    m_running.store(false);

    if (m_thread) {
        if (!m_thread->wait(5000)) {
            // 卡在某个系统调用里（极少见）：再给一次机会，仍不退出就强制中止等待。
            // 宁可泄漏一个线程，也不能在它的栈帧还引用 this 时销毁对象。
            qWarning() << "任务线程未在超时内退出，放弃等待";
            m_thread->terminate();
            m_thread->wait(1000);
        }
        delete m_thread;
        m_thread = nullptr;
    }
}

TaskRunner& TaskRunner::instance()
{
    if (t_current) {
        return *t_current;
    }
    // 非任务线程（UI、设置页）调用时的兜底：永不运行的实例，
    // isRunning() 恒为 false，stop() 无副作用。
    static TaskRunner idle;
    return idle;
}

bool TaskRunner::isAnyRunning()
{
    return s_activeTasks.load(std::memory_order_relaxed) > 0;
}

void TaskRunner::start(const QJsonArray& steps, int cycleCount, const GameContext& ctx,
                       const ItemInfo& item, const QJsonArray& configs)
{
    // 判据用线程是否真的还活着，而不是 m_running：
    // stop() 会把 m_running 立刻置 false 让场景轮询退出，但线程还要跑完收尾，
    // 此时若允许再次 start 就会有两个线程同时操作同一个游戏实例。
    if (m_thread && m_thread->isRunning()) {
        Logger::log(QStringLiteral("任务已在运行中"));
        return;
    }
    if (m_thread) {
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    if (steps.isEmpty()) {
        Logger::log(QStringLiteral("当前方案的内容为空，任务停止"));
        return;
    }

    if (!ctx.isValid()) {
        Logger::log(QStringLiteral("未绑定游戏窗口，请先点击「绑定窗口」选择目标实例"));
        return;
    }

    m_ctx = ctx;
    m_steps = steps;
    m_cycleCount = cycleCount;
    m_item = item;
    m_configs = configs;
    m_stopRequested.store(false);
    m_running.store(true);

    // 线程在 start() 返回前创建，成员赋值对线程可见（线程创建的同步语义）
    m_thread = QThread::create([this]() { runLoop(); });
    m_thread->setObjectName(QStringLiteral("task-%1").arg(ctx.pid));
    m_thread->start();
}

void TaskRunner::stop()
{
    // 两个标志都要置：m_running 让 UI 立刻反馈「已停止」，
    // m_stopRequested 让执行线程里的循环与等待步骤尽快退出。
    m_stopRequested.store(true);
    m_running.store(false);
    Logger::log(QStringLiteral("正在停止任务..."));
}

void TaskRunner::runLoop()
{
    // 登记当前线程的执行器：场景代码里的 TaskRunner::instance().isRunning()
    // 会解析到本实例，从而在停止时正确退出各自的轮询循环
    t_current = this;
    s_activeTasks.fetch_add(1, std::memory_order_relaxed);

    // 线程局部上下文绑定。此后本线程内：
    //   GameWindow::instance()     → 本会话绑定的窗口
    //   ScriptActions::instance()  → 本线程独立的动作层（守卫开关、结果图缓存）
    //   截图共享内存通道            → 本线程独立的映射与事件句柄
    Logger::setThreadTag(m_ctx.logTag);

    // 本线程的方案上下文：场景代码读 currentItem / m_configArray 时
    // 拿到的是本次启动时固定的副本，不受其他标签页切换方案影响
    currentItem = m_item;
    m_configArray = m_configs;

    GameWindow& window = GameWindow::instance();
    window.bindTo(m_ctx.pid, m_ctx.hwnd);

    ScriptActions& actions = ScriptActions::instance();
    actions.setShowImageCallback([this](const QString& path, const cv::Mat& img) {
        if (img.empty()) {
            emit showImage(path);
        } else {
            emit showImageData(path, img);
        }
    });

    // 统一收尾：任何退出路径都必须经过这里，否则运行计数与 TLS 指针会残留
    auto finish = [this, &actions, &window]() {
        actions.setShowImageCallback(nullptr);
        actions.setCollaborationGuardEnabled(false);
        // 线程局部实例会常驻到线程退出，主动释放最近一张结果图
        actions.releaseMemoryResult();

        // 先落运行态再发信号：界面在 finished 槽里会重新查询 isRunning()，
        // 若顺序相反会读到「已结束但仍在运行」的瞬时状态
        m_running.store(false);
        emit finished();

        Logger::log(QStringLiteral("任务循环结束"));

        window.bindTo(0, nullptr);
        Logger::setThreadTag(QString());
        s_activeTasks.fetch_sub(1, std::memory_order_relaxed);
        t_current = nullptr;
    };

    if (!window.locate()) {
        Logger::log(QStringLiteral("窗口未找到"));
        finish();
        return;
    }

    if (m_steps.isEmpty()) {
        Logger::log(QStringLiteral("当前方案的内容为空，任务停止"));
        finish();
        return;
    }

    // 日志路径按实例区分：切换绑定后 PID 变化，需要重新下发一次
    const DWORD logPid = window.currentPid();
    if (!m_isInitLogPath || m_logPid != static_cast<quint32>(logPid)) {
        if (capture::setDllLogPath()) {
            Logger::log(QStringLiteral("已修改dll日志路径: ")
                        + AppPaths::instance().dx11LogPathFor(logPid));
        } else {
            Logger::log(QStringLiteral("dll日志路径修改失败: ")
                        + AppPaths::instance().dx11LogPathFor(logPid));
        }
        m_isInitLogPath = true;
        m_logPid = static_cast<quint32>(logPid);
    }

    // 截图连续失败熔断：计数与回调都是本线程私有的，
    // 一个实例失联不会把其他实例正在跑的任务一起停掉
    capture::resetCaptureFailCount();
    capture::setCaptureFailNotifier([this](int failures) {
        Logger::log(QStringLiteral("连续 %1 次截图失败，游戏窗口可能已失联，自动停止任务").arg(failures));
        stop();
    });

    emit started();

    int number = m_cycleCount;
    int total = number;
    const bool infiniteLoop = (number <= 0);
    Logger::log(QStringLiteral("任务循环次数: %1").arg(infiniteLoop ? QStringLiteral("无限") : QString::number(number)));
    emit cycleProgress(0, total);

    do {
        // 用于跟踪每个步骤的错误重试次数
        QMap<int, int> errorRetryMap;
        bool stopDoLoop = false; // 控制是否停止外部循环

        for (int i = 0; i < m_steps.size() && m_running.load() && !stopDoLoop; ++i) {
            const QJsonObject step = m_steps[i].toObject();
            const QString typeStr = step["type"].toString();
            const ConfigTypeEnum type = stringToConfigType(typeStr);

            const QString savePath = executeStep(step);

            // 系统方案步骤执行失败：内部流程已中断，直接终止整个任务循环，
            // 防止剩余步骤继续执行及下一轮循环连环报错。
            if (savePath.isEmpty() && scenarios::findScenario(typeStr) != nullptr) {
                Logger::log(QStringLiteral("系统方案执行失败，终止任务循环"));
                stopDoLoop = true;
                break;
            }

            //识别错误处理
            if (savePath.isEmpty() && type != ConfigTypeEnum::WAIT) {
                const QString identifyErrorHandle = step["identifyErrorHandle"].toString();
                if (identifyErrorHandle == "next") {
                    Logger::log(QStringLiteral("识别失败，继续执行下一个步骤"));
                } else if (identifyErrorHandle == "jump") {
                    //跳转到指定stepsId对应的步骤
                    if (step.contains("jumpStepsId") && !step["jumpStepsId"].toString().isEmpty()) {
                        const QString jumpStepsId = step["jumpStepsId"].toString();
                        int targetIndex = -1;

                        for (int j = 0; j < m_steps.size(); ++j) {
                            if (m_steps[j].toObject()["stepsId"].toString() == jumpStepsId) {
                                targetIndex = j;
                                break;
                            }
                        }

                        if (targetIndex != -1) {
                            i = targetIndex - 1; // 循环会 i++
                            Logger::log(QStringLiteral("识别失败，跳转到步骤ID '%1' (索引 %2)")
                                            .arg(jumpStepsId).arg(targetIndex));
                        } else {
                            Logger::log(QStringLiteral("未找到步骤ID '%1'，使用默认next处理").arg(jumpStepsId));
                        }
                    } else {
                        Logger::log(QStringLiteral("跳转步骤ID未设置，使用默认next处理"));
                    }
                } else if (identifyErrorHandle == "continue") {
                    Logger::log(QStringLiteral("识别失败，跳过当前任务迭代的剩余步骤"));
                    break;
                } else if (identifyErrorHandle == "break") {
                    Logger::log(QStringLiteral("识别失败，停止整个任务循环"));
                    stopDoLoop = true;
                    break;
                } else if (identifyErrorHandle == "retry") {
                    int& retryCount = errorRetryMap[i];
                    if (retryCount < 999) {
                        retryCount++;
                        i = i - 1; // 重试当前步骤
                        Logger::log(QStringLiteral("识别失败，第(%1/999)次重试当前步骤").arg(retryCount));
                        continue;
                    }
                    Logger::log(QStringLiteral("识别失败，重试次数用尽，继续下一个步骤"));
                } else if (identifyErrorHandle == "end") {
                    Logger::log(QStringLiteral("结束任务"));
                    stopDoLoop = true;
                    break;
                } else {
                    Logger::log(QStringLiteral("未知的错误处理选项: %1，使用默认next处理").arg(identifyErrorHandle));
                }
            }
        }

        if (stopDoLoop) {
            break;
        }

        if (!infiniteLoop) {
            number--;
            Logger::log(QStringLiteral("当前任务执行次数:(%1/%2)")
                            .arg(QString::number(total - number), QString::number(total)));
            emit cycleProgress(total - number, total);
        }

        //每次任务结束都固定休眠1秒，防止无限循环一直执行
        core::waitWithEventProcessing(1000, [this]() { return m_running.load(); });

    } while (m_running.load() && (infiniteLoop || number > 0));

    // 重置本轮任务用到过的系统方案的跨轮次状态（如阵容锁定标记）。
    // 这些状态本身也是线程局部的，不会影响其他会话
    for (const QJsonValue& stepVal : m_steps) {
        const scenarios::ScenarioEntry* scenario =
            scenarios::findScenario(stepVal.toObject().value("type").toString());
        if (scenario && scenario->reset) {
            scenario->reset();
        }
    }

    // 任务结束后关闭协作守卫与清理缓存，统一在 finish() 里完成

    finish();
}

QString TaskRunner::executeStep(const QJsonObject& step)
{
    ScriptActions& actions = ScriptActions::instance();
    const QString typeStr = step["type"].toString();
    QString savePath;

    // 全局「自动协作关闭」（设置页持久化开关）作为守卫基线：每个步骤开始时恢复到基线，
    // 读取的是内存缓存，任务运行中在设置页保存也能即时生效
    const bool globalCollabGuard = SETTING_CONFIG.getGlobalAutoCancelCollab();
    actions.setCollaborationGuardEnabled(globalCollabGuard);

    // 系统方案：从场景注册表按 type 字符串调度（新增/删除系统方案见 ScenarioRegistry.h），
    // 无需修改本文件。返回值语义与普通步骤一致：非空 savePath 表示本轮成功。
    if (const scenarios::ScenarioEntry* scenario = scenarios::findScenario(typeStr)) {
        // 按方案配置开启协作弹窗守卫（识别点击失败自动扫协作弹窗并重试一次），
        // 全局开关开启时方案即使未配置守卫也保持开启；执行完恢复到全局基线
        actions.setCollaborationGuardEnabled(scenario->enableCollaborationGuard || globalCollabGuard);
        savePath = scenario->execute() ? QStringLiteral("system-ok") : QString();
        actions.setCollaborationGuardEnabled(globalCollabGuard);
        return savePath;
    }

    const ConfigTypeEnum type = stringToConfigType(typeStr);

    // 步骤级「自动协作关闭」：勾选后仅在本步骤执行期间叠加开启协作弹窗守卫，
    // 识别点击失败时自动扫协作邀请弹窗并关闭后重试一次；范围点击则为点击前先扫一次
    const bool stepCollabGuard = step["autoCancelCollab"].toBool(false);
    if (stepCollabGuard) {
        actions.setCollaborationGuardEnabled(true);
    }

    // 识别类步骤：直接执行一次，不做重试。
    auto recognizeWithRetry = [this](const std::function<QString()>& recognize) -> QString {
        const QString path = recognize();
        if (!path.isNull()) {
            emit showImage(path);
            return path;
        }
        return QString();
    };

    switch (type) {
        case ConfigTypeEnum::OPENCV: {
                Logger::log(QStringLiteral("开始进行OpenCV识图"));
                const QString imagePath = step["imagePath"].toString();
                const double score = step["score"].toDouble();
                const bool randomClick = step["randomClick"].toBool();
                const bool colorCheck = step["colorCheck"].toBool(false);
                // 边框排除比例，旧配置无该字段时默认全 0（不排除）
                const ClickExclude exclude{
                    step["excludeLeft"].toDouble(0.0),
                    step["excludeRight"].toDouble(0.0),
                    step["excludeTop"].toDouble(0.0),
                    step["excludeBottom"].toDouble(0.0)
                };
                savePath = recognizeWithRetry([&]() {
                    return actions.opencvRecognizesAndClick(imagePath, score, randomClick, colorCheck, exclude);
                });
                break;
        }

        case ConfigTypeEnum::OCR: {
                Logger::log(QStringLiteral("开始进行OCR识图"));
                const QString ocrText = step["ocrText"].toString();
                const double score = step["score"].toDouble();
                const bool randomClick = step["randomClick"].toBool();
                const QRectF roiPercent(step["ocrRoiX"].toDouble(), step["ocrRoiY"].toDouble(),
                                        step["ocrRoiW"].toDouble(), step["ocrRoiH"].toDouble());
                // 识别增强开关位掩码；旧配置无该字段时按「仅放大」处理，保持原有行为
                const auto enhance = static_cast<ocr::Enhance>(
                    step["ocrEnhance"].toInt(static_cast<int>(ocr::Enhance::Upscale)));
                savePath = recognizeWithRetry([&]() {
                    return actions.ocrRecognizesAndClick(ocrText, score, randomClick, roiPercent, enhance);
                });
                break;
        }

        case ConfigTypeEnum::YOLO: {
                Logger::log(QStringLiteral("开始进行YOLO识图"));
                const double score = step["score"].toDouble();
                const bool randomClick = step["randomClick"].toBool();
                // 目标标签来自步骤配置；旧配置没有 yoloLabel 时沿用原先的默认标签
                QString yoloLabel = step["yoloLabel"].toString();
                if (yoloLabel.isEmpty()) {
                    yoloLabel = "common-btn-yellow_confirm";
                }
                Logger::log(QStringLiteral("YOLO目标标签: %1").arg(yoloLabel));
                // 边框排除比例，旧配置无该字段时默认全 0（不排除）
                const ClickExclude exclude{
                    step["excludeLeft"].toDouble(0.0),
                    step["excludeRight"].toDouble(0.0),
                    step["excludeTop"].toDouble(0.0),
                    step["excludeBottom"].toDouble(0.0)
                };
                savePath = recognizeWithRetry([&]() {
                    return actions.yoloRecognizesAndClick(score, randomClick, yoloLabel, exclude);
                });
                break;
        }

        case ConfigTypeEnum::CLICK_ROI: {
                Logger::log(QStringLiteral("开始进行范围点击"));
                const bool randomClick = step["randomClick"].toBool(true);
                // 点击区域（百分比），左/上/宽/高；字段缺失时默认整张图片
                const QRectF roiPercent(step["clickRoiX"].toDouble(0.0), step["clickRoiY"].toDouble(0.0),
                                        step["clickRoiW"].toDouble(100.0), step["clickRoiH"].toDouble(100.0));
                savePath = recognizeWithRetry([&]() {
                    return actions.clickInRoi(roiPercent, randomClick);
                });
                break;
        }

        case ConfigTypeEnum::SWIPE: {
                Logger::log(QStringLiteral("开始进行滑动"));
                // 起点（按下）/终点（抬起），单位为图片尺寸百分比；旧配置缺字段时用默认值
                const QPointF fromPercent(step["swipeFromX"].toDouble(0.0), step["swipeFromY"].toDouble(0.0));
                const QPointF toPercent(step["swipeToX"].toDouble(100.0), step["swipeToY"].toDouble(100.0));
                // 轨迹类型跟随设置页的全局鼠标控制模式（直线/贝塞尔/S形/随机漫步），读内存缓存即时生效
                const QString trajectory = SETTING_CONFIG.getMouseControlMode();
                // 按下到抬起的总时长（毫秒），过短会被游戏判定为点击
                const int durationMs = step["swipeDurationMs"].toInt(500);
                savePath = recognizeWithRetry([&]() {
                    return actions.swipeBetweenPoints(fromPercent, toPercent, trajectory, durationMs);
                });
                break;
        }

        case ConfigTypeEnum::WAIT: {
                const int waitTime = step["time"].toInt();
                const bool randomWait = step["randomWait"].toBool();
                const int offsetTime = step["offsetTime"].toInt();

                int actualWaitTime = waitTime;
                if (randomWait && offsetTime > 0) {
                    // 生成在 [-offsetTime, offsetTime] 范围内的随机偏移量
                    const int randomOffset = QRandomGenerator::global()->bounded(-offsetTime, offsetTime + 1);
                    actualWaitTime = qMax(0, waitTime + randomOffset); // 确保等待时间非负
                }

                if (randomWait) {
                    Logger::log(QStringLiteral("等待%1毫秒（随机偏移，基础时间%2毫秒）...")
                                    .arg(actualWaitTime).arg(waitTime));
                } else {
                    Logger::log(QStringLiteral("等待%1毫秒...").arg(actualWaitTime));
                }

                core::waitWithEventProcessing(actualWaitTime, [this]() { return m_running.load(); });
                break;
        }

        default: {
                Logger::log(QStringLiteral("未知的命令：%1").arg(typeStr));
                break;
        }
    }

    // 步骤级协作守卫用完即恢复到全局基线，不影响后续步骤
    if (stepCollabGuard) {
        actions.setCollaborationGuardEnabled(globalCollabGuard);
    }

    return savePath;
}
