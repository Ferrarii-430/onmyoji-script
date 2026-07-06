#include "src/core/EventLoopUtils.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
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

} // namespace core
