#include "SettingManager.h"

#include <qcoreapplication.h>
#include <QDebug>
#include <QDir>
#include <qjsondocument.h>
#include <qjsonparseerror.h>

bool SettingManager::loadConfig() {
    // 构建默认配置文件路径
    QString basePath = QCoreApplication::applicationDirPath();
    m_configPath = basePath + "/src/resource/setting.json";

    return loadConfig(m_configPath);
}

bool SettingManager::loadConfig(const QString& configPath) {
    m_configPath = configPath;

    QFile configFile(m_configPath);
    if (!configFile.exists()) {
        qWarning() << "配置文件不存在:" << m_configPath;
        m_loaded = false;
        return false;
    }

    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开配置文件:" << m_configPath;
        m_loaded = false;
        return false;
    }

    QByteArray configData = configFile.readAll();
    configFile.close();

    QJsonParseError parseError;
    QJsonDocument configDoc = QJsonDocument::fromJson(configData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON解析错误:" << parseError.errorString();
        m_loaded = false;
        return false;
    }

    if (!configDoc.isObject()) {
        qWarning() << "配置文件格式错误: 根元素不是对象";
        m_loaded = false;
        return false;
    }

    m_config = configDoc.object();
    m_loaded = parseConfig();

    if (m_loaded) {
        qInfo() << "配置文件加载成功:" << m_configPath;
    }

    return m_loaded;
}

bool SettingManager::parseConfig() {
    // 解析鼠标控制模式
    if (m_config.contains("mouseControlMode") && m_config["mouseControlMode"].isString()) {
        m_mouseControlMode = m_config["mouseControlMode"].toString();
    } else {
        qWarning() << "配置缺少 mouseControlMode 字段或类型错误，使用默认值:" << m_mouseControlMode;
    }

    // 解析鼠标速度
    if (m_config.contains("mouseSpeed") && m_config["mouseSpeed"].isDouble()) {
        m_mouseSpeed = m_config["mouseSpeed"].toInt();
        // 验证范围
        if (m_mouseSpeed < 1 || m_mouseSpeed > 10) {
            qWarning() << "mouseSpeed 值超出范围 (1-10)，使用默认值: 7";
            m_mouseSpeed = 7;
        }
    } else {
        qWarning() << "配置缺少 mouseSpeed 字段或类型错误，使用默认值:" << m_mouseSpeed;
    }

    // 解析截图模式
    if (m_config.contains("screenshotMode") && m_config["screenshotMode"].isString()) {
        m_screenshotMode = m_config["screenshotMode"].toString();
    } else {
        qWarning() << "配置缺少 screenshotMode 字段或类型错误，使用默认值:" << m_screenshotMode;
    }

    return true;
}

bool SettingManager::reloadConfig() {
    if (m_configPath.isEmpty()) {
        return loadConfig();
    }
    return loadConfig(m_configPath);
}
