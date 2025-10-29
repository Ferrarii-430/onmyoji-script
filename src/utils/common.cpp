//
// Created by CZY on 2025/9/30.
//

#include "common.h"
#include <qcoreapplication.h>
#include "string"
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QPixmap>
#include <QString>

#include "ConfigManager.h"
#include "Logger.h"



QString getPathByRecognitionImg(const std::string &configId, const std::string &fileName)
{
    // 基础路径
    QString basePath = QCoreApplication::applicationDirPath()
                     + "/src/resource/screenshot/"
                     + QString::fromStdString(configId);

    // 确保目录存在
    QDir dir(basePath);
    if (!dir.exists()) {
        dir.mkpath("."); // 创建所有必要的父目录
    }

    // 拼接完整文件路径
    QString fullPath = basePath + "/" + QString::fromStdString(fileName);
    return fullPath;
}

// 往配置文件里追加一个对象
void addConfigToJsonFile(const QString &filePath, const QString &name)
{
    QFile file(filePath);
    QJsonArray rootArray = m_configArray;

    // 2. 构造新对象
    QJsonObject newObj;
    newObj["id"]   = QUuid::createUuid().toString(QUuid::WithoutBraces);  // UUID
    newObj["name"] = name;
    newObj["type"] = "normal";
    newObj["steps"] = QJsonArray();  // 空数组

    // 3. 插入到数组里
    rootArray.append(newObj);

    // 4. 写回文件
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument newDoc(rootArray);
        file.write(newDoc.toJson(QJsonDocument::Indented));
        file.close();
        Logger::log("已成功写入新配置: " + name);
    } else {
        Logger::log("写入 JSON 文件失败: " + filePath);
    }
}

bool removeConfigById(const QString &filePath, const QString &idToRemove)
{
    QFile file(filePath);
    if (!file.exists()) {
        Logger::log("JSON 文件不存在: " + filePath);
        return false;
    }

    QJsonArray rootArray = m_configArray;

    // 2. 遍历删除
    bool removed = false;
    for (int i = 0; i < rootArray.size(); ++i) {
        QJsonObject obj = rootArray[i].toObject();
        if (obj["id"].toString() == idToRemove) {
            rootArray.removeAt(i);
            removed = true;
            break;
        }
    }

    if (!removed) {
        Logger::log("未找到指定 ID: " + idToRemove);
        return false;
    }

    // 3. 写回文件
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument newDoc(rootArray);
        file.write(newDoc.toJson(QJsonDocument::Indented));
        file.close();
        Logger::log("已删除 ID=" + idToRemove + " 的配置");
        return true;
    } else {
        Logger::log("写入 JSON 文件失败: " + filePath);
        return false;
    }
}


// 往配置文件里追加一个对象 - 添加方案内容
void addConfigToJsonFile(const QString &filePath, const QString &configId, const QJsonObject& json)
{
    QFile file(filePath);
    QJsonArray rootArray = m_configArray;


    //2.遍历找到对应的方案ID下标
    bool hasConfigId = false;
    for (int i = 0; i < rootArray.size(); ++i) {
        QJsonObject obj = rootArray[i].toObject();

        QString idInJsonClean = cleanString(obj["id"].toString());
        QString configIdClean = cleanString(configId);
        if (idInJsonClean.compare(configIdClean, Qt::CaseInsensitive) == 0) {
            //构造新对象
            QJsonObject newObj;
            newObj["stepsId"]    = safeValue(json, "stepsId");
            newObj["taskName"]   = safeValue(json, "taskName");
            newObj["type"]       = safeValue(json, "type");
            newObj["score"]      = safeValue(json, "score");
            newObj["ocrText"]    = safeValue(json, "ocrText");
            newObj["randomClick"]= safeValue(json, "randomClick");
            newObj["imagePath"]  = safeValue(json, "image");
            newObj["time"]       = safeValue(json, "time");
            newObj["randomWait"] = safeValue(json, "randomWait");
            newObj["offsetTime"] = safeValue(json, "offsetTime");
            newObj["jumpStepsId"] = safeValue(json, "jumpStepsId");
            newObj["identifyErrorHandle"] = safeValue(json, "identifyErrorHandle");

            //获取原来的 steps 数组并修改
            QJsonArray stepsArray = obj.value("steps").toArray();
            stepsArray.append(newObj);

            //写回到 obj
            obj["steps"] = stepsArray;

            //写回到 rootArray
            rootArray[i] = obj;

            hasConfigId = true;
            break;
        }
    }

    // 写回文件
    if (hasConfigId) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QJsonDocument newDoc(rootArray);
            file.write(newDoc.toJson(QJsonDocument::Indented));
            file.close();
            Logger::log("已成功写入新配置: " + json["taskName"].toString());
        } else {
            Logger::log("写入 JSON 文件失败: " + filePath);
        }
    } else {
        Logger::log("未找到 configId: " + configId);
    }
}


