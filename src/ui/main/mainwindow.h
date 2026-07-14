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
    // 系统方案的动态配置表单容器(按 systemConfig 描述生成，改动即持久化)
    QWidget *m_systemConfigForm = nullptr;
    void showSystemConfigForm(const QString &configId);

    void loadListWidgetData();
    void onItemClicked(QListWidgetItem *item);
    void showStepsInTable(const QJsonArray &steps);
    void showCurrentSelectStepsInTable();
    void startTaskButtonClick();
    void stopTaskButtonClick();
};


#endif //MAINWINDOW_H
