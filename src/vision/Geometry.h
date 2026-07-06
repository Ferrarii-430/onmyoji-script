#ifndef VISION_GEOMETRY_H
#define VISION_GEOMETRY_H

#include <opencv2/core/types.hpp>
#include <vector>

namespace vision {

// 计算两个矩形的交并比
float rectIoU(const cv::Rect& a, const cv::Rect& b);

// 非极大值抑制合并重叠矩形
std::vector<cv::Rect> nonMaxSuppression(const std::vector<cv::Rect>& rects, float iouThreshold = 0.3f);

// 在矩形内随机取一点
cv::Point randomPointInRect(const cv::Rect& r);

// 在矩形内随机取一点，按 paddingRatio 排除边缘区域
cv::Point randomPointInRect(const cv::Rect& r, float paddingRatio);

// 在矩形内随机取一点，排除 [excludeStartWidth, excludeEndWidth]（宽度百分比）对应的垂直区域
cv::Point randomPointInRectExcludeWidth(const cv::Rect& r,
                                        double excludeStartWidth, double excludeEndWidth,
                                        int maxAttempts = 10);

} // namespace vision

#endif // VISION_GEOMETRY_H