// 更新配置文件中的步骤对象 - 更新方案内容
void updateConfigInJsonFile(const QString &filePath, const QString &configId, const QJsonObject& json) {
    QFile file(filePath);
    QJsonArray rootArray = m_configArray;


    // 2. 遍历找到对应的方案ID和步骤ID
    bool hasConfigId = false;
    bool hasStepsId = false;
    for (int i = 0; i < rootArray.size(); ++i) {
        QJsonObject obj = rootArray[i].toObject();

        QString idInJsonClean = cleanString(obj["id"].toString());
        QString configIdClean = cleanString(configId);
        if (idInJsonClean.compare(configIdClean, Qt::CaseInsensitive) == 0) {
            hasConfigId = true;

            // 获取steps数组
            QJsonArray stepsArray = obj.value("steps").toArray();

            // 遍历steps数组找到对应的stepsId
            for (int j = 0; j < stepsArray.size(); ++j) {
                QJsonObject stepObj = stepsArray[j].toObject();
                if (stepObj["stepsId"].toString() == json["stepsId"].toString()) {
                    // 更新步骤对象
                    QJsonObject updatedStep;
                    updatedStep["stepsId"]    = safeValue(json, "stepsId");
                    updatedStep["taskName"]   = safeValue(json, "taskName");
                    updatedStep["type"]       = safeValue(json, "type");
                    updatedStep["score"]      = safeValue(json, "score");
                    updatedStep["ocrText"]    = safeValue(json, "ocrText");
                    updatedStep["randomClick"]= safeValue(json, "randomClick");
                    updatedStep["imagePath"]  = safeValue(json, "image");
                    updatedStep["time"]       = safeValue(json, "time");
                    updatedStep["randomWait"] = safeValue(json, "randomWait");
                    updatedStep["offsetTime"] = safeValue(json, "offsetTime");
                    updatedStep["jumpStepsId"] = safeValue(json, "jumpStepsId");
                    updatedStep["identifyErrorHandle"] = safeValue(json, "identifyErrorHandle");

                    // 替换原来的步骤对象
                    stepsArray[j] = updatedStep;

                    // 更新配置对象的steps数组
                    obj["steps"] = stepsArray;
                    // 更新根数组
                    rootArray[i] = obj;

                    hasStepsId = true;
                    break;
                }
            }
            break;
        }
    }

    // 写回文件
    if (hasConfigId && hasStepsId) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QJsonDocument newDoc(rootArray);
            file.write(newDoc.toJson(QJsonDocument::Indented));
            file.close();
            Logger::log("已成功更新配置: " + json["taskName"].toString());
        } else {
            Logger::log("写入 JSON 文件失败: " + filePath);
        }
    } else if (!hasConfigId) {
        Logger::log("未找到 configId: " + configId);
    } else {
        Logger::log("未找到 stepsId: " + json["stepsId"].toString());
    }
}


QJsonValue safeValue(const QJsonObject &obj, const QString &key) {
    if (!obj.contains(key) || obj.value(key).isNull()) {
        return QJsonValue::Null;
    }
    const QJsonValue val = obj.value(key);
    if (val.isString() && val.toString().isEmpty()) return QJsonValue::Null;
    if (val.isDouble() && qIsNaN(val.toDouble())) return QJsonValue::Null;
    if (val.isBool()) return val; // 布尔类型直接返回
    return val;
}

void saveBase64ImageToFile(QJsonObject &data) {
    QString base64 = data.value("image").toString();
    QString stepsId = data.value("stepsId").toString();

    if (base64.isEmpty() || stepsId.isEmpty()) {
        qWarning() << "image 或 stepsId 为空，无法保存";
        return;
    }

    // 1. 解码 Base64
    QByteArray bytes = QByteArray::fromBase64(base64.toLatin1());

    // 2. 转 QPixmap
    QPixmap pix;
    if (!pix.loadFromData(bytes, "PNG")) {
        qWarning() << "Base64 解码失败";
        return;
    }

    // 3. 确定目录
    QString dirPath = QCoreApplication::applicationDirPath() + "/src/resource/screenshot";
    QDir dir;
    if (!dir.exists(dirPath)) {
        if (!dir.mkpath(dirPath)) {
            qWarning() << "创建目录失败：" << dirPath;
            return;
        }
    }

    // 4. 图片保存路径
    QString filePath = dirPath + "/" + stepsId + ".png";
    if (!pix.save(filePath, "PNG")) {
        qWarning() << "图片保存失败：" << filePath;
        return;
    }

    qDebug() << "图片保存成功：" << filePath;

    // 5. 写回路径到 JSON
    // data["image"] = filePath;
    data["image"] = stepsId + ".png";
}

