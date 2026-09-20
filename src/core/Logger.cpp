#include "src/core/Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <iostream>
#include <utility>

#include "src/core/AppPaths.h"

namespace {

// 单个日志文件大小上限，超出即轮转
constexpr qint64 kMaxLogFileSize = 5 * 1024 * 1024; // 5 MB
// 历史备份数量，生成 app_log.1.txt ~ app_log.N.txt
constexpr int kBackupCount = 2;

} // namespace

Logger::Sink Logger::m_sink = nullptr;
QFile* Logger::m_logFile = nullptr;
bool Logger::m_fileTried = false;

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

    writeToFile(msg);
}

void Logger::log(const std::string& msg)
{
    log(QString::fromUtf8(msg.c_str()));
}

void Logger::writeToFile(const QString& msg)
{
    ensureLogFile();
    if (!m_logFile) {
        return;
    }

    // 读磁盘实际大小判断（每次写完都会 flush），跨重启遗留的超限文件也能及时轮转
    if (QFileInfo(m_logFile->fileName()).size() >= kMaxLogFileSize) {
        rotateLogFile();
        if (!m_logFile) {
            return;
        }
    }

    const QString line = QStringLiteral("[%1] %2\n")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")), msg);
    m_logFile->write(line.toUtf8());
    m_logFile->flush(); // 逐条落盘：崩溃时日志不丢，且下次大小判断准确
}

void Logger::ensureLogFile()
{
    if (m_fileTried) {
        return;
    }
    m_fileTried = true;

    // QCoreApplication 未创建时 applicationDirPath 不可用，本次运行放弃文件输出
    if (!QCoreApplication::instance()) {
        return;
    }

    const QString path = AppPaths::instance().appLogPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile* file = new QFile(path);
    if (!file->open(QIODevice::Append | QIODevice::Text)) {
        delete file;
        return;
    }
    m_logFile = file;
}

void Logger::rotateLogFile()
{
    const QFileInfo currentInfo(m_logFile->fileName());
    const QString currentPath = currentInfo.absoluteFilePath();
    const QString base = currentInfo.absolutePath() + "/" + currentInfo.completeBaseName();

    m_logFile->close();

    // 删除最旧备份，其余逐档后移：app_log.1 -> app_log.2，当前 -> app_log.1
    QFile::remove(QStringLiteral("%1.%2.txt").arg(base).arg(kBackupCount));
    for (int i = kBackupCount - 1; i >= 1; --i) {
        QFile::rename(QStringLiteral("%1.%2.txt").arg(base).arg(i),
                      QStringLiteral("%1.%2.txt").arg(base).arg(i + 1));
    }
    const QString first = QStringLiteral("%1.1.txt").arg(base);
    QFile::remove(first);
    QFile::rename(currentPath, first);

    // 重建新的当前文件；失败则放弃文件输出（m_logFile 置空且不再重试）
    if (!m_logFile->open(QIODevice::Append | QIODevice::Text)) {
        delete m_logFile;
        m_logFile = nullptr;
    }
}
