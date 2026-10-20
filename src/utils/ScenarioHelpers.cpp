//
// Created by CZY on 2026/10/20.
//
#include "src/utils/ScenarioHelpers.h"

#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <climits>

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

} // namespace utils
