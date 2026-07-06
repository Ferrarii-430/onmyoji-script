#include "src/core/Logger.h"

#include <iostream>
#include <utility>

Logger::Sink Logger::m_sink = nullptr;

void Logger::setSink(Sink sink)
{
    m_sink = std::move(sink);
}

void Logger::log(const QString& msg)
{
    std::cout << msg.toUtf8().constData() << std::endl;

    if (m_sink) {
        m_sink(msg);
    }
}

void Logger::log(const std::string& msg)
{
    log(QString::fromUtf8(msg.c_str()));
}
