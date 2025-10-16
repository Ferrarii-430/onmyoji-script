#include "src/widget/main/mainwindow.h"
#include "src/widget/main/ui_mainwindow.h"
#include <iostream>
#include <opencv2/core/utility.hpp>
#include <opencv2/core/utils/logger.hpp>

void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
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

    // 设置OpenCV日志级别（可选）
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);

    qInstallMessageHandler(myMessageHandler); // 重定向所有 qDebug/qWarning
    QApplication a(argc, argv);
    mainwindow w;
    w.show();
    return QApplication::exec();
}