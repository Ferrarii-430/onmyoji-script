#ifndef TASKRUNNER_H
#define TASKRUNNER_H

#include <QJsonArray>
#include <QObject>
#include <QString>

// 任务执行器：负责按方案步骤循环执行识别/点击/等待等动作。
// UI 层只需调用 run()/stop() 并连接信号显示进度。
class TaskRunner : public QObject
{
    Q_OBJECT

public:
    static TaskRunner& instance();

    TaskRunner(const TaskRunner&) = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;

    // 同步执行步骤列表（内部持续处理事件循环保持 UI 响应）。
    // cycleCount <= 0 表示无限循环。
    void run(const QJsonArray& steps, int cycleCount);

    void stop();
    bool isRunning() const { return m_isRunning; }

signals:
    void showImage(const QString& savePath);
    void started();
    void finished();

private:
    TaskRunner() = default;

    // 执行单个步骤，返回识别结果图路径（失败为空）
    QString executeStep(const QJsonObject& step);

    bool m_isRunning = false;
    bool m_isInitLogPath = false;
};

#endif // TASKRUNNER_H
