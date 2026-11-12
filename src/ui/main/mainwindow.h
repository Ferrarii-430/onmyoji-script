//
// 主窗口：只做多实例容器，不持有任何游戏状态。
// 每个标签页是一个 sessionwidget，各自绑定一个游戏实例。
// 全局只执行一次的工作（模型加载、日志分发、退出清理、绑定持久化）放在这里。
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QVector>
#include <QStringList>
#include <QWidget>

class QTabWidget;
class QCloseEvent;
class sessionwidget;

class mainwindow : public QWidget {
Q_OBJECT

public:
    explicit mainwindow(QWidget *parent = nullptr);
    ~mainwindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();

    // 进程级一次性初始化：模型加载、日志分发、更新检查
    void initGlobalServices();

    // 按 windows.json 恢复标签页（仅恢复仍存活的实例）
    void restoreSessions();

    sessionwidget* createSession();
    sessionwidget* currentSession() const;
    void closeSession(int index);

    // 按日志标识找到产生日志的标签页；找不到返回 nullptr
    sessionwidget* findSessionByTag(const QString& tag) const;

    // 是否有任何标签页正在执行任务
    bool anyTaskRunning() const;

    // 把当前所有标签页的绑定写回 windows.json
    void persistSessions();

    // 重新计算「多个标签页绑定同一 PID」的关系，并下发给各标签页做提示。
    // 只提醒不拦截：重复绑定依然允许，但标题角标、状态栏与日志都要可见。
    void refreshSharedBindings();

    // 绑定指定 PID 的其他标签页显示名（排除 self），供绑定时的提醒弹窗使用
    QStringList peerDisplayNamesFor(quint32 pid, sessionwidget* self) const;

    void refreshTabTitle(sessionwidget* session);
    void activateSession(sessionwidget* session);

    // 退出前卸载所有已注入实例的 hook DLL
    void unloadAllHooks();

    // 关闭标签页时若任务在跑，等停止完成后再删
    void closeSessionDeferred(sessionwidget* session);

    QTabWidget *m_tabs = nullptr;

    // 关窗被运行中任务拦截时置位：任务结束后再真正关闭
    bool m_pendingClose = false;

    // 恢复标签页期间抑制持久化，避免 bindInstance 触发反复回写
    bool m_restoring = false;

    // 上一次写入日志的「共用实例」组合，变化时才记日志，避免刷屏
    QString m_lastSharedLog;
};

#endif //MAINWINDOW_H
