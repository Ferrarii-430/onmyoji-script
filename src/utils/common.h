//
// Created by CZY on 2025/9/30.
//

#ifndef COMMON_H
#define COMMON_H
#include <QString>

QString getPathByRecognitionImg(const std::string &configId, const std::string &fileName);
void addConfigToJsonFile(const QString &filePath, const QString &name);
bool removeConfigById(const QString &filePath, const QString &idToRemove);

#endif //COMMON_H
