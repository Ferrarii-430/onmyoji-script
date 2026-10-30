//
// Created by CZY on 2026/10/20.
//
#include "src/utils/ScenarioHelpers.h"

#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <climits>
#include <utility>

#include "src/vision/Geometry.h"

namespace utils {

cv::Rect boxToRect(const QJsonArray& box)
{
    if (box.size() < 4) return {};
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (const QJsonValue& ptVal : box) {
        const QJsonArray pt = ptVal.toArray();
        if (pt.size() < 2) continue;
        const int x = pt[0].toInt();
        const int y = pt[1].toInt();
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }
    if (minX > maxX || minY > maxY) return {};
    return {minX, minY, maxX - minX, maxY - minY};
}

cv::Point makeClickPoint(const cv::Rect& rect, const bool randomClick)
{
    if (randomClick) {
        return vision::randomPointInRect(rect);
    }
    return {rect.x + rect.width / 2, rect.y + rect.height / 2};
}

QStringList ocrTexts(const QJsonArray& ocrData)
{
    QStringList texts;
    texts.reserve(ocrData.size());
    for (const QJsonValue& val : ocrData) {
        const QString text = val.toObject().value("text").toString().trimmed();
        if (!text.isEmpty()) {
            texts.append(text);
        }
    }
    texts.sort();
    return texts;
}

bool OcrTextTracker::unchangedSinceLast(const QJsonArray& ocrData)
{
    QStringList texts = ocrTexts(ocrData);
    if (texts.isEmpty()) {
        // 识别失败不能当作“列表未变化”，也不覆盖上一次快照
        return false;
    }
    const bool unchanged = texts == lastTexts_;
    lastTexts_ = std::move(texts);
    return unchanged;
}

} // namespace utils
