#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <functional>
#include <string>

class QFile;

// 全局日志入口。输出三路：
//   1. stdout（GUI 子系统下不可见，保留调试用）
//   2. UI 回调（setSink 注册，同步显示到界面）
//   3. 日志文件（src/resource/log/app_log.txt，带时间戳）
// 文件有大小限制：超过 5MB 自动轮转，最多保留 2 个历史备份（app_log.1/2.txt）。
//
// 多线程：各会话的任务跑在独立 worker 线程里，日志会并发写入。
//   - 文件写入与 sink 存取都由内部互斥量保护；
//   - 线程可通过 setThreadTag 标明自己属于哪个会话，sink 收到 tag 后
//     把日志投递到对应标签页，避免 A 实例的日志写进 B 的日志框。
class Logger
{
public:
    // tag 为产生该条日志的会话标识（由 setThreadTag 设置），空表示 UI 主线程
    using Sink = std::function<void(const QString& tag, const QString& msg)>;

    static void setSink(Sink sink);

    static void log(const QString& msg);
    static void log(const std::string& msg);

    // 当前线程的会话标识。worker 线程启动时设置，线程内所有日志都会带上它
    static void setThreadTag(const QString& tag);
    static QString threadTag();

private:
    // 带时间戳写入文件一条；文件超限（含跨重启遗留的大文件）时先轮转
    static void writeToFile(const QString& msg);
    // 惰性打开日志文件并确保目录存在；打开失败本次运行不再重试
    static void ensureLogFile();
    // 轮转：删除最旧备份，逐档后移（.1 -> .2），当前文件改名为 .1，再重建当前文件
    static void rotateLogFile();

    static Sink m_sink;
    static QFile* m_logFile;    // 打开中的日志文件；nullptr 表示未打开或已放弃
    static bool m_fileTried;    // 文件打开是否已尝试过
};

#endif //LOGGER_H