QString cleanString(const QString &s) {
    QString result = s;
    // 去掉空格、回车、换行、制表符
    result.remove(QRegularExpression("[\\s\\r\\n\\t]+"));
    // 去掉零宽空格等 Unicode 控制字符
    result.remove(QRegularExpression("[\u200B-\u200D\uFEFF]"));
    return result;
}


bool removeConfigById(const QString &filePath, const QString &configId, const QString &stepsId)
{
    QFile file(filePath);
    if (!file.exists()) {
        Logger::log("JSON 文件不存在: " + filePath);
        return false;
    }

    QJsonArray rootArray = m_configArray;

    // 遍历删除
    bool hasConfigId = false;
    bool hasStepsId = false;
    QString imagePathToDelete; // 存储要删除的图片路径

    for (int i = 0; i < rootArray.size(); ++i) {
        QJsonObject rootArrayObj = rootArray[i].toObject();
        if (rootArrayObj["id"].toString() == configId) {
            QJsonArray stepsArray = rootArrayObj["steps"].toArray();
            hasConfigId = true;

            for (int j = 0; j < stepsArray.size(); ++j) {
                QJsonObject stepsObj = stepsArray[j].toObject();
                if (stepsObj["stepsId"].toString() == stepsId) {
                    // 找到对应的stepsId，获取 imagePath
                    if (stepsObj.contains("imagePath") && !stepsObj["imagePath"].isNull()) {
                        imagePathToDelete = stepsObj["imagePath"].toString();
                        QString savePath = ConfigManager::instance().screenshotPath() + imagePathToDelete;
                        // 删除图片文件
                        if (!imagePathToDelete.isEmpty()) {
                            QFile imageFile(savePath);
                            if (imageFile.exists()) {
                                if (imageFile.remove()) {
                                    Logger::log("已删除图片文件: " + savePath);
                                } else {
                                    Logger::log("删除图片文件失败: " + savePath);
                                }
                            } else {
                                Logger::log("图片文件不存在: " + savePath);
                            }
                        }
                    }

                    // 从 stepsArray 中删除该步骤
                    stepsArray.removeAt(j);

                    // 写回到 obj
                    rootArrayObj["steps"] = stepsArray;

                    // 写回到 rootArray
                    rootArray[i] = rootArrayObj;

                    hasStepsId = true;
                    break;
                }
            }
        }

        if (hasConfigId && hasStepsId)
        {
            break;
        }
    }

    if (!hasConfigId) {
        Logger::log("未找到指定 ConfigId: " + configId);
        return false;
    }

    if (!hasStepsId) {
        Logger::log("未找到指定 StepsId: " + stepsId);
        return false;
    }

    // 写回文件
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument newDoc(rootArray);
        file.write(newDoc.toJson(QJsonDocument::Indented));
        file.close();
        Logger::log("已删除 stepsId=" + stepsId + " 的配置");
        return true;
    } else {
        Logger::log("写入 JSON 文件失败: " + filePath);
        return false;
    }
}

//更新方案名称
void updateProgrammeContent(const QString &filePath, const QString &configId, const QString &name)
{
    if (configId.isEmpty())
    {
        Logger::log(QString("当前未选择方案"));
        return;
    }

    QFile file(filePath);
    QJsonArray rootArray = m_configArray;


    bool hasConfigId = false;
    for (int i = 0; i < rootArray.size(); ++i)
    {
        QJsonObject obj = rootArray[i].toObject();
        if (obj["id"].toString() == configId)
        {
            obj["name"] = name;//更新
            hasConfigId = true;

            //回写
            rootArray[i] = obj;

            break;
        }
    }

    // 写回文件
    if (hasConfigId) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QJsonDocument newDoc(rootArray);
            file.write(newDoc.toJson(QJsonDocument::Indented));
            file.close();
            Logger::log("已成功更新配置: " + name);
        } else {
            Logger::log("写入 JSON 文件失败: " + filePath);
        }
    } else {
        Logger::log("未找到 configId: " + configId);
    }
}

