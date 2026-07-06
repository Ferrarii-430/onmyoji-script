#include "src/vision/TemplateMatcher.h"

#include <algorithm>
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

    // 强制禁用所有优化
    cv::setUseOptimized(false);

    cv::Mat gHay, gNeedle;

    try {
        if (haystack.channels() == 3) {
            cv::cvtColor(haystack, gHay, cv::COLOR_BGR2GRAY);
        } else {
            gHay = haystack.clone();
        }

        if (needle.channels() == 3) {
            cv::cvtColor(needle, gNeedle, cv::COLOR_BGR2GRAY);
        } else {
            gNeedle = needle.clone();
        }
    } catch (const cv::Exception& e) {
        Logger::log(QString("[ERROR] 图像转换失败: %1").arg(e.what()));
        return false;
    }

    if (gHay.empty() || gNeedle.empty()) {
        Logger::log(QString("[ERROR] 灰度图像为空"));
        return false;
    }

    std::vector<cv::Rect> candidateRects;
    std::vector<double> candidateScores;

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

            Logger::log(QString("[OpenCV] 缩放=%1 得分=%2 pos=(%3,%4) size=%5x%6")
                .arg(s).arg(maxVal).arg(maxLoc.x).arg(maxLoc.y)
                .arg(resizedNeedle.cols).arg(resizedNeedle.rows));

            if (maxVal >= threshold) {
                cv::Rect r(maxLoc.x, maxLoc.y, resizedNeedle.cols, resizedNeedle.rows);
                candidateRects.push_back(r);
                candidateScores.push_back(maxVal);
            }

        } catch (const std::exception& e) {
            Logger::log(QString("[WARN] 尺度 %1 处理异常: %2").arg(s).arg(e.what()));
            continue;
        }
    }

    if (candidateRects.empty()) {
        Logger::log(QString("[WARN] 没有找到任何匹配 >= %1").arg(threshold));
        return false;
    }

    // NMS：重叠候选框仅保留分数更高的
    std::vector<bool> keep(candidateRects.size(), true);

    for (size_t i = 0; i < candidateRects.size(); ++i) {
        if (!keep[i]) continue;

        for (size_t j = i + 1; j < candidateRects.size(); ++j) {
            if (!keep[j]) continue;

            cv::Rect intersection = candidateRects[i] & candidateRects[j];
            double overlap = intersection.area() / (double)std::min(candidateRects[i].area(), candidateRects[j].area());

            if (overlap > 0.3) {
                if (candidateScores[j] > candidateScores[i]) {
                    keep[i] = false;
                } else {
                    keep[j] = false;
                }
            }
        }
    }

    std::vector<cv::Rect> keptRects;
    std::vector<double> keptScores;

    for (size_t i = 0; i < candidateRects.size(); ++i) {
        if (keep[i]) {
            keptRects.push_back(candidateRects[i]);
            keptScores.push_back(candidateScores[i]);
        }
    }

    if (keptScores.empty()) {
        Logger::log(QString("[WARN] NMS后无候选框"));
        return false;
    }

    auto maxIt = std::max_element(keptScores.begin(), keptScores.end());
    int idx = std::distance(keptScores.begin(), maxIt);

    outRect = keptRects[idx];
    outScore = keptScores[idx];

    Logger::log(QString("[RESULT] bestScore=%1 rect=(%2,%3,%4x%5)")
            .arg(outScore)
            .arg(outRect.x)
            .arg(outRect.y)
            .arg(outRect.width)
            .arg(outRect.height));

    return true;
}

} // namespace vision
