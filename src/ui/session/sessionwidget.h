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
#include <QJsonObject>
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

    // 本标签页的系统方案配置覆盖值：{ "方案 id": { "配置 key": 值 } }。
    // 系统方案参数按游戏实例独立（组队模式、协同身份等都算），
    // config.json 只保留方案默认值，避免多个标签页共用同一方案时互相覆盖。
    QJsonObject systemConfigOverrides() const { return m_systemConfigOverrides; }
    // 恢复持久化的覆盖值（不触发变更信号，避免恢复阶段反复写文件）
    void setSystemConfigOverrides(const QJsonObject& overrides);

    // 与本页绑定同一 PID 的其他标签页（空表示独占），供 shell 拼接提示文案
    QStringList sharedPeers() const { return m_sharedPeers; }

    // 由 shell 注入：查询绑定同一游戏实例的其他标签页显示名（不含本页）。
    // 仅在绑定/启动任务时用于「共用实例」提醒，返回空表示独占。
    void setPeerLookup(std::function<QStringList(DWORD)> lookup);

    // 由 shell 注入：当前是否只有本标签页一个。
    // 只有唯一标签页时无需手动绑定，按旧逻辑自动定位第一个游戏实例；
    // 多标签页时无法判断本页该操作哪个实例，必须由用户显式绑定。
    void setSingleSessionProbe(std::function<bool()> probe);

    // 由 shell 在任意标签页绑定关系变化后调用：
    // peers 为绑定同一 PID 的其他标签页显示名，空表示本页独占该实例。
    // 不拦截操作，只刷新标题角标与绑定状态提示。
    void setSharedPeers(const QStringList& peers);

    // 把 UI 线程的预览上下文指向本实例（编辑器里手动测试步骤时使用）。
    // 任务执行线程不依赖它，任务启动时会固定自己的上下文。
    void activateBinding();

    bool isTaskRunning() const;

    // 其他标签页改动了 config.json（全局共享）后由 shell 调用：
    // 重建本页方案列表并尽量保留本页自己的选中项，不越权改动别的标签页状态。
    // changedConfigId 为受影响的方案 id；列表结构变化（新增/删除/导入）时为空。
    void reloadProfiles(const QString& changedConfigId);

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
    // 本标签页的系统方案配置变化（供 shell 持久化）
    void systemConfigOverridesChanged();
    // 本标签页改动了 config.json（新增/删除/改名方案、增删改步骤、导入配置）。
    // config.json 是全进程共享的，shell 据此让其他标签页重建方案列表，
    // 否则只有动手的那个标签页能看到变化。
    // configId 为受影响的方案 id；列表结构变化（新增/删除/导入）时为空。
    void profilesChanged(const QString& configId);
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

    // 是否唯一标签页；未注入时按「不是唯一」处理（不敢擅自替用户选实例）
    std::function<bool()> m_singleSessionProbe;

    // 是否唯一标签页（未注入 probe 时保守返回 false）
    bool isOnlySession() const;

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

    // 唯一标签页且未手动绑定时，按旧逻辑定位第一个游戏实例并绑定。
    // 成功返回 true 并刷新界面；找不到实例时记日志并返回 false。
    bool autoBindFirstInstance();

    void showSystemConfigForm(const QString &configId);

    // 读取本标签页的配置值：优先取覆盖值，没有则用方案默认值（config.json）
    QJsonValue effectiveSystemConfigValue(const QString& configId, const QString& key,
                                          const QJsonValue& fallback) const;
    // 记录本标签页的配置值并通知 shell 持久化（不写 config.json）
    void saveSystemConfigOverride(const QString& configId, const QString& key,
                                  const QJsonValue& value);
    // 把本标签页当前的配置值写回方案默认值(config.json)，并清掉本页覆盖
    void saveSystemConfigAsDefault(const QString& configId);
    // 丢弃本标签页的配置覆盖，回到方案默认值
    void resetSystemConfigToDefault(const QString& configId);
    // 把本标签页的覆盖值写进「本次运行」的配置副本（不改 config.json，
    // 因此多个标签页同时跑同一方案也不会互相覆盖）
    void applySystemConfigOverrides(QJsonArray& configs, const QString& configId) const;

    // 本标签页的系统方案配置覆盖值（方案 id -> { key: value }）
    QJsonObject m_systemConfigOverrides;
    void setTaskRunningState(bool running);
    void showCycleProgress(int completed, int total);

    // 刷新「绑定窗口：xxx」状态文本
    void updateBindStateLabel();

    void loadListWidgetData();
    // 只重建列表条目，不做选中处理（调用方自己决定选中哪个方案）
    void rebuildProgrammeList();
    // 在列表中选中指定方案，并同步本页的选中状态与标题；方案不存在/被隐藏时返回 false
    bool focusProgrammeInList(const QString& configId);
    // 选中第一个可见方案；一个可见方案都没有时清空选中与内容区
    void selectFirstVisibleProgramme();
    // 按本页当前选择重建内容区（普通方案→步骤表格，系统方案→配置表单）
    void refreshSelectedProgrammeContent();
    // 从 m_configArray 取方案对象，找不到返回空对象
    QJsonObject findConfigById(const QString& configId) const;

    // 重建列表期间置位：itemChanged 只在用户真的改名时才写回文件，
    // 否则程序重建条目会被当成改名，把别的标签页的方案名覆盖掉
    bool m_reloadingProfiles = false;

    void onItemClicked(QListWidgetItem *item);
    void showStepsInTable(const QJsonArray &steps);
    void showCurrentSelectStepsInTable();
    void startTaskButtonClick();
    void stopTaskButtonClick();

    Ui::sessionwidget *ui;
};

#endif //SESSIONWIDGET_H
