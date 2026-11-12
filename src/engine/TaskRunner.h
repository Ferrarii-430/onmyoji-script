#ifndef TASKRUNNER_H
#define TASKRUNNER_H

#include <QJsonArray>
#include <QObject>
#include <QString>
#include <atomic>

#include <opencv2/core/mat.hpp>

#include "src/core/ProfileStore.h"
#include "src/game/GameContext.h"

class QThread;

// 任务执行器：按方案步骤循环执行识别/点击/等待等动作。
//
// 每个会话（标签页）持有自己的一个实例，start() 会把执行体放到独立线程：
//   - 执行线程内 GameWindow / ScriptActions / 截图通道都是线程局部对象，
//     因此多个实例可以同时操作不同的游戏窗口，互不干扰；
//   - 信号跨线程自动排队投递回 UI 线程，由各会话自己的槽接收。
//
// 场景代码通过 TaskRunner::instance() 判断是否该继续（检查停止标志），
// 该方法返回「当前线程正在执行的那个实例」，未在运行线程中时返回空壳对象。
class TaskRunner : public QObject
{
    Q_OBJECT

public:
    explicit TaskRunner(QObject* parent = nullptr);
    ~TaskRunner() override;

    TaskRunner(const TaskRunner&) = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;

    // 当前线程的执行器。任务线程内由 runLoop 设置；
    // 其他线程（如 UI 线程）返回一个永不运行的空壳实例，保证调用安全。
    static TaskRunner& instance();

    // 是否有任何会话正在执行任务（供设置页等跨会话的全局判断使用）
    static bool isAnyRunning();

    // 异步启动任务：立即返回，执行体在本实例的 worker 线程内运行。
    // ctx 固定本次任务操作的游戏实例；item/configs 是本会话选中的方案与配置表，
    // 运行期间不会改变，因此多个会话可以同时各跑各的。
    void start(const QJsonArray& steps, int cycleCount, const GameContext& ctx,
               const ItemInfo& item, const QJsonArray& configs);

    // 协作式停止：立即把运行态置为 false（场景的轮询会随之退出），
    // 正在执行的单步动作会自然跑完，结束后发 finished。
    void stop();

    bool isRunning() const { return m_running.load(); }

signals:
    void showImage(const QString& savePath);

    // 内存回显模式：结果图不落盘，随信号直接送到界面。
    // cv::Mat 拷贝为浅拷贝（共享像素 + 原子引用计数），跨线程只读安全。
    void showImageData(const QString& key, const cv::Mat& image);

    void started();
    void finished();

    // 每完成一轮循环上报一次进度；total <= 0 表示无限循环
    void cycleProgress(int completed, int total);

private:
    void runLoop();                                  // worker 线程入口
    QString executeStep(const QJsonObject& step);    // 执行单个步骤

    // 当前线程正在执行的 TaskRunner（线程局部）
    static thread_local TaskRunner* t_current;

    // 全局运行中的任务数，用于 isAnyRunning()
    static std::atomic<int> s_activeTasks;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};

    QThread* m_thread = nullptr;

    // 启动时固定的执行上下文
    GameContext m_ctx;
    QJsonArray m_steps;
    int m_cycleCount = 1;

    // 本会话选中的方案与配置表：worker 线程启动时写入自己的 thread_local 副本，
    // 场景代码通过 currentItem / m_configArray 读取时拿到的就是本实例的
    ItemInfo m_item;
    QJsonArray m_configs;

    // 上次下发日志路径的实例 PID：跨实例复用时需要重新下发
    bool m_isInitLogPath = false;
    quint32 m_logPid = 0;
};

// 内存回显的结果图通过队列信号跨线程传递，必须先声明元类型
Q_DECLARE_METATYPE(cv::Mat)

#endif // TASKRUNNER_H
