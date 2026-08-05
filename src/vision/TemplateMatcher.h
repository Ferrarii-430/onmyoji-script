#ifndef TEMPLATEMATCHER_H
#define TEMPLATEMATCHER_H

#include <opencv2/core/mat.hpp>

namespace vision {

// 多尺度模板匹配，返回最优匹配矩形和置信度
bool findTemplateMultiScale(const cv::Mat& haystack, const cv::Mat& needle,
                            cv::Rect& outRect, double& outScore,
                            double scaleMin, double scaleMax,
                            double scaleStep, double threshold);

// 对匹配 ROI 与彩色模板做 HSV 颜色校验，解决灰度匹配下同形状不同色模板
// 均得高分(如0.99)的误判问题。分别计算模板与 ROI 的 HSV 通道均值并比较，
// hue 通道做环形处理。任一彩色图为空时不阻断(返回 true)。
// hueTol/satTol/valTol 为三通道均值容差(H:0~179, S/V:0~255)。
bool verifyHsvColorMatch(const cv::Mat& haystackColor, const cv::Mat& needleColor,
                         const cv::Rect& roi,
                         double hueTol = 25.0, double satTol = 60.0, double valTol = 60.0);

} // namespace vision

#endif // TEMPLATEMATCHER_H
