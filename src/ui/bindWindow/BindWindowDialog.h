#ifndef BINDWINDOWDIALOG_H
#define BINDWINDOWDIALOG_H

#include <QDialog>
#include <QString>

#include "src/platform/WindowPicker.h"

class QLabel;
class QPushButton;
class QTableWidget;

// 窗口绑定对话框：列出目标进程的全部实例，支持从列表选择，
// 也支持「点选窗口」（全屏取景器直接点目标游戏窗口）。
// 界面直接用代码构建（与 BossConfigDialog 一致），不引入额外 .ui 文件。
class BindWindowDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BindWindowDialog(QWidget* parent = nullptr);
    ~BindWindowDialog() override;

    // 用户确认的目标窗口；对话框被取消时 valid() 为 false
    const platform::PickedWindow& selected() const { return m_selected; }

    // exec() 的自定义返回码。
    // 点选窗口不能在对话框内部启动遮罩：hide() 一个 exec() 中的模态框再
    // 嵌套 QEventLoop，模态状态与焦点容易挂死（表现为整个程序无响应）。
    // 这里改为带着返回码关闭自己，由 selectWindow 在对话框销毁后再启动。
    enum ResultCode {
        PickFromScreen = 2,
        Unbind = 3
    };

    const QString& processName() const { return m_processName; }

    // 便捷入口：弹出对话框，确认后写入 out。
    // 选择「点选窗口」时会在对话框关闭后启动全屏取景器，取消则回到列表。
    static bool selectWindow(QWidget* parent, platform::PickedWindow* out);

private:
    void buildUi();
    void refreshList();
    void pickFromScreen();
    void confirmSelection();

    // 解除绑定：返回 Unbind 返回码，由 selectWindow 转成「out 为空实例」
    void requestUnbind();

    // 当前选中行的 PID；无选中返回 0
    quint32 currentPid() const;
    void selectRowByPid(quint32 pid);

    QTableWidget* m_table = nullptr;
    QLabel* m_tipLabel = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_pickButton = nullptr;
    QPushButton* m_unbindButton = nullptr;
    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;

    platform::PickedWindow m_selected;
    QString m_processName;

    // 进程名可能被后续改造成可配置，这里集中一处便于调整
    static const QString& targetProcessName();
};

#endif // BINDWINDOWDIALOG_H
