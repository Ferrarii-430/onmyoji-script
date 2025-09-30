//
// Created by CZY on 2025/9/30.
//

#include "common.h"
#include <qcoreapplication.h>
#include "string"
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

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
    QJsonArray rootArray;

    // 1. 如果文件存在则先读取
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray data = file.readAll();
            file.close();

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(data, &err);
            if (err.error == QJsonParseError::NoError && doc.isArray()) {
                rootArray = doc.array();
            } else {
                Logger::log(QString("解析 JSON 出错，重新初始化为数组"));
            }
        }
    }

    // 2. 构造新对象
    QJsonObject newObj;
    newObj["id"]   = QUuid::createUuid().toString(QUuid::WithoutBraces);  // UUID
    newObj["name"] = name;
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

    QJsonArray rootArray;

    // 1. 读取 JSON
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError && doc.isArray()) {
            rootArray = doc.array();
        } else {
            Logger::log(QString("解析 JSON 出错，文件不是数组"));
            return false;
        }
    }

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