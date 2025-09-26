//
// Created by CZY on 2025/9/26.
//

#include "Logger.h"
#include "src/widget/main/mainwindow.h"
#include <iostream>
#include <QTextCursor>

mainwindow* Logger::m_mainWindow = nullptr;

void Logger::setMainWindow(mainwindow* win)
{
    m_mainWindow = win;
}

void Logger::log(const QString &msg)
{
    std::cout << msg.toUtf8().constData() << std::endl;

    if (m_mainWindow) {
        m_mainWindow->appendLogToUI(msg);  // 通过接口访问 UI
    }
}

void Logger::log(const std::string& msg)
{
    // 转成 QString 后调用上面的函数
    log(QString::fromUtf8(msg.c_str()));
}
