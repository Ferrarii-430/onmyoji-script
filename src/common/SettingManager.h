//
// Created by CZY on 2025/10/14.
//

#ifndef SETTINGMANAGER_H
#define SETTINGMANAGER_H
#include <QJsonObject>


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
    QString getMouseControlMode() const { return m_mouseControlMode; }
    int getMouseSpeed() const { return m_mouseSpeed; }
    QString getScreenshotMode() const { return m_screenshotMode; }
    QString getMouseClickMode() const { return m_mouseClickMode; }

    // 获取原始JSON对象（用于扩展）
    QJsonObject getRawConfig() const { return m_config; }

    // 检查配置是否已加载
    bool isLoaded() const { return m_loaded; }

    // 重新加载配置
    bool reloadConfig();

private:
    SettingManager() = default;
    ~SettingManager() = default;

    bool parseConfig();

    QString m_configPath;
    QJsonObject m_config;
    bool m_loaded = false;

    // 配置字段
    QString m_mouseControlMode = "BEZIER";  // 默认值
    int m_mouseSpeed = 7;                   // 默认值
    QString m_screenshotMode = "DLL注入";   // 默认值
    QString m_mouseClickMode = "PostMessage";   // 默认值
};

// 全局访问宏
#define SETTING_CONFIG SettingManager::getInstance()

#endif //SETTINGMANAGER_H
