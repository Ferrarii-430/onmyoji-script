#include "src/widget/main/mainwindow.h"
#include "src/widget/main/ui_mainwindow.h"
#include <iostream>

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
    qInstallMessageHandler(myMessageHandler); // 重定向所有 qDebug/qWarning
    QApplication a(argc, argv);
    mainwindow w;
    w.show();
    return QApplication::exec();
}