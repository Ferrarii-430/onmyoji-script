//
// Created by CZY on 2025/11/10.
//

#include "worker.h"
#include <QThread>
#include <QRandomGenerator>
#include <ExecutionSteps.h> // 你已有的执行逻辑
#include <Logger.h>

#include "ConfigTypeEnum.h"

Worker::Worker(QObject *parent)
    : QObject(parent), running(false), m_cycles(1) {}

Worker::~Worker() {}

void Worker::setSteps(const QJsonArray &steps) {
    m_steps = steps;
}

void Worker::setCycleCount(int cycles) {
    m_cycles = cycles;
}

void Worker::stopWork() {
    running.store(true);
    emit logMessage(QString("Worker: 停止请求已发出"));
}

void Worker::startWork() {
    if (m_steps.isEmpty()) {
        emit logMessage("Worker: 步骤为空，退出");
        emit finished();
        return;
    }

    const bool infiniteLoop = (m_cycles <= 0);
    int remaining = m_cycles;
    int totalCycles = infiniteLoop ? -1 : m_cycles;

    do {
        for (int i = 0; i < m_steps.size() && !running.load(); ++i) {
            QJsonObject step = m_steps[i].toObject();
            QString typeStr = step["type"].toString();
            // 使用你原来的 stringToConfigType(...) 函数
            ConfigTypeEnum type = stringToConfigType(typeStr);
            QString savePath;

            // 下面示范 OPENCV 和 WAIT（其余可照搬）
            if (type == ConfigTypeEnum::OPENCV) {
                emit logMessage("Worker: 开始 OPENCV 识别");
                QString imagePath = step["imagePath"].toString();
                double score = step["score"].toDouble();
                bool randomClick = step["randomClick"].toBool();
                int retryCount = 0;
                while (retryCount < 3 && !running.load()) {
                    savePath = ExecutionSteps::getInstance().opencvRecognizesAndClick(imagePath, score, randomClick);
                    if (!savePath.isNull()) {
                        emit showImage(savePath);
                        break;
                    }
                    retryCount++;
                    if (retryCount < 3) {
                        emit logMessage(QString("Worker: 截图失败，第%1次重试").arg(retryCount));
                        QThread::msleep(1000); // 后台线程安全的睡眠
                    }
                }
            } else if (type == ConfigTypeEnum::WAIT) {
                int waitTime = step["time"].toInt();
                bool randomWait = step["randomWait"].toBool();
                int offsetTime = step["offsetTime"].toInt();
                int actualWaitTime = waitTime;
                if (randomWait && offsetTime > 0) {
                    int randomOffset = QRandomGenerator::global()->bounded(-offsetTime, offsetTime + 1);
                    actualWaitTime = qMax(0, waitTime + randomOffset);
                }
                emit logMessage(QString("Worker: 等待 %1 毫秒").arg(actualWaitTime));
                const int interval = 100;
                for (int t = 0; t < actualWaitTime && !running.load(); t += interval) {
                    QThread::msleep(qMin(interval, actualWaitTime - t));
                }
            } else {
                // 其它类型：照你原来的处理填充
                emit logMessage(QString("Worker: 未实现类型 %1，跳过").arg(typeStr));
            }

            // 识别错误/成功的后处理逻辑（跳转、retry、break 等）也要在 worker 中实现
            // 注意：若需要更新 UI（比如表格），发信号到主线程处理。
        }

        if (!infiniteLoop) remaining--;

        // 每次任务循环结束休眠 1s（后台线程）
        QThread::msleep(1000);

    } while (!running.load() && (infiniteLoop || remaining > 0));

    emit logMessage("Worker: 任务循环结束");
    emit finished();
}

