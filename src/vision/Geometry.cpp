#include "src/vision/Geometry.h"

#include <algorithm>
#include <cstdlib>

#include "src/core/Logger.h"

namespace vision {

float rectIoU(const cv::Rect& a, const cv::Rect& b)
{
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width, b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);

    int w = std::max(0, x2 - x1);
    int h = std::max(0, y2 - y1);
    float inter = w * h;
    float unionArea = a.area() + b.area() - inter;
    return unionArea > 0 ? inter / unionArea : 0.f;
}

std::vector<cv::Rect> nonMaxSuppression(const std::vector<cv::Rect>& rects, float iouThreshold)
{
    std::vector<cv::Rect> result;
    std::vector<cv::Rect> sortedRects = rects;

    // 按面积从大到小排序
    std::sort(sortedRects.begin(), sortedRects.end(),
              [](const cv::Rect& a, const cv::Rect& b){ return a.area() > b.area(); });

    std::vector<bool> suppressed(sortedRects.size(), false);

    for (size_t i = 0; i < sortedRects.size(); ++i) {
        if (suppressed[i]) continue;
        result.push_back(sortedRects[i]);
        for (size_t j = i + 1; j < sortedRects.size(); ++j) {
            if (suppressed[j]) continue;
            if (rectIoU(sortedRects[i], sortedRects[j]) > iouThreshold) {
                suppressed[j] = true;
            }
        }
    }
    return result;
}

cv::Point randomPointInRect(const cv::Rect& r)
{
    int x = r.x + rand() % r.width;
    int y = r.y + rand() % r.height;
    return cv::Point(x, y);
}

cv::Point randomPointInRect(const cv::Rect& r, float paddingRatio)
{
    int paddingX = static_cast<int>(r.width * paddingRatio);
    int paddingY = static_cast<int>(r.height * paddingRatio);

    paddingX = std::max(1, paddingX);
    paddingY = std::max(1, paddingY);

    int innerX = r.x + paddingX;
    int innerY = r.y + paddingY;
    int innerWidth = r.width - 2 * paddingX;
    int innerHeight = r.height - 2 * paddingY;

    if (innerWidth <= 0) innerWidth = 1;
    if (innerHeight <= 0) innerHeight = 1;

    int x = innerX + rand() % innerWidth;
    int y = innerY + rand() % innerHeight;

    return cv::Point(x, y);
}

cv::Point randomPointInRectExcludeWidth(const cv::Rect& r,
                                        double excludeStartWidth, double excludeEndWidth,
                                        int maxAttempts)
{
    if (excludeStartWidth >= excludeEndWidth) {
        return randomPointInRect(r);
    }

    int excludeX = r.x + r.width * excludeStartWidth;
    int excludeWidth = r.width * (excludeEndWidth - excludeStartWidth);
    cv::Rect excludeRect(excludeX, r.y, excludeWidth, r.height);

    cv::Rect intersection = r & excludeRect;
    if (intersection.width <= 0 || intersection.height <= 0) {
        return randomPointInRect(r);
    }

    int attempts = 0;
    while (attempts < maxAttempts) {
        cv::Point candidate = randomPointInRect(r);
        if (!excludeRect.contains(candidate)) {
            return candidate;
        }
        attempts++;
    }

    Logger::log(QString("达到最大尝试次数，使用备选中心点"));
    return cv::Point(r.x + r.width / 2, r.y + r.height / 2);
}

} // namespace vision
