#ifndef WINDOWBINDINGSTORE_H
#define WINDOWBINDINGSTORE_H

#include <QJsonObject>
#include <QString>
#include <QVector>

// 一条窗口绑定记录。
// PID 只在本次运行周期内可靠（游戏重启后会变），因此持久化时同时记录
// 可执行文件路径与窗口标题，供下次启动做「重新匹配」的候选依据。
struct WindowBinding {
    int slot = 0;           // 槽位序号，从 1 开始，用于 tab 标题兜底
    QString displayName;    // 用户可见名称；空则显示「实例 N」
    quint32 pid = 0;        // 绑定时的进程 PID
    QString imagePath;      // 可执行文件完整路径
    QString imageName;      // 可执行文件名，如 onmyoji.exe
    QString windowTitle;    // 绑定时的窗口标题
    qint64 boundAt = 0;     // 绑定时间戳（Unix 秒）

    bool isValid() const { return pid != 0; }

    // tab / 列表标题：优先用户命名，其次窗口标题，最后回退「实例 N」
    QString tabTitle() const;

    QJsonObject toJson() const;
    static WindowBinding fromJson(const QJsonObject& obj);
};

// 绑定信息持久化（src/resource/windows.json）。
// 后续多 tab 改造由它提供初始 tab 列表；当前版本供主界面展示绑定状态。
class WindowBindingStore {
public:
    static WindowBindingStore& instance() {
        static WindowBindingStore instance;
        return instance;
    }

    WindowBindingStore(const WindowBindingStore&) = delete;
    WindowBindingStore& operator=(const WindowBindingStore&) = delete;

    // 从配置文件读取；文件不存在视为空列表并返回 true
    bool load();
    bool save() const;
    bool isLoaded() const { return m_loaded; }

    const QVector<WindowBinding>& all() const { return m_items; }
    int count() const { return m_items.size(); }
    bool isEmpty() const { return m_items.isEmpty(); }

    // 追加一条绑定并自动分配槽位，返回其下标
    int add(const WindowBinding& binding);
    bool removeAt(int index);
    bool updateAt(int index, const WindowBinding& binding);
    void clear();
    void clearSlots();

    // 按 PID 查找，未找到返回 -1
    int indexOfPid(quint32 pid) const;

    // 清掉已失效（进程不存在）的记录，返回移除条数
    int removeDead();

    static QString filePath();

private:
    WindowBindingStore() = default;
    ~WindowBindingStore() = default;

    QVector<WindowBinding> m_items;
    bool m_loaded = false;
};

// 全局访问宏，与 SETTING_CONFIG 风格一致
#define WINDOW_BINDINGS WindowBindingStore::instance()

#endif // WINDOWBINDINGSTORE_H
