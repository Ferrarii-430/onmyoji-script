#include "src/core/EventLoopUtils.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QRandomGenerator>
#include <QThread>
#include <QTimer>
#include <algorithm>

namespace core {

namespace {

// 等待是否发生在任务执行线程（而非 UI 主线程）。
// 多会话并发后每个任务跑在自己的 worker 线程，Qt 事件循环只应由主线程驱动：
// worker 线程里跑 QEventLoop 既无意义（没人需要它处理事件），
// 又可能在没有事件分发器的线程上产生不确定行为。
bool isWorkerThread()
{
    return QCoreApplication::instance() != nullptr
           && QThread::currentThread() != QCoreApplication::instance()->thread();
}

} // namespace

void waitWithEventProcessing(int milliseconds)
{
    waitWithEventProcessing(milliseconds, nullptr);
}

void waitWithEventProcessing(int milliseconds, const std::function<bool()>& keepWaiting)
{
    if (milliseconds <= 0) {
        if (!isWorkerThread()) {
            QCoreApplication::processEvents(QEventLoop::AllEvents);
        }
        return;
    }

    // worker 线程：纯睡眠，分段检查停止标志。
    // 分段而非一次性 msleep，是为了让「停止任务」能在 20ms 内响应，
    // 而不是等完一个可能长达数秒的等待步骤。
    if (isWorkerThread()) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < milliseconds) {
            if (keepWaiting && !keepWaiting()) {
                break;
            }
            const qint64 remaining = milliseconds - timer.elapsed();
            QThread::msleep(static_cast<unsigned long>(std::max<qint64>(1, std::min<qint64>(20, remaining))));
        }
        return;
    }

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < milliseconds) {
        if (keepWaiting && !keepWaiting()) {
            break;
        }

        const int remaining = milliseconds - static_cast<int>(timer.elapsed());
        const int slice = std::min(50, remaining);

        QEventLoop loop;
        QTimer::singleShot(slice, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::AllEvents);
    }

    QCoreApplication::processEvents(QEventLoop::AllEvents);
}

void waitRandomWithEventProcessing(int milliseconds, int offsetMilliseconds)
{
    int actualMilliseconds = milliseconds;
    if (offsetMilliseconds > 0) {
        // 生成在 [-offsetMilliseconds, offsetMilliseconds] 范围内的随机偏移量
        const int randomOffset = QRandomGenerator::global()->bounded(-offsetMilliseconds, offsetMilliseconds + 1);
        actualMilliseconds = qMax(0, milliseconds + randomOffset);
    }
    waitWithEventProcessing(actualMilliseconds);
}

} // namespace core
