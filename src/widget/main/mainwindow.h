//
// Created by CZY on 2025/9/25.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <qcoreapplication.h>
#include <QListWidget>
#include <QWidget>
#include <QJsonArray>


struct ItemInfo {
    QString taskName;
    QString id;
};

QT_BEGIN_NAMESPACE
namespace Ui { class mainwindow; }
QT_END_NAMESPACE

class mainwindow : public QWidget {
Q_OBJECT

public:
    Ui::mainwindow *ui;
    QJsonArray m_configArray;
    ItemInfo currentItem;
    QString CONFIG_PATH = QCoreApplication::applicationDirPath() + "/src/resource/config.json";
    explicit mainwindow(QWidget *parent = nullptr);
    ~mainwindow() override;
    void appendLog(const QString &log) const;
    void appendLogToUI(const QString &msg);
    void onProgrammeAddBtnClicked();
    void onProgrammeRemoveBtnClicked();
    void onProgrammeContentAddBtnClicked();
    void showOpenCVIdentifyImage(const QString& savePath) const;

private:
    void loadListWidgetData();
    void onItemClicked(QListWidgetItem *item);
    void loadConfig();
    void showStepsInTable(const QJsonArray &steps);
    void showCurrentSelectStepsInTable();
    void startTaskButtonClick();
    void stopTaskButtonClick();
    void setCurrentItem(const QString &id, const QString &taskName);
    bool m_isRunning = false;
};


#endif //MAINWINDOW_H
