//
// Created by CZY on 2025/9/26.
//

#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <string>

class mainwindow; // 前置声明

class Logger
{
public:
    // 设置全局 mainwindow 指针（程序启动时调用一次）
    static void setMainWindow(mainwindow* win);

    // 打印日志（QString）
    static void log(const QString& msg);

    // 打印日志（std::string）
    static void log(const std::string& msg);

private:
    static mainwindow* m_mainWindow;
};


#endif //LOGGER_H
