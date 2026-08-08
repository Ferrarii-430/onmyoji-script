//
// Created by CZY on 2025/9/30.
//

#ifndef COMMON_H
#define COMMON_H
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

struct ItemInfo {
    QString taskName;
    QString id;
};

inline QJsonArray m_configArray;
inline ItemInfo currentItem;
QString getPathByRecognitionImg(const std::string &configId, const std::string &fileName);
void addConfigToJsonFile(const QString &filePath, const QString &name);
bool removeConfigById(const QString &filePath, const QString &idToRemove);
void addConfigToJsonFile(const QString &filePath, const QString &configId, const QJsonObject& json);
void saveBase64ImageToFile(QJsonObject &data);
QString cleanString(const QString &s);
QJsonValue safeValue(const QJsonObject &obj, const QString &key);
bool removeConfigById(const QString &filePath, const QString &configId, const QString &stepsId);
void updateConfigInJsonFile(const QString &filePath, const QString &configId, const QJsonObject& json);
void updateProgrammeContent(const QString &filePath, const QString &configId, const QString &name);
bool moveProgramme(const QString &filePath, const QString &configId, int stepsIndex, bool status);
void refreshConfig();
QJsonArray getConfigJSON();
QJsonArray getLastConfigJSON();
void commonSetCurrentItem(const QString &id, const QString &taskName);
QMap<QString,QString> getStepsSelect(const QString &configId, const QString &currentStepsId);

// ------------------------------
// 系统方案自定义配置 systemConfig 的读写
// 每个系统方案(config.json 里 type=="system")可带一个 systemConfig 数组，
// 数组每项既描述控件(control/label/min/max/...)又存当前值(value)，key 为唯一标识。
// 每个方案对象还可带顶层 tip 字段，用于在 UI 上显示该方案的功能使用说明；
// systemConfig 数组每项也可带 tip 字段，用于解释单个配置项。
// ------------------------------
// 读取某方案完整的 systemConfig 数组(含控件描述)，无则返回空数组，用于 UI 渲染表单
QJsonArray getSystemConfig(const QString &configId);
// 读取某方案顶层 tip 字段(方案功能使用说明)，无则返回空字符串
QString getSystemTip(const QString &configId);
// 按 key 读取某方案的配置值(只取 value)，找不到时返回 defaultValue，用于场景运行时取值
QJsonValue getSystemConfigValue(const QString &configId, const QString &key,
                                const QJsonValue &defaultValue = QJsonValue());
// 更新某方案某 key 的配置值并写回文件(只改 value，不动控件描述)
void updateSystemConfigValue(const QString &filePath, const QString &configId,
                             const QString &key, const QJsonValue &value);
#endif //COMMON_H
