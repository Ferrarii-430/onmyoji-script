#include "src/vision/TemplateMatcher.h"

#include <algorithm>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <opencv2/imgproc.hpp>

#include "src/core/Logger.h"

namespace vision {

bool findTemplateMultiScale(const cv::Mat& haystack, const cv::Mat& needle,
                            cv::Rect& outRect, double& outScore,
                            double scaleMin, double scaleMax,
                            double scaleStep, double threshold)
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

    cv::Mat gHay, gNeedle;

    try {
        if (haystack.channels() == 3) {
            cv::cvtColor(haystack, gHay, cv::COLOR_BGR2GRAY);
        } else {
            gHay = haystack;
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

    cv::Rect bestRect;
    QElapsedTimer timer;
    timer.start();
    const double earlyExitScore = std::max(threshold, 0.95);

    for (double s = scaleMin; s <= scaleMax + scaleStep/2; s += scaleStep) {
        try {
            int newWidth = std::max(1, static_cast<int>(gNeedle.cols * s));
            int newHeight = std::max(1, static_cast<int>(gNeedle.rows * s));

            if (newWidth < 10 || newHeight < 10) {
                continue;
            }
            if (newWidth > gHay.cols || newHeight > gHay.rows) {
                continue;
            }

            cv::Mat resizedNeedle;

            // 使用INTER_NEAREST，最安全的插值方法
            cv::resize(gNeedle, resizedNeedle, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_NEAREST);

            if (resizedNeedle.empty()) {
                continue;
            }

            cv::Mat result;

            try {
                cv::matchTemplate(gHay, resizedNeedle, result, cv::TM_CCOEFF_NORMED);
            } catch (...) {
                Logger::log(QString("[WARN] matchTemplate 异常，跳过该尺度"));
                continue;
            }

            if (result.empty()) {
                continue;
            }

            double minVal, maxVal;
            cv::Point minLoc, maxLoc;

            try {
                cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
            } catch (...) {
                Logger::log(QString("[WARN] minMaxLoc 异常，跳过该尺度"));
                continue;
            }

            if (maxVal > outScore) {
                outScore = maxVal;
                bestRect = cv::Rect(maxLoc.x, maxLoc.y, resizedNeedle.cols, resizedNeedle.rows);
            }

            if (maxVal >= earlyExitScore) {
                break;
            }

        } catch (const std::exception& e) {
            Logger::log(QString("[WARN] 尺度 %1 处理异常: %2").arg(s).arg(e.what()));
        }

        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }

    if (outScore < threshold) {
        Logger::log(QString("[WARN] 没有找到匹配，bestScore=%1 threshold=%2 耗时=%3ms")
                        .arg(outScore).arg(threshold).arg(timer.elapsed()));
        return false;
    }

    outRect = bestRect;

    Logger::log(QString("[RESULT] bestScore=%1 rect=(%2,%3,%4x%5) 耗时=%6ms")
            .arg(outScore)
            .arg(outRect.x)
            .arg(outRect.y)
            .arg(outRect.width)
            .arg(outRect.height)
            .arg(timer.elapsed()));

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