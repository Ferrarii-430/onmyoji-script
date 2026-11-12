//
// Created by CZY on 2025/10/14.
//

#ifndef SETTINGMANAGER_H
#define SETTINGMANAGER_H
#include <QJsonObject>
#include <QReadLocker>
#include <QReadWriteLock>


// 全局配置。多会话并发后各任务线程会频繁读取这些字段，
// 而设置页保存时会整体重载，因此所有读写都经读写锁保护：
// 读多写少，QReadWriteLock 的并发读开销可以忽略。
class SettingManager {
public:
    // 获取单例实例
    static SettingManager& getInstance() {
        static SettingManager instance;
        return instance;
    }

    // 删除拷贝构造函数和赋值操作符
    SettingManager(const SettingManager&) = delete;
    SettingManager& operator=(const SettingManager&) = delete;

    // 加载配置文件
    bool loadConfig();
    bool loadConfig(const QString& configPath);

    // 获取配置值
    QString getMouseControlMode() const { QReadLocker locker(&m_lock); return m_mouseControlMode; }
    int getMouseSpeed() const { QReadLocker locker(&m_lock); return m_mouseSpeed; }
    QString getScreenshotMode() const { QReadLocker locker(&m_lock); return m_screenshotMode; }
    QString getMouseClickMode() const { QReadLocker locker(&m_lock); return m_mouseClickMode; }
    bool getPersistScreenshot() const { QReadLocker locker(&m_lock); return m_persistScreenshot; }
    // 全局「自动协作关闭」：任务运行期间所有识别点击失败时自动扫协作弹窗并重试一次
    bool getGlobalAutoCancelCollab() const { QReadLocker locker(&m_lock); return m_globalAutoCancelCollab; }

    // 获取原始JSON对象（用于扩展）
    QJsonObject getRawConfig() const { QReadLocker locker(&m_lock); return m_config; }

    // 检查配置是否已加载
    bool isLoaded() const { QReadLocker locker(&m_lock); return m_loaded; }

    // 重新加载配置
    bool reloadConfig();

private:
    SettingManager() = default;
    ~SettingManager() = default;

    bool parseConfig();

    // 保护下列字段。parseConfig 由持写锁的 loadConfig 调用，内部不再加锁
    mutable QReadWriteLock m_lock;

    QString m_configPath;
    QJsonObject m_config;
    bool m_loaded = false;

    // 配置字段
    QString m_mouseControlMode = "BEZIER";  // 默认值
    int m_mouseSpeed = 7;                   // 默认值
    QString m_screenshotMode = "DLL注入";   // 默认值
    QString m_mouseClickMode = "PostMessage";   // 默认值
    // 是否将截图持久化为 PNG 文件；默认值与出厂 setting.json (false) 对齐，
    // 避免配置缺失该字段时退化为开启、每次识别都产生磁盘 IO
    bool m_persistScreenshot = false;
    // 全局「自动协作关闭」，默认关闭；缺失字段时保持关闭
    bool m_globalAutoCancelCollab = false;
};

// 全局访问宏
#define SETTING_CONFIG SettingManager::getInstance()

#endif //SETTINGMANAGER_H
