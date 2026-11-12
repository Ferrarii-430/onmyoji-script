#include "src/core/WindowBindingStore.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include "src/core/AppPaths.h"
#include "src/platform/ProcessUtils.h"

namespace {

const QString kKeyVersion = QStringLiteral("version");
const QString kKeyBindings = QStringLiteral("bindings");

constexpr int kFileVersion = 1;

} // namespace

QString WindowBinding::tabTitle() const
{
    if (!displayName.isEmpty()) {
        return displayName;
    }
    if (!windowTitle.isEmpty()) {
        return windowTitle;
    }
    return QStringLiteral("实例 %1").arg(slot);
}

QJsonObject WindowBinding::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("slot")] = slot;
    obj[QStringLiteral("displayName")] = displayName;
    obj[QStringLiteral("pid")] = static_cast<qint64>(pid);
    obj[QStringLiteral("imagePath")] = imagePath;
    obj[QStringLiteral("imageName")] = imageName;
    obj[QStringLiteral("windowTitle")] = windowTitle;
    obj[QStringLiteral("boundAt")] = boundAt;
    return obj;
}

WindowBinding WindowBinding::fromJson(const QJsonObject& obj)
{
    WindowBinding item;
    item.slot = obj.value(QStringLiteral("slot")).toInt();
    item.displayName = obj.value(QStringLiteral("displayName")).toString();
    item.pid = static_cast<quint32>(
        obj.value(QStringLiteral("pid")).toVariant().toLongLong());
    item.imagePath = obj.value(QStringLiteral("imagePath")).toString();
    item.imageName = obj.value(QStringLiteral("imageName")).toString();
    item.windowTitle = obj.value(QStringLiteral("windowTitle")).toString();
    item.boundAt = obj.value(QStringLiteral("boundAt")).toVariant().toLongLong();
    return item;
}

QString WindowBindingStore::filePath()
{
    return AppPaths::applicationDirPath() + "/src/resource/windows.json";
}

bool WindowBindingStore::load()
{
    m_items.clear();

    QFile file(filePath());
    if (!file.exists()) {
        // 首次运行没有绑定记录属于正常状态，不视为失败
        m_loaded = true;
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        // 配置损坏时按空列表继续，避免整个程序无法启动
        m_loaded = true;
        return false;
    }

    const QJsonArray array = doc.object().value(kKeyBindings).toArray();
    m_items.reserve(array.size());
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }
        WindowBinding item = WindowBinding::fromJson(value.toObject());
        if (item.slot <= 0) {
            item.slot = m_items.size() + 1;
        }
        m_items.push_back(item);
    }

    m_loaded = true;
    return true;
}

bool WindowBindingStore::save() const
{
    QJsonArray array;
    for (const WindowBinding& item : m_items) {
        array.append(item.toJson());
    }

    QJsonObject root;
    root[kKeyVersion] = kFileVersion;
    root[kKeyBindings] = array;

    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    // QSaveFile 先写临时文件再原子替换，避免写一半崩溃留下半截 JSON
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

int WindowBindingStore::add(const WindowBinding& binding)
{
    WindowBinding item = binding;

    if (item.slot <= 0) {
        int nextSlot = 1;
        for (const WindowBinding& existing : m_items) {
            nextSlot = std::max(nextSlot, existing.slot + 1);
        }
        item.slot = nextSlot;
    }

    if (item.boundAt <= 0) {
        item.boundAt = QDateTime::currentSecsSinceEpoch();
    }

    m_items.push_back(item);
    return m_items.size() - 1;
}

bool WindowBindingStore::removeAt(int index)
{
    if (index < 0 || index >= m_items.size()) {
        return false;
    }
    m_items.removeAt(index);
    return true;
}

bool WindowBindingStore::updateAt(int index, const WindowBinding& binding)
{
    if (index < 0 || index >= m_items.size()) {
        return false;
    }
    m_items[index] = binding;
    return true;
}

void WindowBindingStore::clear()
{
    m_items.clear();
}

void WindowBindingStore::clearSlots()
{
    for (WindowBinding& item : m_items) {
        item.slot = 0;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        m_items[i].slot = i + 1;
    }
}

int WindowBindingStore::indexOfPid(quint32 pid) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

int WindowBindingStore::removeDead()
{
    int removed = 0;
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (!platform::isProcessAlive(static_cast<DWORD>(m_items[i].pid))) {
            m_items.removeAt(i);
            ++removed;
        }
    }
    return removed;
}
