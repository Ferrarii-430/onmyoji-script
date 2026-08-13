#include "src/vision/TemplateMatcher.h"

#include <algorithm>
#include <cmath>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
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

        // 创建目标画布并填充黑边（与 src 通道数一致），再把缩放后的图像贴到居中位置。
        // 注意：dstRoi 在基准画布坐标系，srcRoi 在 scaled 内部坐标系，两者尺寸必须严格一致，
        // 否则 copyTo 会尝试 resize 目标子矩阵(fixedSize)触发断言。
        cv::Mat dst = cv::Mat::zeros(targetHeight, targetWidth, src.type());
        cv::Rect dstRoi(info.offsetX, info.offsetY, scaledW, scaledH);
        // 防御：确保 ROI 完全位于画布内（极端缩放误差下可能越界一像素）
        dstRoi &= cv::Rect(0, 0, targetWidth, targetHeight);
        if (dstRoi.width <= 0 || dstRoi.height <= 0) {
            Logger::log(QString("[ERROR] normalizeGameFrame ROI 非法"));
            return cv::Mat();
        }
        // scaled 中对应区域：与 dstRoi 同尺寸，起点为 (0,0)
        cv::Rect srcRoi(0, 0, dstRoi.width, dstRoi.height);
        scaled(srcRoi).copyTo(dst(dstRoi));

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

void extractTemplateMask(const cv::Mat& src, cv::Mat& outGray, cv::Mat& outMask)
{
    outGray.release();
    outMask.release();

    if (src.empty()) return;

    try {
        if (src.channels() == 4) {
            std::vector<cv::Mat> ch;
            cv::split(src, ch);
            cv::Mat bgr;
            cv::merge(std::vector<cv::Mat>{ch[0], ch[1], ch[2]}, bgr);
            cv::cvtColor(bgr, outGray, cv::COLOR_BGR2GRAY);
            // alpha > 0 视为有效匹配区域
            cv::threshold(ch[3], outMask, 1, 255, cv::THRESH_BINARY);
        } else if (src.channels() == 3) {
            cv::cvtColor(src, outGray, cv::COLOR_BGR2GRAY);
        } else if (src.channels() == 1) {
            outGray = src.clone();
        } else {
            // 其他通道数：先做灰度化尝试
            cv::cvtColor(src, outGray, cv::COLOR_BGR2GRAY);
        }
    } catch (const cv::Exception& e) {
        Logger::log(QString("[WARN] extractTemplateMask 失败: %1").arg(e.what()));
    }
}

static QString templateSidecarPath(const QString& templatePath)
{
    QFileInfo fi(templatePath);
    // 侧载文件统一放在 screenshot/benchmark/ 子目录下
    const QString benchmarkDir = fi.absolutePath() + "/benchmark";
    return benchmarkDir + "/" + fi.completeBaseName() + ".json";
}

// 把绝对路径转成相对于 applicationDirPath 的相对路径，便于侧载中记录可移植路径。
static QString relativeToAppDir(const QString& absolutePath)
{
    if (absolutePath.isEmpty()) return absolutePath;
    const QString appDir = QCoreApplication::applicationDirPath();
    QString normalizedAppDir = QDir::cleanPath(appDir);
    QString normalizedPath = QDir::cleanPath(absolutePath);
    if (!normalizedAppDir.endsWith('/')) normalizedAppDir += '/';
    if (normalizedPath.startsWith(normalizedAppDir, Qt::CaseInsensitive)) {
        return normalizedPath.mid(normalizedAppDir.length());
    }
    return absolutePath;
}

