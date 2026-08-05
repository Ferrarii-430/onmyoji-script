#ifndef TEMPLATEMATCHER_H
#define TEMPLATEMATCHER_H

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

namespace vision {

// 基准分辨率（所有模板与截图在匹配前都归一化到该尺寸）
constexpr int BASE_MATCH_WIDTH = 1920;
constexpr int BASE_MATCH_HEIGHT = 1080;

// 将任意分辨率截图等比例归一化到 targetWidth x targetHeight。
// 缩小用 INTER_AREA，放大用 INTER_CUBIC，保留通道数。
// 失败返回空 Mat。
cv::Mat normalizeResolution(const cv::Mat& src,
                            int targetWidth = BASE_MATCH_WIDTH,
                            int targetHeight = BASE_MATCH_HEIGHT);

// 基准坐标系(1920x1080)下的点转换为实际截图坐标系下的点。
cv::Point convertBasePointToScreen(const cv::Point& basePoint,
                                   const cv::Size& realSize,
                                   const cv::Size& baseSize = cv::Size(BASE_MATCH_WIDTH, BASE_MATCH_HEIGHT));

// 基准坐标系下的矩形转换为实际截图坐标系下的矩形。
cv::Rect convertBaseRectToScreen(const cv::Rect& baseRect,
                                 const cv::Size& realSize,
                                 const cv::Size& baseSize = cv::Size(BASE_MATCH_WIDTH, BASE_MATCH_HEIGHT));

// 固定分辨率模板匹配：先将 haystack 归一化到 1920x1080，再做单次 matchTemplate。
// needle 需为基准分辨率下保存的模板。outRect 返回的是【基准坐标系】下的矩形。
bool findTemplate(const cv::Mat& haystack, const cv::Mat& needle,
                  cv::Rect& outRect, double& outScore, double threshold);

// [兼容旧调用] 内部走归一化单次匹配流程，scaleMin/scaleMax/scaleStep 已废弃。
// 与 findTemplate 的区别：返回的 outRect 已转换回【截图原始坐标系】，
// 便于旧调用方直接用于绘制 / 点击。
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
