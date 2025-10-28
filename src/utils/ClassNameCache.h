#ifndef CLASSNAMECACHE_H
#define CLASSNAMECACHE_H

#include <QString>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QDebug>

class ClassNameCache
{
private:
    static QStringList classNames_;
    static bool initialized_;
    static const QString DEFAULT_FILENAME;

    static bool loadFromFile(const QString& filename = DEFAULT_FILENAME);

public:
    // 禁用实例化
    ClassNameCache() = delete;

    static bool initialize(const QString& filename = DEFAULT_FILENAME);
    static QString getClassName(int classId);
    static int getClassCount();
    static void printAllClasses();
    static void clear();
    static void reload(const QString& filename = DEFAULT_FILENAME);

    // 新增：获取所有类别名称
    static QStringList getAllClassNames();
};

#endif // CLASSNAMECACHE_H