void saveTemplateCaptureSize(const QString& templatePath, const cv::Size& size)
{
    if (templatePath.isEmpty() || size.width <= 0 || size.height <= 0) return;

    const QString sidecarPath = templateSidecarPath(templatePath);
    const QString sidecarDir = QFileInfo(sidecarPath).absolutePath();
    QDir dir;
    if (!dir.exists(sidecarDir)) {
        if (!dir.mkpath(sidecarDir)) {
            Logger::log(QString("[WARN] 无法创建侧载目录: %1").arg(sidecarDir));
            return;
        }
    }

    QJsonObject obj;
    obj["captureWidth"] = size.width;
    obj["captureHeight"] = size.height;
    obj["path"] = relativeToAppDir(templatePath);

    QFile file(sidecarPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::log(QString("[WARN] 无法保存模板侧载: %1").arg(file.errorString()));
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.close();
    Logger::log(QString("已保存模板侧载: %1 (path=%2)").arg(sidecarPath).arg(obj["path"].toString()));
}

cv::Size loadTemplateCaptureSize(const QString& templatePath)
{
    if (templatePath.isEmpty()) return cv::Size();

    const QString sidecarPath = templateSidecarPath(templatePath);
    QFile file(sidecarPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::log(QString("[WARN] 未找到模板侧载文件: %1").arg(sidecarPath));
        return cv::Size();
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        Logger::log(QString("[WARN] 模板侧载文件格式错误: %1").arg(sidecarPath));
        return cv::Size();
    }

    QJsonObject obj = doc.object();
    int w = obj.value("captureWidth").toInt(-1);
    int h = obj.value("captureHeight").toInt(-1);
    if (w <= 0 || h <= 0) {
        Logger::log(QString("[WARN] 模板侧载分辨率无效(%1x%2): %3").arg(w).arg(h).arg(sidecarPath));
        return cv::Size();
    }

    Logger::log(QString("读取模板侧载分辨率: %1x%2 (%3, path=%4)")
                .arg(w).arg(h).arg(sidecarPath).arg(obj.value("path").toString()));
    return cv::Size(w, h);
}

// 固定分辨率模板匹配：haystack 保持比例 + padding 归一化，
// 再对 needle 做多尺度扫描匹配。outRect 返回【基准坐标系】下的矩形。
bool findTemplate(const cv::Mat& haystack, const cv::Mat& needle,
                  cv::Rect& outRect, double& outScore, double threshold,
                  ScaleInfo* info, const cv::Mat& mask,
                  const cv::Size& templateCaptureSize)
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

    // 1. 决定归一化目标分辨率：
    //    - 若提供了模板截取时的分辨率，则以该分辨率为基准（工作流：在当前窗口分辨率下截图做模板）
    //    - 否则保持原来的 1920x1080 基准
    const bool hasCaptureSize = (templateCaptureSize.width > 0 && templateCaptureSize.height > 0);
    const int targetWidth  = hasCaptureSize ? templateCaptureSize.width  : BASE_MATCH_WIDTH;
    const int targetHeight = hasCaptureSize ? templateCaptureSize.height : BASE_MATCH_HEIGHT;

    ScaleInfo localInfo;
    cv::Mat normHay = normalizeGameFrame(haystack, localInfo, targetWidth, targetHeight);
    if (normHay.empty()) {
        Logger::log(QString("[ERROR] 截图归一化失败"));
        return false;
    }
    if (info) *info = localInfo;

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

    // 3. 对截图做轻度对比度增强，降低亮度变化对 TM_CCOEFF_NORMED 的影响
    try {
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        clahe->apply(gHay, gHay);
    } catch (const cv::Exception& e) {
        Logger::log(QString("[WARN] CLAHE 失败: %1").arg(e.what()));
    }

    // mask 校验：尺寸必须和模板一致，且为 CV_8U
    cv::Mat validMask;
    if (!mask.empty() && mask.size() == needle.size() && mask.type() == CV_8U) {
        validMask = mask;
    }

    // 4. 模板基准化：
    //    - 如果使用了模板截取分辨率，needle 已经处于目标分辨率，无需再缩放
    //    - 否则按老逻辑把 needle 从截图分辨率映射到 1920x1080
    cv::Mat baseNeedle;
    cv::Mat baseMask;
    if (hasCaptureSize) {
        baseNeedle = gNeedle;
        baseMask = validMask;
    } else {
        const double baseScale = localInfo.valid() ? localInfo.scale : 1.0;
        if (std::abs(baseScale - 1.0) < 1e-3) {
            baseNeedle = gNeedle;
            baseMask = validMask;
        } else {
            cv::Size baseSize(static_cast<int>(std::round(gNeedle.cols * baseScale)),
                              static_cast<int>(std::round(gNeedle.rows * baseScale)));
            if (baseSize.width < 1 || baseSize.height < 1 ||
                baseSize.width > gHay.cols || baseSize.height > gHay.rows) {
                Logger::log(QString("[ERROR] 模板映射到基准尺寸后越界: baseNeedle=%1x%2, haystack=%3x%4")
                            .arg(baseSize.width).arg(baseSize.height).arg(gHay.cols).arg(gHay.rows));
                return false;
            }
            const int baseInterp = (baseScale < 1.0) ? cv::INTER_AREA : cv::INTER_CUBIC;
            cv::resize(gNeedle, baseNeedle, baseSize, 0, 0, baseInterp);
            if (!validMask.empty()) {
                cv::resize(validMask, baseMask, baseSize, 0, 0, cv::INTER_NEAREST);
            }
        }
    }

    // 5. 在基准分辨率下做小范围多尺度扫描，覆盖 UI 缩放/截图标示差异
    const double scaleMin = 0.85;
    const double scaleMax = 1.15;
    const double scaleStep = 0.05;

    double bestScore = -1.0;
    cv::Rect bestRect;
    double bestFineScale = 1.0;

    for (double s = scaleMin; s <= scaleMax + 1e-6; s += scaleStep) {
        cv::Size scaledSize(static_cast<int>(std::round(baseNeedle.cols * s)),
                            static_cast<int>(std::round(baseNeedle.rows * s)));
        if (scaledSize.width < 1 || scaledSize.height < 1) continue;
        if (scaledSize.width > gHay.cols || scaledSize.height > gHay.rows) continue;

        cv::Mat scaledNeedle;
        const int needleInterp = (s < 1.0) ? cv::INTER_AREA : cv::INTER_CUBIC;
        cv::resize(baseNeedle, scaledNeedle, scaledSize, 0, 0, needleInterp);
        if (scaledNeedle.empty()) continue;

        cv::Mat scaledMask;
        if (!baseMask.empty()) {
            cv::resize(baseMask, scaledMask, scaledSize, 0, 0, cv::INTER_NEAREST);
        }

        cv::Mat result;
        try {
            if (scaledMask.empty()) {
                cv::matchTemplate(gHay, scaledNeedle, result, cv::TM_CCOEFF_NORMED);
            } else {
                cv::matchTemplate(gHay, scaledNeedle, result, cv::TM_CCOEFF_NORMED, scaledMask);
            }
        } catch (const cv::Exception& e) {
            Logger::log(QString("[WARN] matchTemplate 尺度 %1 异常: %2").arg(s).arg(e.what()));
            continue;
        }
        if (result.empty()) continue;

        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

        if (maxVal > bestScore) {
            bestScore = maxVal;
            // 返回匹配到的实际区域尺寸（基准坐标系）
            bestRect = cv::Rect(maxLoc.x, maxLoc.y, scaledNeedle.cols, scaledNeedle.rows);
            bestFineScale = s;
        }
    }

    // 防御：确保矩形在基准画布范围内
    bestRect &= cv::Rect(0, 0, gHay.cols, gHay.rows);

    outScore = bestScore;
    outRect = bestRect;

    const double loggedBaseScale = hasCaptureSize ? 1.0 : (localInfo.valid() ? localInfo.scale : 1.0);
    const int loggedBaseW = hasCaptureSize ? templateCaptureSize.width  : BASE_MATCH_WIDTH;
    const int loggedBaseH = hasCaptureSize ? templateCaptureSize.height : BASE_MATCH_HEIGHT;

    if (outScore < threshold) {
        Logger::log(QString("[WARN] 没有找到匹配，bestScore=%1 threshold=%2 耗时=%3ms "
                            "(src=%4x%5 -> base %6x%7, scale=%8, offset=(%9,%10), baseNeedleScale=%11, fineScale=%12)")
                    .arg(outScore, 0, 'f', 4).arg(threshold, 0, 'f', 4)
                    .arg(timer.elapsed())
                    .arg(localInfo.srcWidth).arg(localInfo.srcHeight)
                    .arg(loggedBaseW).arg(loggedBaseH)
                    .arg(localInfo.scale, 0, 'f', 4)
                    .arg(localInfo.offsetX).arg(localInfo.offsetY)
                    .arg(loggedBaseScale, 0, 'f', 4)
                    .arg(bestFineScale, 0, 'f', 2));
        return false;
    }

    Logger::log(QString("[RESULT] bestScore=%1 baseRect=(%2,%3,%4x%5) 耗时=%6ms "
                        "(src=%7x%8 -> base %9x%10, scale=%11, offset=(%12,%13), baseNeedleScale=%14, fineScale=%15)")
                .arg(outScore, 0, 'f', 4)
                .arg(outRect.x).arg(outRect.y).arg(outRect.width).arg(outRect.height)
                .arg(timer.elapsed())
                .arg(localInfo.srcWidth).arg(localInfo.srcHeight)
                .arg(loggedBaseW).arg(loggedBaseH)
                .arg(localInfo.scale, 0, 'f', 4)
                .arg(localInfo.offsetX).arg(localInfo.offsetY)
                .arg(loggedBaseScale, 0, 'f', 4)
                .arg(bestFineScale, 0, 'f', 2));

    return true;
}

