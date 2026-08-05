#include "src/vision/TemplateMatcher.h"

#include <algorithm>
#include <cmath>
#include <QElapsedTimer>
#include <opencv2/imgproc.hpp>

#include "src/core/Logger.h"

namespace vision {

// 保持比例缩放到目标尺寸，不足部分填充黑边（letterbox）。
// 适用于 DX11 捕获尺寸 != 基准 1920x1080、且可能存在轻微宽高比误差的场景。
cv::Mat normalizeGameFrame(const cv::Mat& src, ScaleInfo& info,
                           int targetWidth, int targetHeight)
{
    info = ScaleInfo{}; // 重置为默认值

    if (src.empty()) {
        Logger::log(QString("[ERROR] normalizeGameFrame 输入为空"));
        return cv::Mat();
    }
    if (src.cols <= 0 || src.rows <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        Logger::log(QString("[ERROR] normalizeGameFrame 尺寸非法: src=%1x%2 target=%3x%4")
                    .arg(src.cols).arg(src.rows).arg(targetWidth).arg(targetHeight));
        return cv::Mat();
    }

    info.srcWidth = src.cols;
    info.srcHeight = src.rows;

    // 已经是目标尺寸：无需缩放和 padding
    if (src.cols == targetWidth && src.rows == targetHeight) {
        info.scale = 1.0;
        info.offsetX = 0;
        info.offsetY = 0;
        return src.clone();
    }

    try {
        // 取宽高方向各自需要的缩放比例，取较小者保证图像完整放入画布(不变形)
        const double scaleX = targetWidth / static_cast<double>(src.cols);
        const double scaleY = targetHeight / static_cast<double>(src.rows);
        const double scale = std::min(scaleX, scaleY);
        info.scale = scale;

        // 按保持比例缩放后的实际尺寸
        const int scaledW = std::max(1, static_cast<int>(std::round(src.cols * scale)));
        const int scaledH = std::max(1, static_cast<int>(std::round(src.rows * scale)));

        // 居中放置，两侧/上下补黑边
        info.offsetX = (targetWidth - scaledW) / 2;
        info.offsetY = (targetHeight - scaledH) / 2;

        // 缩小时用 INTER_AREA（抗锯齿），放大时用 INTER_CUBIC（清晰边缘）
        const int interp = (scale < 1.0) ? cv::INTER_AREA : cv::INTER_CUBIC;

        cv::Mat scaled;
        cv::resize(src, scaled, cv::Size(scaledW, scaledH), 0, 0, interp);
        if (scaled.empty()) {
            Logger::log(QString("[ERROR] normalizeGameFrame resize 后为空"));
            return cv::Mat();
        }

        // 创建目标画布并填充黑边（与 src 通道数一致），再把缩放后的图像贴到居中位置
        cv::Mat dst(targetHeight, targetWidth, src.type(), cv::Scalar::all(0));
        cv::Rect roi(info.offsetX, info.offsetY, scaledW, scaledH);
        // 防御：确保 ROI 完全位于画布内
        roi &= cv::Rect(0, 0, targetWidth, targetHeight);
        if (roi.width <= 0 || roi.height <= 0) {
            Logger::log(QString("[ERROR] normalizeGameFrame ROI 非法"));
            return cv::Mat();
        }
        scaled(roi & cv::Rect(0, 0, scaled.cols, scaled.rows)).copyTo(dst(roi));

        return dst;
    } catch (const cv::Exception& e) {
        Logger::log(QString("[ERROR] normalizeGameFrame 异常: %1").arg(e.what()));
        return cv::Mat();
    }
}

// 基准坐标系(1920x1080)下的点还原回 DX11 原始捕获坐标系下的点。
// realX = (baseX - offsetX) / scale
cv::Point convertNormalizedPointToCapture(const cv::Point& basePoint, const ScaleInfo& info)
{
    if (!info.valid()) return basePoint;
    const double sx = (basePoint.x - info.offsetX) / info.scale;
    const double sy = (basePoint.y - info.offsetY) / info.scale;
    return cv::Point(static_cast<int>(std::round(sx)),
                     static_cast<int>(std::round(sy)));
}

// 基准坐标系下的矩形还原回 DX11 原始捕获坐标系下的矩形。
cv::Rect convertNormalizedRectToCapture(const cv::Rect& baseRect, const ScaleInfo& info)
{
    if (!info.valid()) return baseRect;
    const double x1 = (baseRect.x - info.offsetX) / info.scale;
    const double y1 = (baseRect.y - info.offsetY) / info.scale;
    const double x2 = (baseRect.x + baseRect.width - info.offsetX) / info.scale;
    const double y2 = (baseRect.y + baseRect.height - info.offsetY) / info.scale;
    const int rx = static_cast<int>(std::round(x1));
    const int ry = static_cast<int>(std::round(y1));
    const int rw = static_cast<int>(std::round(x2 - x1));
    const int rh = static_cast<int>(std::round(y2 - y1));
    // 防御：避免负宽高
    return cv::Rect(rx, ry, std::max(0, rw), std::max(0, rh));
}

// [已废弃] 旧的强制拉伸归一化，保留只为兼容。
cv::Mat normalizeResolution(const cv::Mat& src, int targetWidth, int targetHeight)
{
    if (src.empty()) {
        Logger::log(QString("[ERROR] normalizeResolution 输入为空"));
        return cv::Mat();
    }
    if (src.cols <= 0 || src.rows <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        Logger::log(QString("[ERROR] normalizeResolution 尺寸非法"));
        return cv::Mat();
    }
    if (src.cols == targetWidth && src.rows == targetHeight) {
        return src.clone();
    }
    try {
        cv::Mat dst;
        const double sx = targetWidth / static_cast<double>(src.cols);
        const double sy = targetHeight / static_cast<double>(src.rows);
        const int interp = (sx < 1.0 || sy < 1.0) ? cv::INTER_AREA : cv::INTER_CUBIC;
        cv::resize(src, dst, cv::Size(targetWidth, targetHeight), 0, 0, interp);
        return dst;
    } catch (const cv::Exception& e) {
        Logger::log(QString("[ERROR] normalizeResolution resize 异常: %1").arg(e.what()));
        return cv::Mat();
    }
}

// [已废弃] 旧的简单坐标转换（按宽高独立缩放），仅用于兼容旧 findTemplateMultiScale。
cv::Point convertBasePointToScreen(const cv::Point& basePoint,
                                   const cv::Size& realSize,
                                   const cv::Size& baseSize)
{
    if (baseSize.width <= 0 || baseSize.height <= 0) return basePoint;
    const double sx = realSize.width / static_cast<double>(baseSize.width);
    const double sy = realSize.height / static_cast<double>(baseSize.height);
    return cv::Point(static_cast<int>(std::round(basePoint.x * sx)),
                     static_cast<int>(std::round(basePoint.y * sy)));
}

cv::Rect convertBaseRectToScreen(const cv::Rect& baseRect,
                                 const cv::Size& realSize,
                                 const cv::Size& baseSize)
{
    if (baseSize.width <= 0 || baseSize.height <= 0) return baseRect;
    const double sx = realSize.width / static_cast<double>(baseSize.width);
    const double sy = realSize.height / static_cast<double>(baseSize.height);
    return cv::Rect(static_cast<int>(std::round(baseRect.x * sx)),
                    static_cast<int>(std::round(baseRect.y * sy)),
                    static_cast<int>(std::round(baseRect.width * sx)),
                    static_cast<int>(std::round(baseRect.height * sy)));
}

// 固定分辨率模板匹配：haystack 保持比例 + padding 归一化到 1920x1080，再单次 matchTemplate。
// outRect 返回【基准坐标系】下的矩形；info（若传入）记录本次归一化的 ScaleInfo。
bool findTemplate(const cv::Mat& haystack, const cv::Mat& needle,
                  cv::Rect& outRect, double& outScore, double threshold,
                  ScaleInfo* info)
{
    outScore = -1;
    if (info) *info = ScaleInfo{};

    if (haystack.empty() || needle.empty()) {
        Logger::log(QString("[ERROR] 输入图像为空"));
        return false;
    }
    if (haystack.cols == 0 || haystack.rows == 0 ||
        needle.cols == 0 || needle.rows == 0) {
        Logger::log(QString("[ERROR] 输入图像尺寸为0"));
        return false;
    }

    QElapsedTimer timer;
    timer.start();

    // 1. 截图保持比例 + padding 归一化到基准分辨率
    ScaleInfo localInfo;
    cv::Mat normHay = normalizeGameFrame(haystack, localInfo, BASE_MATCH_WIDTH, BASE_MATCH_HEIGHT);
    if (normHay.empty()) {
        Logger::log(QString("[ERROR] 截图归一化失败"));
        return false;
    }
    if (info) *info = localInfo;

    // 2. 灰度化（保留原图做 HSV 校验用，这里只用归一化后的图）
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
                            "(src=%4x%5 -> base %6x%7, scale=%8, offset=(%9,%10))")
                    .arg(outScore, 0, 'f', 4).arg(threshold, 0, 'f', 4)
                    .arg(timer.elapsed())
                    .arg(localInfo.srcWidth).arg(localInfo.srcHeight)
                    .arg(BASE_MATCH_WIDTH).arg(BASE_MATCH_HEIGHT)
                    .arg(localInfo.scale, 0, 'f', 4)
                    .arg(localInfo.offsetX).arg(localInfo.offsetY));
        return false;
    }

    Logger::log(QString("[RESULT] bestScore=%1 baseRect=(%2,%3,%4x%5) 耗时=%6ms "
                        "(src=%7x%8 -> base %9x%10, scale=%11, offset=(%12,%13))")
                .arg(outScore, 0, 'f', 4)
                .arg(outRect.x).arg(outRect.y).arg(outRect.width).arg(outRect.height)
                .arg(timer.elapsed())
                .arg(localInfo.srcWidth).arg(localInfo.srcHeight)
                .arg(BASE_MATCH_WIDTH).arg(BASE_MATCH_HEIGHT)
                .arg(localInfo.scale, 0, 'f', 4)
                .arg(localInfo.offsetX).arg(localInfo.offsetY));

    return true;
}

// [兼容旧调用] 内部走归一化单次匹配，返回的 outRect 已转换回 DX11 原始捕获坐标系。
// scaleMin/scaleMax/scaleStep 参数保留但已废弃，不再生效。
bool findTemplateMultiScale(const cv::Mat& haystack, const cv::Mat& needle,
                            cv::Rect& outRect, double& outScore,
                            double scaleMin, double scaleMax,
                            double scaleStep, double threshold)
{
    (void)scaleMin; (void)scaleMax; (void)scaleStep; // 已废弃

    cv::Rect baseRect;
    ScaleInfo info;
    if (!findTemplate(haystack, needle, baseRect, outScore, threshold, &info)) {
        return false;
    }

    // 基准坐标 -> DX11 原始捕获坐标，便于旧调用方直接用于绘制/点击/HSV校验
    outRect = convertNormalizedRectToCapture(baseRect, info);
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
