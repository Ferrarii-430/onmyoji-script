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

// 根据百分比生成宽度方向的排除区域（垂直条带），[startRatio, endRatio) 为 0~1 的小数
cv::Rect widthExcludeRect(const cv::Rect& r, double startRatio, double endRatio);

// 根据百分比生成高度方向的排除区域（水平条带），[startRatio, endRatio) 为 0~1 的小数
cv::Rect heightExcludeRect(const cv::Rect& r, double startRatio, double endRatio);

// 在矩形内随机取一点，排除指定的多个区域，支持任意排除组合（如左侧 + 上下边框）
cv::Point randomPointInRectExcludeAreas(const cv::Rect& r,
                                        const std::vector<cv::Rect>& excludeAreas,
                                        int maxAttempts = 10);

} // namespace vision

#endif // VISION_GEOMETRY_H
