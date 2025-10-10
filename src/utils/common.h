//
// Created by CZY on 2025/9/30.
//

#ifndef COMMON_H
#define COMMON_H
#include <QJsonObject>
#include <QString>

QString getPathByRecognitionImg(const std::string &configId, const std::string &fileName);
void addConfigToJsonFile(const QString &filePath, const QString &name);
bool removeConfigById(const QString &filePath, const QString &idToRemove);
void addConfigToJsonFile(const QString &filePath, const QString &name);
void addConfigToJsonFile(const QString &filePath, const QString &configId, const QJsonObject& json);
void saveBase64ImageToFile(QJsonObject &data);
QString cleanString(const QString &s);
QJsonValue safeValue(const QJsonObject &obj, const QString &key);
bool removeConfigById(const QString &filePath, const QString &configId, const QString &stepsId);
void updateConfigInJsonFile(const QString &filePath, const QString &configId, const QJsonObject& json);
void updateProgrammeContent(const QString &filePath, const QString &configId, const QString &name);
#endif //COMMON_H
