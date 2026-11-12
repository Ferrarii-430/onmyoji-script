//
// 单个游戏实例的操作面板。
// 每个标签页一个实例，各自持有方案选择、运行状态、日志与窗口绑定。
//
// 并发模型：每个会话持有自己的 TaskRunner，任务在独立 worker 线程执行，
// 执行线程内绑定本会话的 PID/窗口（线程局部上下文），因此多个标签页
// 可以同时运行且互不影响。信号由各会话自己的 runner 发出，天然隔离。
//

#ifndef SESSIONWIDGET_H
#define SESSIONWIDGET_H

#include <windows.h>
#include <QListWidget>
#include <QWidget>
#include <QJsonArray>
#include <QStringList>
#include <functional>
#include <opencv2/core/mat.hpp>

#include "src/core/ProfileStore.h"

QT_BEGIN_NAMESPACE
namespace Ui { class sessionwidget; }
QT_END_NAMESPACE

class QLabel;
class TaskRunner;
struct GameContext;

class sessionwidget : public QWidget {
Q_OBJECT

public:
    explicit sessionwidget(QWidget *parent = nullptr);
    ~sessionwidget() override;

    // 标签页标题：已绑定显示实例 PID，未绑定显示「未绑定」
    QString tabTitle() const;

    // 接口层展示名（供持久化使用）
    QString displayName() const;

    DWORD boundPid() const { return m_boundPid; }
    HWND boundHwnd() const { return m_boundHwnd; }

    // 日志分发标识：shell 据此把 worker 线程的日志投递到本标签页
    QString logTag() const { return m_logTag; }

    // 记录绑定并刷新界面
    void bindInstance(DWORD pid, HWND hwnd, const QString& displayName = QString());

    // 与本页绑定同一 PID 的其他标签页（空表示独占），供 shell 拼接提示文案
    QStringList sharedPeers() const { return m_sharedPeers; }

    // 由 shell 注入：查询绑定同一游戏实例的其他标签页显示名（不含本页）。
    // 仅在绑定/启动任务时用于「共用实例」提醒，返回空表示独占。
    void setPeerLookup(std::function<QStringList(DWORD)> lookup);

    // 由 shell 在任意标签页绑定关系变化后调用：
    // peers 为绑定同一 PID 的其他标签页显示名，空表示本页独占该实例。
    // 不拦截操作，只刷新标题角标与绑定状态提示。
    void setSharedPeers(const QStringList& peers);

    // 把 UI 线程的预览上下文指向本实例（编辑器里手动测试步骤时使用）。
    // 任务执行线程不依赖它，任务启动时会固定自己的上下文。
    void activateBinding();

    bool isTaskRunning() const;

    // 协作式停止：请求停止但不阻塞等待，结束时会发 taskFinished
    void stopTask();

    void appendLogToUI(const QString &msg);

public slots:
    // providedImage 非空时直接显示（内存回显跨线程送达）；
    // 为空时按 savePath 从磁盘或本线程缓存加载
    void showOpenCVIdentifyImage(const QString& savePath,
                                 const cv::Mat& providedImage = cv::Mat()) const;

signals:
    // 绑定信息变化（供 shell 持久化）
    void bindingChanged();
    // 标签页标题需要更新
    void titleChanged();
    // 任务开始/结束
    void taskRunningChanged(bool running);
    void taskFinished();

public:
    void onProgrammeAddBtnClicked();
    void onProgrammeRemoveBtnClicked();
    void onSettingBtnClicked();
    void onBindWindowBtnClicked();
    void onProgrammeContentAddBtnClicked();
    void onProgrammeUpBtnClicked();
    void onProgrammeDownBtnClicked();

private:
    // 系统方案的动态配置表单容器(按 systemConfig 描述生成，改动即持久化)
    QWidget *m_systemConfigForm = nullptr;

    // 任务运行期间替换 QSpinBox 的循环进度标签，停止后销毁恢复默认
    QLabel *m_cycleProgressLabel = nullptr;

    // 本实例绑定的游戏窗口
    DWORD m_boundPid = 0;
    HWND m_boundHwnd = nullptr;
    QString m_displayName;

    // 与本页绑定同一 PID 的其他标签页显示名（空表示独占该实例）。
    // 仅作提示用：允许共用，但界面与日志都要明确告知可能互相干扰。
    QStringList m_sharedPeers;
    std::function<QStringList(DWORD)> m_peerLookup;

    // 已经就「共用实例」提醒过的 PID，避免每次启动任务都弹窗；
    // 冲突解除后清零，下次再共用会重新提醒
    DWORD m_warnedSharedPid = 0;

    // 本标签页当前选中的方案。
    // 全局 currentItem 是所有会话共用的「最后一次点击」，
    // 启动任务必须以本标签页自己的选择为准，否则会被别的标签页覆盖。
    ItemInfo m_selectedItem;

    // 本会话专属的任务执行器与日志标识
    TaskRunner* m_runner = nullptr;
    QString m_logTag;

    // 组装启动任务时的执行上下文（固定 pid/hwnd，任务线程据此绑定窗口）
    GameContext buildContext() const;

    void showSystemConfigForm(const QString &configId);
    void setTaskRunningState(bool running);
    void showCycleProgress(int completed, int total);

    // 刷新「绑定窗口：xxx」状态文本
    void updateBindStateLabel();

    void loadListWidgetData();
    void onItemClicked(QListWidgetItem *item);
    void showStepsInTable(const QJsonArray &steps);
    void showCurrentSelectStepsInTable();
    void startTaskButtonClick();
    void stopTaskButtonClick();

    Ui::sessionwidget *ui;
};

#endif //SESSIONWIDGET_H
