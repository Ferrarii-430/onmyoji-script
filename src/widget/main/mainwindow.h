//
// Created by CZY on 2025/9/25.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <qcoreapplication.h>
#include <QListWidget>
#include <QWidget>
#include <QJsonArray>

QT_BEGIN_NAMESPACE
namespace Ui { class mainwindow; }
QT_END_NAMESPACE

class mainwindow : public QWidget {
Q_OBJECT

public slots:
    void showOpenCVIdentifyImage(const QString& savePath) const; // 如果尚未声明为槽

public:
    Ui::mainwindow *ui;
    QString CONFIG_PATH = QCoreApplication::applicationDirPath() + "/src/resource/config.json";
    QString DX11_CAPTURE_PATH = QCoreApplication::applicationDirPath() + "/src/resource/thumbnail/debug_capture_result.png";
    QString SCREENSHOT_PATH = QCoreApplication::applicationDirPath() + "/src/resource/screenshot/";
    QString THUMBNAIL_PATH = QCoreApplication::applicationDirPath() + "/src/resource/thumbnail/";
    QString DX11_LOG_PATH = QCoreApplication::applicationDirPath() + "/log/dx11_log.txt";
    QString DX11_HOOK_DLL_PATH = QCoreApplication::applicationDirPath() + "/hook/libdx11_hook.dll";
    QString DX11_HOOK_DLL_NAME ="libdx11_hook.dll";
    explicit mainwindow(QWidget *parent = nullptr);
    ~mainwindow() override;
    void appendLogToUI(const QString &msg);
    void onProgrammeAddBtnClicked();
    void onProgrammeRemoveBtnClicked();
    void onSettingBtnClicked();
    void onProgrammeContentAddBtnClicked();
    void onProgrammeUpBtnClicked();
    void onProgrammeDownBtnClicked();

private:
    void loadListWidgetData();
    void onItemClicked(QListWidgetItem *item);
    void showStepsInTable(const QJsonArray &steps);
    void showCurrentSelectStepsInTable();
    void startTaskButtonClick();
    void stopTaskButtonClick();
    bool m_isRunning = false;
    bool isInitLogPath = false;
};


#endif //MAINWINDOW_H
