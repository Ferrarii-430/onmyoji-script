#include <QApplication>
#include <iostream>
#include <opencv2/core/utility.hpp>
#include <opencv2/core/utils/logger.hpp>

#include "src/ui/main/mainwindow.h"
#include "src/ui/theme/Theme.h"

// 重定向所有 qDebug/qWarning 到标准输出
static void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(context)
    switch (type) {
    case QtDebugMsg:
        std::cout << msg.toUtf8().constData() << std::endl;
        break;
    case QtWarningMsg:
        std::cout << "[Warning] " << msg.toUtf8().constData() << std::endl;
        break;
    case QtCriticalMsg:
        std::cerr << "[Critical] " << msg.toUtf8().constData() << std::endl;
        break;
    case QtFatalMsg:
        std::cerr << "[Fatal] " << msg.toUtf8().constData() << std::endl;
        abort();
    }
}

int main(int argc, char* argv[])
{
    // 禁用OpenCV的优化（包括SIMD指令）
    cv::setUseOptimized(false);

    // 设置OpenCV日志级别
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);

    qInstallMessageHandler(myMessageHandler);

    QApplication a(argc, argv);
    theme::apply(a);
    mainwindow w;
    w.show();

    return QApplication::exec();
}
