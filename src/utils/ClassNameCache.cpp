#include "ClassNameCache.h"
#include "ConfigManager.h"

// 静态成员初始化
QStringList ClassNameCache::classNames_;
bool ClassNameCache::initialized_ = false;
const QString ClassNameCache::DEFAULT_FILENAME = "classes.txt";

bool ClassNameCache::loadFromFile(const QString& filename)
{
    QString fileToLoad = filename.isEmpty() ? ConfigManager::instance().classesNamePath() : filename;
    qDebug() << fileToLoad;
    QFile file(fileToLoad);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Error: Could not open class names file:" << fileToLoad;
        return false;
    }

    classNames_.clear();
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            classNames_.append(line);
        }
    }

    file.close();
    initialized_ = true;

    qInfo() << "Successfully loaded" << classNames_.size() << "class names from" << fileToLoad;
    return true;
}

bool ClassNameCache::initialize(const QString& filename)
{
    if (initialized_) {
        return true;
    }
    return loadFromFile(filename);
}

QString ClassNameCache::getClassName(int classId)
{
    if (!initialized_ && !loadFromFile()) {
        return QString("unknown_%1").arg(classId);
    }

    if (classId >= 0 && classId < classNames_.size()) {
        return classNames_.at(classId);
    } else {
        return QString("unknown_%1").arg(classId);
    }
}

int ClassNameCache::getClassCount()
{
    if (!initialized_ && !loadFromFile()) {
        return 0;
    }
    return classNames_.size();
}

void ClassNameCache::printAllClasses()
{
    if (!initialized_ && !loadFromFile()) {
        qWarning() << "Cannot print classes: class names not loaded.";
        return;
    }

    qInfo() << "\n=== All Class Names (" << classNames_.size() << "classes) ===";
    for (int i = 0; i < classNames_.size(); ++i) {
        qInfo() << i << ":" << classNames_.at(i);
    }
    qInfo() << "=== End of Class Names ===";
}

void ClassNameCache::clear()
{
    classNames_.clear();
    initialized_ = false;
    qInfo() << "Class name cache cleared.";
}

void ClassNameCache::reload(const QString& filename)
{
    clear();
    loadFromFile(filename);
}

QStringList ClassNameCache::getAllClassNames()
{
    if (!initialized_ && !loadFromFile()) {
        return QStringList();
    }
    return classNames_;
}