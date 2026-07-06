//
// Created by CZY on 2025/9/25.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QListWidget>
#include <QWidget>
#include <QJsonArray>

QT_BEGIN_NAMESPACE
namespace Ui { class mainwindow; }
QT_END_NAMESPACE

class mainwindow : public QWidget {
Q_OBJECT

public slots:
    void showOpenCVIdentifyImage(const QString& savePath) const;

public:
    Ui::mainwindow *ui;
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
};


#endif //MAINWINDOW_H
