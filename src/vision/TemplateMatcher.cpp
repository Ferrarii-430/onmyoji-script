#include "src/vision/TemplateMatcher.h"

#include <algorithm>
#include <cmath>
#include <QElapsedTimer>
#include <opencv2/imgproc.hpp>

#include "src/core/Logger.h"

namespace vision {

// 将任意分辨率截图等比例归一化到 targetWidth x targetHeight。
// 缩小用 INTER_AREA（抗锯齿），放大用 INTER_CUBIC（清晰边缘），保留通道数。
cv::Mat normalizeResolution(const cv::Mat& src,
                            int targetWidth,
                            int targetHeight)
{
    if (src.empty()) {
        Logger::log(QString("[ERROR] normalizeResolution 输入为空"));
        return cv::Mat();
    }
    if (src.cols <= 0 || src.rows <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        Logger::log(QString("[ERROR] normalizeResolution 尺寸非法: src=%1x%2 target=%3x%4")
                    .arg(src.cols).arg(src.rows).arg(targetWidth).arg(targetHeight));
        return cv::Mat();
    }

    // 已经是目标尺寸，直接克隆返回（避免无谓缩放）
    if (src.cols == targetWidth && src.rows == targetHeight) {
        return src.clone();
    }

    try {
        cv::Mat dst;
        const double scaleX = targetWidth / static_cast<double>(src.cols);
        const double scaleY = targetHeight / static_cast<double>(src.rows);
        // 任一方向缩小都用 INTER_AREA（对下采样更友好），否则用 INTER_CUBIC
        const int interp = (scaleX < 1.0 || scaleY < 1.0) ? cv::INTER_AREA : cv::INTER_CUBIC;
        cv::resize(src, dst, cv::Size(targetWidth, targetHeight), 0, 0, interp);
        return dst;
    } catch (const cv::Exception& e) {
        Logger::log(QString("[ERROR] normalizeResolution resize 异常: %1").arg(e.what()));
        return cv::Mat();
    }
}

// 基准坐标系下的点转换为实际截图坐标系下的点
cv::Point convertBasePointToScreen(const cv::Point& basePoint,
                                   const cv::Size& realSize,
                                   const cv::Size& baseSize)
{
    if (baseSize.width <= 0 || baseSize.height <= 0) return basePoint;
    const double scaleX = realSize.width / static_cast<double>(baseSize.width);
    const double scaleY = realSize.height / static_cast<double>(baseSize.height);
    return cv::Point(
        static_cast<int>(std::round(basePoint.x * scaleX)),
        static_cast<int>(std::round(basePoint.y * scaleY))
    );
}

// 基准坐标系下的矩形转换为实际截图坐标系下的矩形
cv::Rect convertBaseRectToScreen(const cv::Rect& baseRect,
                                 const cv::Size& realSize,
                                 const cv::Size& baseSize)
{
    if (baseSize.width <= 0 || baseSize.height <= 0) return baseRect;
    const double scaleX = realSize.width / static_cast<double>(baseSize.width);
    const double scaleY = realSize.height / static_cast<double>(baseSize.height);
    return cv::Rect(
        static_cast<int>(std::round(baseRect.x * scaleX)),
        static_cast<int>(std::round(baseRect.y * scaleY)),
        static_cast<int>(std::round(baseRect.width * scaleX)),
        static_cast<int>(std::round(baseRect.height * scaleY))
    );
}

// 固定分辨率模板匹配：haystack 内部归一化到 1920x1080 后单次 matchTemplate。
// outRect 返回【基准坐标系】下的矩形，调用方如需在原图上绘制/点击请自行转换。
bool findTemplate(const cv::Mat& haystack, const cv::Mat& needle,
                  cv::Rect& outRect, double& outScore, double threshold)
{
    outScore = -1;

    if (haystack.empty() || needle.empty()) {
        Logger::log(QString("[ERROR] 输入图像为空"));
        return false;
    }
    if (haystack.cols == 0 || haystack.rows == 0 ||
        needle.cols == 0 || needle.rows == 0) {
        Logger::log(QString("[ERROR] 输入图像尺寸为0"));
        return false;
    }

    const cv::Size baseSize(BASE_MATCH_WIDTH, BASE_MATCH_HEIGHT);

    QElapsedTimer timer;
    timer.start();

    // 1. 截图归一化到基准分辨率（模板默认已是基准分辨率）
    cv::Mat normHay = normalizeResolution(haystack, BASE_MATCH_WIDTH, BASE_MATCH_HEIGHT);
    if (normHay.empty()) {
        Logger::log(QString("[ERROR] 截图归一化失败"));
        return false;
    }

    // 2. 灰度化
    cv::Mat gHay, gNeedle;
    try {
        if (normHay.channels() == 3) {
            cv::cvtColor(normHay, gHay, cv::COLOR_BGR2GRAY);
        } else {
            gHay = normHay;
        }
        if (needle.channels() == 3) {
            cv::cvtColor(needle, gNeedle, cv::COLOR_BGR2GRAY);
        } else {
            gNeedle = needle;
        }
    } catch (const cv::Exception& e) {
        Logger::log(QString("[ERROR] 图像转换失败: %1").arg(e.what()));
        return false;
    }

    if (gHay.empty() || gNeedle.empty()) {
        Logger::log(QString("[ERROR] 灰度图像为空"));
        return false;
    }

    // 模板尺寸不能大于归一化后的截图尺寸
    if (gNeedle.cols > gHay.cols || gNeedle.rows > gHay.rows) {
        Logger::log(QString("[ERROR] 模板尺寸(%1x%2)大于基准截图尺寸(%3x%4)，"
                            "请确认模板是否在 1920x1080 基准下截取")
                    .arg(gNeedle.cols).arg(gNeedle.rows).arg(gHay.cols).arg(gHay.rows));
        return false;
    }

    // 3. 单次模板匹配（不再多尺度扫描）
    cv::Mat result;
    try {
        cv::matchTemplate(gHay, gNeedle, result, cv::TM_CCOEFF_NORMED);
    } catch (const cv::Exception& e) {
        Logger::log(QString("[ERROR] matchTemplate 异常: %1").arg(e.what()));
        return false;
    }
    if (result.empty()) {
        Logger::log(QString("[ERROR] matchTemplate 结果为空"));
        return false;
    }

    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    try {
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    } catch (const cv::Exception& e) {
        Logger::log(QString("[ERROR] minMaxLoc 异常: %1").arg(e.what()));
        return false;
    }

    outScore = maxVal;
    // 返回基准坐标系下的矩形
    outRect = cv::Rect(maxLoc.x, maxLoc.y, gNeedle.cols, gNeedle.rows);

    if (outScore < threshold) {
        Logger::log(QString("[WARN] 没有找到匹配，bestScore=%1 threshold=%2 耗时=%3ms "
                            "(基准 %4x%5, 原始截图 %6x%7)")
                    .arg(outScore, 0, 'f', 4).arg(threshold, 0, 'f', 4)
                    .arg(timer.elapsed())
                    .arg(BASE_MATCH_WIDTH).arg(BASE_MATCH_HEIGHT)
                    .arg(haystack.cols).arg(haystack.rows));
        return false;
    }

    Logger::log(QString("[RESULT] bestScore=%1 baseRect=(%2,%3,%4x%5) 耗时=%6ms "
                        "(基准 %7x%8, 原始截图 %9x%10)")
                .arg(outScore, 0, 'f', 4)
                .arg(outRect.x).arg(outRect.y).arg(outRect.width).arg(outRect.height)
                .arg(timer.elapsed())
                .arg(BASE_MATCH_WIDTH).arg(BASE_MATCH_HEIGHT)
                .arg(haystack.cols).arg(haystack.rows));

    return true;
}

// [兼容旧调用] 内部走归一化单次匹配，返回的 outRect 已转换回截图原始坐标系。
// scaleMin/scaleMax/scaleStep 参数保留但已废弃，不再生效。
bool findTemplateMultiScale(const cv::Mat& haystack, const cv::Mat& needle,
                            cv::Rect& outRect, double& outScore,
                            double scaleMin, double scaleMax,
                            double scaleStep, double threshold)
{
    (void)scaleMin; (void)scaleMax; (void)scaleStep; // 已废弃

    cv::Rect baseRect;
    if (!findTemplate(haystack, needle, baseRect, outScore, threshold)) {
        return false;
    }

    // 基准坐标 -> 截图原始坐标，便于旧调用方直接用于绘制/点击/HSV校验
    outRect = convertBaseRectToScreen(baseRect, haystack.size());
    return true;
}

bool verifyHsvColorMatch(const cv::Mat& haystackColor, const cv::Mat& needleColor,
                         const cv::Rect& roi,
                         double hueTol, double satTol, double valTol)
{
    // 任一彩色图为空时不阻断匹配结果（仅靠形状匹配）
    if (haystackColor.empty() || needleColor.empty()) return true;
    if (roi.width <= 0 || roi.height <= 0) return true;

    // 将 ROI 限制在图像范围内
    cv::Rect clamped = roi & cv::Rect(0, 0, haystackColor.cols, haystackColor.rows);
    if (clamped.width <= 0 || clamped.height <= 0) return false;

    cv::Mat haystackRoi = haystackColor(clamped);

    cv::Mat hHsv, nHsv;
    try {
        cv::cvtColor(haystackRoi, hHsv, cv::COLOR_BGR2HSV);
        cv::cvtColor(needleColor, nHsv, cv::COLOR_BGR2HSV);
    } catch (const cv::Exception& e) {
        Logger::log(QString("[WARN] HSV 转换失败: %1").arg(e.what()));
        return true; // 转换失败时不阻断
    }

    cv::Scalar hMean, hStd, nMean, nStd;
    cv::meanStdDev(hHsv, hMean, hStd);
    cv::meanStdDev(nHsv, nMean, nStd);

    // H: 0~179 (环形), S/V: 0~255
    double dh = std::abs(hMean[0] - nMean[0]);
    dh = std::min(dh, 180.0 - dh); // 处理 hue 环形(如红色横跨0/179)

    double ds = std::abs(hMean[1] - nMean[1]);
    double dv = std::abs(hMean[2] - nMean[2]);

    const bool ok = (dh <= hueTol && ds <= satTol && dv <= valTol);
    Logger::log(QString("[HSV校验] dh=%1 ds=%2 dv=%3 (tol h=%4 s=%5 v=%6) => %7")
                .arg(dh, 0, 'f', 1).arg(ds, 0, 'f', 1).arg(dv, 0, 'f', 1)
                .arg(hueTol, 0, 'f', 1).arg(satTol, 0, 'f', 1).arg(valTol, 0, 'f', 1)
                .arg(ok ? "通过" : "不通过"));

    return ok;
}

} // namespace vision
