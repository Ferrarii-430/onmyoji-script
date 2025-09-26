//
// Created by CZY on 2025/9/25.
//

#ifndef EXECUTIONSTEPS_H
#define EXECUTIONSTEPS_H
#include <string>
#include <windows.h>
#include <opencv2/core/mat.hpp>

class ExecutionSteps
{
public:
    // 获取唯一实例
    static ExecutionSteps& getInstance();
    std::string target = "onmyoji.exe";
    HWND hwnd = nullptr;
    bool checkHWNDHandle();
    DWORD findPidByProcessName(const std::string& procName);
    HWND findWindowByPid(DWORD pid);
    void opencvRecognizesAndClick(const std::string& templPath);
    bool captureWindowToMat(HWND hwnd, cv::Mat& outBGR);
    bool findTemplateMultiScaleInMatNMS(const cv::Mat& haystack, const cv::Mat& needle, cv::Rect& outRect,
                                        double& outScore,
                                        double scaleMin, double scaleMax, double scaleStep, double threshold);
    std::vector<cv::Rect> nonMaxSuppression(const std::vector<cv::Rect>& rects, float iouThreshold);
    float rectIoU(const cv::Rect& a, const cv::Rect& b);
    cv::Point getRandomPointInRect(const cv::Rect& r);
    void clickInWindow(const cv::Point& ptClient);

private:
    // 构造函数私有化，防止外部创建
    ExecutionSteps() = default;

    // 禁用拷贝和赋值
    ExecutionSteps(const ExecutionSteps&) = delete;
    ExecutionSteps& operator=(const ExecutionSteps&) = delete;
};

#endif // EXECUTIONSTEPS_H
