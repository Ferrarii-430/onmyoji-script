#pragma once

#include <QHash>
#include <QMultiHash>
#include <QString>
#include <QStringList>
#include <vector>

#include "YOLODetector.h"

struct LabelInfo {
    QString className;
    QString function;
    QStringList scenes;
    QString group;
};

class DetectionCatalog
{
public:
    static bool initialize(const QString& catalogPath = QString());
    static bool reload(const QString& catalogPath = QString());
    static bool isInitialized();

    static bool tryGetLabelInfo(const QString& className, LabelInfo& outInfo);
    static QStringList labelsForGroup(const QString& groupName);
    static QStringList labelsForScene(const QString& sceneName);

    static bool hasLabel(const std::vector<Detection>& detections, const QString& labelName);
    static bool hasAnyLabel(const std::vector<Detection>& detections, const QStringList& labelNames);
    static bool hasAnyLabelInScene(const std::vector<Detection>& detections, const QString& sceneName);

private:
    static bool ensureInitialized();
    static bool loadFromFile(const QString& catalogPath);

    static QString normalize(const QString& value);

    static inline bool initialized_ = false;
    static inline QString sourcePath_;
    static inline QHash<QString, LabelInfo> labelIndex_;
    static inline QMultiHash<QString, QString> groupToLabels_;
    static inline QMultiHash<QString, QString> sceneToLabels_;
};
