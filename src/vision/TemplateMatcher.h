#ifndef TEMPLATEMATCHER_H
#define TEMPLATEMATCHER_H

#include <opencv2/core/mat.hpp>

namespace vision {

// 多尺度模板匹配，返回最优匹配矩形和置信度
bool findTemplateMultiScale(const cv::Mat& haystack, const cv::Mat& needle,
                            cv::Rect& outRect, double& outScore,
                            double scaleMin, double scaleMax,
                            double scaleStep, double threshold);

} // namespace vision

#endif // TEMPLATEMATCHER_H
