#include "ScenarioRegistry.h"

#include <QMap>

namespace scenarios {

namespace {

// 函数内静态 Map：注册只发生在各方案 .cpp 的静态初始化期（单线程、先于任何查找），
// 之后只读。QMap 为节点存储，值的地址稳定，findScenario 返回的指针长期有效。
QMap<QString, ScenarioEntry>& registry()
{
    static QMap<QString, ScenarioEntry> instance;
    return instance;
}

} // namespace

void registerScenario(const ScenarioEntry& entry)
{
    if (entry.type.isEmpty() || !entry.execute) {
        return; // 非法注册项直接忽略
    }
    registry().insert(entry.type, entry);
}

const ScenarioEntry* findScenario(const QString& type)
{
    const auto it = registry().constFind(type);
    return it == registry().cend() ? nullptr : &it.value();
}

} // namespace scenarios
