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

class QLabel;
class QCloseEvent;

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

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // 系统方案的动态配置表单容器(按 systemConfig 描述生成，改动即持久化)
    QWidget *m_systemConfigForm = nullptr;

    // 任务运行期间替换 QSpinBox 的循环进度标签，停止后销毁恢复默认
    QLabel *m_cycleProgressLabel = nullptr;

    // 关窗被运行中任务拦截时置位：finished 信号到来后再真正关闭窗口
    bool m_pendingClose = false;

    void showSystemConfigForm(const QString &configId);
    void setTaskRunningState(bool running);
    void showCycleProgress(int completed, int total);

    // 退出前把游戏进程内的 hook DLL 安全卸载（未注入时为无操作）
    void unloadHookOnExit();

    void loadListWidgetData();
    void onItemClicked(QListWidgetItem *item);
    void showStepsInTable(const QJsonArray &steps);
    void showCurrentSelectStepsInTable();
    void startTaskButtonClick();
    void stopTaskButtonClick();
};


#endif //MAINWINDOW_H
