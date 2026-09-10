// RapidOCR (RapidAI/RapidOcrOnnx, MIT) 进程内 OCR 管线的工具函数。
// 搬运自上游并按本项目精简：去掉调试图片落盘与文件路径拼接，
// onnxruntime 头改用本项目已有的平铺 include。
#ifndef __OCR_UTILS_H__
#define __OCR_UTILS_H__

#include <opencv2/core.hpp>
#include "OcrStruct.h"
#include <onnxruntime_cxx_api.h>
#include <numeric>

template<class T>
inline T clamp(T x, T min, T max) {
    if (x > max)
        return max;
    if (x < min)
        return min;
    return x;
}

double getCurrentTime();

std::wstring strToWstr(std::string str);

ScaleParam getScaleParam(cv::Mat &src, const int targetSize);

std::vector<cv::Point2f> getBox(const cv::RotatedRect &rect);

int getThickness(cv::Mat &boxImg);

void drawTextBox(cv::Mat &boxImg, cv::RotatedRect &rect, int thickness);

void drawTextBox(cv::Mat &boxImg, const std::vector<cv::Point> &box, int thickness);

void drawTextBoxes(cv::Mat &boxImg, std::vector<TextBox> &textBoxes, int thickness);

cv::Mat matRotateClockWise180(cv::Mat src);

cv::Mat matRotateClockWise90(cv::Mat src);

cv::Mat getRotateCropImage(const cv::Mat &src, std::vector<cv::Point> box);

cv::Mat adjustTargetImg(cv::Mat &src, int dstWidth, int dstHeight);

std::vector<cv::Point2f> getMinBoxes(const cv::RotatedRect &boxRect, float &maxSideLen);

float boxScoreFast(const std::vector<cv::Point2f> &boxes, const cv::Mat &pred);

cv::RotatedRect unClip(std::vector<cv::Point2f> box, float unClipRatio);

std::vector<float> substractMeanNormalize(cv::Mat &src, const float *meanVals, const float *normVals);

std::vector<int> getAngleIndexes(std::vector<Angle> &angles);

std::vector<Ort::AllocatedStringPtr> getInputNames(Ort::Session *session);

std::vector<Ort::AllocatedStringPtr> getOutputNames(Ort::Session *session);

#endif //__OCR_UTILS_H__
