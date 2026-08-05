#ifndef TEMPLATEMATCHER_H
#define TEMPLATEMATCHER_H

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

namespace vision {

// 基准分辨率（所有模板与截图在匹配前都归一化到该尺寸）
constexpr int BASE_MATCH_WIDTH = 1920;
constexpr int BASE_MATCH_HEIGHT = 1080;

// 归一化过程的缩放信息：保持比例缩放 + 黑边 padding 后的坐标映射关系。
// 通过该结构可以把基准坐标系(1920x1080)的点/矩形精确还原回 DX11 原始捕获坐标。
struct ScaleInfo {
    double scale = 1.0;   // 保持比例缩放比例（src -> base 使用的统一缩放系数）
    int offsetX = 0;      // 缩放后图像在基准画布中的左上角 X 偏移（黑边宽度）
    int offsetY = 0;      // 缩放后图像在基准画布中的左上角 Y 偏移（黑边高度）
    int srcWidth = 0;     // DX11 原始捕获宽度
    int srcHeight = 0;    // DX11 原始捕获高度

    bool valid() const { return scale > 0.0 && srcWidth > 0 && srcHeight > 0; }
};

// 将任意 DX11 捕获截图保持比例缩放到 1920x1080，不足部分填充黑边。
// info 记录 scale/offset/src 尺寸，用于后续把基准坐标精确还原回原始捕获坐标。
// 缩小用 INTER_AREA，放大用 INTER_CUBIC，保留通道数。失败返回空 Mat。
cv::Mat normalizeGameFrame(const cv::Mat& src, ScaleInfo& info,
                           int targetWidth = BASE_MATCH_WIDTH,
                           int targetHeight = BASE_MATCH_HEIGHT);

// 基准坐标系(1920x1080)下的点还原回 DX11 原始捕获坐标系下的点。
// 公式: realX = (baseX - offsetX) / scale
cv::Point convertNormalizedPointToCapture(const cv::Point& basePoint, const ScaleInfo& info);

// 基准坐标系下的矩形还原回 DX11 原始捕获坐标系下的矩形。
cv::Rect convertNormalizedRectToCapture(const cv::Rect& baseRect, const ScaleInfo& info);

// [已废弃] 旧的强制拉伸归一化，保留只为兼容，新代码请用 normalizeGameFrame。
cv::Mat normalizeResolution(const cv::Mat& src,
                            int targetWidth = BASE_MATCH_WIDTH,
                            int targetHeight = BASE_MATCH_HEIGHT);

// [已废弃] 旧的简单坐标转换（按宽高独立缩放），仅用于兼容旧 findTemplateMultiScale。
cv::Point convertBasePointToScreen(const cv::Point& basePoint,
                                   const cv::Size& realSize,
                                   const cv::Size& baseSize = cv::Size(BASE_MATCH_WIDTH, BASE_MATCH_HEIGHT));
cv::Rect convertBaseRectToScreen(const cv::Rect& baseRect,
                                 const cv::Size& realSize,
                                 const cv::Size& baseSize = cv::Size(BASE_MATCH_WIDTH, BASE_MATCH_HEIGHT));

// 固定分辨率模板匹配：先将 haystack 保持比例 + padding 归一化到 1920x1080，
// 再做单次 matchTemplate。outRect 返回【基准坐标系】下的矩形。
// 若 info 不为 nullptr，会写入本次匹配的 ScaleInfo 供调用方还原坐标使用。
bool findTemplate(const cv::Mat& haystack, const cv::Mat& needle,
                  cv::Rect& outRect, double& outScore, double threshold,
                  ScaleInfo* info = nullptr);

// [兼容旧调用] 内部走归一化单次匹配流程，scaleMin/scaleMax/scaleStep 已废弃。
// 返回的 outRect 已转换回【DX11 原始捕获坐标系】，便于旧调用方直接用于绘制/点击。
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
