#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <functional>
#include <string>

// 全局日志入口。默认输出到 stdout；
// UI 层可通过 setSink 注册回调，把日志同步显示到界面。
class Logger
{
public:
    using Sink = std::function<void(const QString&)>;

    static void setSink(Sink sink);

    static void log(const QString& msg);
    static void log(const std::string& msg);

private:
    static Sink m_sink;
};

#endif //LOGGER_H