bool moveProgramme(const QString &filePath, const QString &configId, const int stepsIndex, const bool status)
{
    QFile file(filePath);
    if (!file.exists()) {
        Logger::log("JSON 文件不存在: " + filePath);
        return false;
    }

    QJsonArray rootArray = m_configArray;

    // 遍历数据后移动
    bool hasConfigId = false;
    bool hasStepsId = false;
    for (int i = 0; i < rootArray.size(); ++i) {
        QJsonObject rootArrayObj = rootArray[i].toObject();
        if (rootArrayObj["id"].toString() == configId) {
            hasConfigId = true;
            QJsonArray stepsArray = rootArrayObj["steps"].toArray();

            // 检查 stepsIndex 是否有效
            if (stepsIndex < 0 || stepsIndex >= stepsArray.size()) {
                Logger::log("stepsIndex 超出范围: " + QString::number(stepsIndex));
                return false;
            }

            hasStepsId = true;

            // 在这里上移或者下移stepsIndex对应的下标数据
            if (status) {
                // status为true表示上移
                if (stepsIndex > 0) {
                    // 交换当前元素和前一个元素
                    QJsonValue currentStep = stepsArray[stepsIndex];
                    QJsonValue prevStep = stepsArray[stepsIndex - 1];

                    stepsArray[stepsIndex] = prevStep;
                    stepsArray[stepsIndex - 1] = currentStep;

                    Logger::log(QString("步骤【 %1 】上移成功").arg(currentStep["taskName"].toString()));
                } else {
                    Logger::log(QString("已经是第一个步骤，无法上移"));
                    return false;
                }
            } else {
                // status为false表示下移
                if (stepsIndex < stepsArray.size() - 1) {
                    // 交换当前元素和后一个元素
                    QJsonValue currentStep = stepsArray[stepsIndex];
                    QJsonValue nextStep = stepsArray[stepsIndex + 1];

                    stepsArray[stepsIndex] = nextStep;
                    stepsArray[stepsIndex + 1] = currentStep;

                    Logger::log(QString("步骤【 %1 】下移成功").arg(currentStep["taskName"].toString()));
                } else {
                    Logger::log(QString("已经是最后一个步骤，无法下移"));
                    return false;
                }
            }

            // 更新 steps 数组
            rootArrayObj["steps"] = stepsArray;
            rootArray[i] = rootArrayObj;
            break;
        }
    }

    if (!hasConfigId) {
        Logger::log("未找到指定 ConfigId: " + configId);
        return false;
    }

    if (!hasStepsId) {
        Logger::log("未找到指定 stepsIndex 的下标数据: " + QString::number(stepsIndex));
        return false;
    }

    // 写回文件
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument newDoc(rootArray);
        file.write(newDoc.toJson(QJsonDocument::Indented));
        file.close();
        // Logger::log(QString("步骤移动成功并保存到文件"));
        return true;
    } else {
        Logger::log("写入 JSON 文件失败: " + filePath);
        return false;
    }
}

void refreshConfig()
{
    QFile file(ConfigManager::instance().configPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::log(QString("无法打开配置文件：" + file.errorString() + "路径:" + ConfigManager::instance().configPath()));
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return;

    QJsonArray arr = doc.array();

    // 保存全局数据
    m_configArray = arr;
}

QJsonArray getConfigJSON()
{
    return m_configArray;
}

QJsonArray getLastConfigJSON()
{
    refreshConfig();
    return m_configArray;
}

//修改当前选择的方案
void commonSetCurrentItem(const QString &id, const QString &taskName)
{
    currentItem.id = id;
    currentItem.taskName = taskName;
}

QMap<QString,QString> getStepsSelect(const QString &configId, const QString &currentStepsId)
{
    QMap<QString,QString> stepsSelect;
    QJsonArray rootArray = m_configArray;

    bool hasConfigId = false;
    bool hasStepsId = false;
    for (int i = 0; i < rootArray.size(); ++i)
    {
        QJsonObject obj = rootArray[i].toObject();
        if (comparesEqual(obj["id"].toString(), configId))
        {
            // 获取steps数组
            QJsonArray stepsArray = obj.value("steps").toArray();
            for (int j = 0; j < stepsArray.size(); ++j)
            {
                QJsonObject stepsObj = stepsArray[j].toObject();
                if (!comparesEqual(stepsObj["stepsId"].toString(), currentStepsId)) //不可以跳转到当前的Step
                {
                    QString key = QString("步骤%1-%2").arg(j + 1).arg(stepsObj["taskName"].toString());
                    stepsSelect.insert(key,stepsObj["stepsId"].toString());
                    hasStepsId = true;
                }
            }
            hasConfigId = true;
            break;
        }
    }

    if (!hasConfigId) {
        Logger::log("未找到指定 ConfigId: " + configId);
        return stepsSelect;
    }

    if (!hasStepsId) {
        Logger::log("未找到指定 StepsId: " + currentStepsId);
        return stepsSelect;
    }

    return stepsSelect;
}