// 计算两个矩形的 IoU（交并比）
static double rectIoU(const cv::Rect& a, const cv::Rect& b)
{
    const cv::Rect inter = a & b;
    if (inter.width <= 0 || inter.height <= 0) return 0.0;
    const double interArea = inter.width * inter.height;
    const double unionArea = a.width * a.height + b.width * b.height - interArea;
    return unionArea > 0.0 ? interArea / unionArea : 0.0;
}

// 多目标模板匹配：复用 findTemplate 的预处理逻辑，但在每个尺度上收集所有超过阈值的峰值，
// 然后跨尺度做 NMS 去重，返回所有匹配结果。
bool findTemplateAll(const cv::Mat& haystack, const cv::Mat& needle,
                     std::vector<TemplateMatch>& outMatches, double threshold,
                     ScaleInfo* info, const cv::Mat& mask,
                     const cv::Size& templateCaptureSize)
{
    outMatches.clear();
    if (info) *info = ScaleInfo{};

    if (haystack.empty() || needle.empty()) {
        Logger::log(QString("[ERROR] findTemplateAll 输入图像为空"));
        return false;
    }

    QElapsedTimer timer;
    timer.start();

    // 1. 归一化目标分辨率
    const bool hasCaptureSize = (templateCaptureSize.width > 0 && templateCaptureSize.height > 0);
    const int targetWidth  = hasCaptureSize ? templateCaptureSize.width  : BASE_MATCH_WIDTH;
    const int targetHeight = hasCaptureSize ? templateCaptureSize.height : BASE_MATCH_HEIGHT;

    ScaleInfo localInfo;
    cv::Mat normHay = normalizeGameFrame(haystack, localInfo, targetWidth, targetHeight);
    if (normHay.empty()) {
        Logger::log(QString("[ERROR] findTemplateAll 截图归一化失败"));
        return false;
    }
    if (info) *info = localInfo;

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
        Logger::log(QString("[ERROR] findTemplateAll 图像转换失败: %1").arg(e.what()));
        return false;
    }

    // 3. CLAHE 对比度增强
    try {
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        clahe->apply(gHay, gHay);
    } catch (const cv::Exception& e) {
        Logger::log(QString("[WARN] findTemplateAll CLAHE 失败: %1").arg(e.what()));
    }

    // 4. mask 校验
    cv::Mat validMask;
    if (!mask.empty() && mask.size() == needle.size() && mask.type() == CV_8U) {
        validMask = mask;
    }

    // 5. 模板基准化
    cv::Mat baseNeedle;
    cv::Mat baseMask;
    if (hasCaptureSize) {
        baseNeedle = gNeedle;
        baseMask = validMask;
    } else {
        const double baseScale = localInfo.valid() ? localInfo.scale : 1.0;
        if (std::abs(baseScale - 1.0) < 1e-3) {
            baseNeedle = gNeedle;
            baseMask = validMask;
        } else {
            cv::Size baseSize(static_cast<int>(std::round(gNeedle.cols * baseScale)),
                              static_cast<int>(std::round(gNeedle.rows * baseScale)));
            if (baseSize.width < 1 || baseSize.height < 1 ||
                baseSize.width > gHay.cols || baseSize.height > gHay.rows) {
                Logger::log(QString("[ERROR] findTemplateAll 模板映射越界"));
                return false;
            }
            const int baseInterp = (baseScale < 1.0) ? cv::INTER_AREA : cv::INTER_CUBIC;
            cv::resize(gNeedle, baseNeedle, baseSize, 0, 0, baseInterp);
            if (!validMask.empty()) {
                cv::resize(validMask, baseMask, baseSize, 0, 0, cv::INTER_NEAREST);
            }
        }
    }

    // 6. 多尺度扫描，收集所有候选匹配
    const double scaleMin = 0.85;
    const double scaleMax = 1.15;
    const double scaleStep = 0.05;

    struct Candidate {
        cv::Rect rect;
        double score;
        double fineScale;
    };
    std::vector<Candidate> candidates;

    for (double s = scaleMin; s <= scaleMax + 1e-6; s += scaleStep) {
        cv::Size scaledSize(static_cast<int>(std::round(baseNeedle.cols * s)),
                            static_cast<int>(std::round(baseNeedle.rows * s)));
        if (scaledSize.width < 1 || scaledSize.height < 1) continue;
        if (scaledSize.width > gHay.cols || scaledSize.height > gHay.rows) continue;

        cv::Mat scaledNeedle;
        const int needleInterp = (s < 1.0) ? cv::INTER_AREA : cv::INTER_CUBIC;
        cv::resize(baseNeedle, scaledNeedle, scaledSize, 0, 0, needleInterp);
        if (scaledNeedle.empty()) continue;

        cv::Mat scaledMask;
        if (!baseMask.empty()) {
            cv::resize(baseMask, scaledMask, scaledSize, 0, 0, cv::INTER_NEAREST);
        }

        cv::Mat result;
        try {
            if (scaledMask.empty()) {
                cv::matchTemplate(gHay, scaledNeedle, result, cv::TM_CCOEFF_NORMED);
            } else {
                cv::matchTemplate(gHay, scaledNeedle, result, cv::TM_CCOEFF_NORMED, scaledMask);
            }
        } catch (const cv::Exception& e) {
            Logger::log(QString("[WARN] findTemplateAll matchTemplate 尺度 %1 异常: %2").arg(s).arg(e.what()));
            continue;
        }
        if (result.empty()) continue;

        // 在结果矩阵中找出所有超过阈值的局部峰值
        // 方法：阈值化后逐个找最大值，找到后抑制其邻域，再找下一个
        cv::Mat resultCopy = result.clone();
        const int suppressRadius = std::max(scaledSize.width, scaledSize.height) / 2;

        while (true) {
            double maxVal;
            cv::Point maxLoc;
            cv::minMaxLoc(resultCopy, nullptr, &maxVal, nullptr, &maxLoc);
            if (maxVal < threshold) break;

            candidates.push_back(Candidate{
                cv::Rect(maxLoc.x, maxLoc.y, scaledNeedle.cols, scaledNeedle.rows),
                maxVal,
                s
            });

            // 抑制该峰值周围区域，避免同一个目标被重复检测
            const int x0 = std::max(0, maxLoc.x - suppressRadius);
            const int y0 = std::max(0, maxLoc.y - suppressRadius);
            const int x1 = std::min(resultCopy.cols, maxLoc.x + suppressRadius + 1);
            const int y1 = std::min(resultCopy.rows, maxLoc.y + suppressRadius + 1);
            cv::rectangle(resultCopy, cv::Rect(x0, y0, x1 - x0, y1 - y0),
                          cv::Scalar(0.0), cv::FILLED);
        }
    }

    // 7. 按分数降序排序
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

    // 8. NMS：跨尺度去重，IoU > 0.3 的低分候选被抑制
    constexpr double NMS_IOU_THRESHOLD = 0.3;
    std::vector<Candidate> accepted;
    for (const auto& cand : candidates) {
        bool suppressed = false;
        for (const auto& acc : accepted) {
            if (rectIoU(cand.rect, acc.rect) > NMS_IOU_THRESHOLD) {
                suppressed = true;
                break;
            }
        }
        if (!suppressed) {
            accepted.push_back(cand);
        }
    }

    // 9. 裁剪到画布范围并输出
    for (const auto& acc : accepted) {
        cv::Rect r = acc.rect & cv::Rect(0, 0, gHay.cols, gHay.rows);
        if (r.width > 0 && r.height > 0) {
            outMatches.push_back(TemplateMatch{r, acc.score, acc.fineScale});
        }
    }

    Logger::log(QString("[findAll] 候选=%1 去重后=%2 阈值=%3 耗时=%4ms "
                        "(src=%5x%6 -> base %7x%8, scale=%9)")
                .arg(candidates.size()).arg(outMatches.size())
                .arg(threshold, 0, 'f', 2).arg(timer.elapsed())
                .arg(localInfo.srcWidth).arg(localInfo.srcHeight)
                .arg(targetWidth).arg(targetHeight)
                .arg(localInfo.scale, 0, 'f', 4));

    return !outMatches.empty();
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
