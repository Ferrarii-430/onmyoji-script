//
// Created by CZY on 2025/11/10.
//

#ifndef WORKER_H
#define WORKER_H

#pragma once
#include <QObject>
#include <QJsonArray>
#include <atomic>
#include <QThread>

class Worker : public QObject {
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);
    ~Worker();

    void setSteps(const QJsonArray &steps);
    void setCycleCount(int cycles); // <=0 表示无限
public slots:
    void startWork();   // 启动执行（在 worker 所在线程调用）
    void stopWork();    // 请求停止（线程安全）

    signals:
        void logMessage(const QString &msg);
    void showImage(const QString &path); // 主线程显示图片
    void finished(); // 工作完成
    void progress(int current, int total);

private:
    QJsonArray m_steps;
    std::atomic_bool running { true };
    int m_cycles;
};

#endif //WORKER_H
