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
    // 打印 OpenCV 构建信息（检查 BUILD 时是否包含 AVX/IPP）
    // std::cout << cv::getBuildInformation() << std::endl;

    // 运行时检查 CPU 功能（OpenCV 提供的检查）
    // std::cout << "check CPU AVX: " << cv::checkHardwareSupport(CV_CPU_AVX) << std::endl;
    // std::cout << "check CPU AVX2: " << cv::checkHardwareSupport(CV_CPU_AVX2) << std::endl;

    // 确保没有人为禁用优化
    // std::cout << "UseOptimized: " << cv::useOptimized() << std::endl;

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