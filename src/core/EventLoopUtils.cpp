#include "src/core/EventLoopUtils.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QRandomGenerator>
#include <QTimer>
#include <algorithm>

namespace core {

void waitWithEventProcessing(int milliseconds)
{
    waitWithEventProcessing(milliseconds, nullptr);
}

void waitWithEventProcessing(int milliseconds, const std::function<bool()>& keepWaiting)
{
    if (milliseconds <= 0) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
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
