#include "DetectionCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include "ConfigManager.h"
#include "Logger.h"

namespace {
QString normalizedKey(const QString& value)
{
    QString normalized = value.simplified().toLower();
    return normalized;
}
}

bool DetectionCatalog::initialize(const QString& catalogPath)
{
    if (!catalogPath.isEmpty()) {
        sourcePath_ = catalogPath;
    } else {
        sourcePath_ = ConfigManager::instance().labelCatalogPath();
    }
    return loadFromFile(sourcePath_);
}

bool DetectionCatalog::reload(const QString& catalogPath)
{
    initialized_ = false;
    labelIndex_.clear();
    groupToLabels_.clear();
    sceneToLabels_.clear();
    return initialize(catalogPath.isEmpty() ? sourcePath_ : catalogPath);
}

bool DetectionCatalog::isInitialized()
{
    return initialized_;
}

bool DetectionCatalog::tryGetLabelInfo(const QString& className, LabelInfo& outInfo)
{
    if (!ensureInitialized()) {
        return false;
    }
    const auto key = normalizedKey(className);
    const auto it = labelIndex_.find(key);
    if (it == labelIndex_.end()) {
        return false;
    }
    outInfo = it.value();
    return true;
}

QStringList DetectionCatalog::labelsForGroup(const QString& groupName)
{
    QStringList result;
    if (!ensureInitialized()) {
        return result;
    }
    const auto key = normalizedKey(groupName);
    const auto values = groupToLabels_.values(key);
    for (const auto& label : values) {
        if (!result.contains(label)) {
            result.append(label);
        }
    }
    return result;
}

QStringList DetectionCatalog::labelsForScene(const QString& sceneName)
{
    QStringList result;
    if (!ensureInitialized()) {
        return result;
    }
    const auto key = normalizedKey(sceneName);
    const auto values = sceneToLabels_.values(key);
    for (const auto& label : values) {
        if (!result.contains(label)) {
            result.append(label);
        }
    }
    return result;
}

bool DetectionCatalog::hasLabel(const std::vector<Detection>& detections, const QString& labelName)
{
    if (!ensureInitialized()) {
        return false;
    }
    const auto target = normalizedKey(labelName);
    return std::any_of(detections.begin(), detections.end(), [&](const Detection& det) {
        return normalizedKey(det.className) == target;
    });
}

bool DetectionCatalog::hasAnyLabel(const std::vector<Detection>& detections, const QStringList& labelNames)
{
    if (!ensureInitialized()) {
        return false;
    }
    QSet<QString> targets;
    for (const auto& label : labelNames) {
        targets.insert(normalizedKey(label));
    }
    if (targets.isEmpty()) {
        return false;
    }
    return std::any_of(detections.begin(), detections.end(), [&](const Detection& det) {
        return targets.contains(normalizedKey(det.className));
    });
}

bool DetectionCatalog::hasAnyLabelInScene(const std::vector<Detection>& detections, const QString& sceneName)
{
    if (!ensureInitialized()) {
        return false;
    }
    const auto labels = labelsForScene(sceneName);
    if (labels.isEmpty()) {
        return false;
    }
    return hasAnyLabel(detections, labels);
}

bool DetectionCatalog::ensureInitialized()
{
    if (initialized_) {
        return true;
    }
    return initialize();
}

bool DetectionCatalog::loadFromFile(const QString& catalogPath)
{
    QFile file(catalogPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::log(QString("无法打开标签配置文件: %1").arg(catalogPath));
        return false;
    }

    const auto data = file.readAll();
    file.close();

    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        Logger::log(QString("解析标签配置失败: %1").arg(error.errorString()));
        return false;
    }

    labelIndex_.clear();
    groupToLabels_.clear();
    sceneToLabels_.clear();

    const auto root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!it.value().isArray()) {
            continue;
        }
        const QString groupName = it.key();
        const QString normalizedGroup = normalizedKey(groupName);
        const auto array = it.value().toArray();
        for (const auto& entryValue : array) {
            if (!entryValue.isObject()) {
                continue;
            }
            const auto entry = entryValue.toObject();
            const QString className = entry.value("className").toString();
            if (className.isEmpty()) {
                continue;
            }
            LabelInfo info;
            info.className = className;
            info.function = entry.value("function").toString();
            info.group = groupName;
            if (entry.contains("scenes") && entry.value("scenes").isArray()) {
                const auto scenesArray = entry.value("scenes").toArray();
                for (const auto& sceneValue : scenesArray) {
                    info.scenes.append(sceneValue.toString());
                }
            }

            const QString normalizedLabel = normalizedKey(className);
            labelIndex_.insert(normalizedLabel, info);
            groupToLabels_.insert(normalizedGroup, className);
            for (const auto& scene : info.scenes) {
                sceneToLabels_.insert(normalizedKey(scene), className);
            }
        }
    }

    initialized_ = true;
    Logger::log(QString("已加载%1个标签配置").arg(labelIndex_.size()));
    return true;
}
*** End of File***
