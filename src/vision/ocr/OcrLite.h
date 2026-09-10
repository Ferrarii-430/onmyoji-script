// RapidOCR (RapidAI/RapidOcrOnnx, MIT) OCR 编排器：det -> 透视矫正裁剪 -> cls -> rec。
// 搬运自上游并按本项目精简：去掉 pImpl/JNI/CAPI/CUDA/文件 IO 与调试日志，
// 仅保留进程内 detect(cv::Mat)。textBlocks 坐标已减去 padding 还原到原图坐标系。
#ifndef __OCR_LITE_H__
#define __OCR_LITE_H__

#include <string>
#include "OcrStruct.h"
#include "DbNet.h"
#include "AngleNet.h"
#include "CrnnNet.h"

class OcrLite {
public:
    OcrLite() = default;
    ~OcrLite() = default;

    void setNumThread(int numOfThread);

    bool initModels(const std::string &detPath, const std::string &clsPath,
                    const std::string &recPath, const std::string &keysPath);

    OcrResult detect(const cv::Mat &mat,
                     int padding, int maxSideLen,
                     float boxScoreThresh, float boxThresh, float unClipRatio,
                     bool doAngle, bool mostAngle);

private:
    DbNet dbNet;
    AngleNet angleNet;
    CrnnNet crnnNet;

    std::vector<cv::Mat> getPartImages(cv::Mat &src, std::vector<TextBox> &textBoxes);

    OcrResult detect(cv::Mat &src, cv::Rect &originRect, ScaleParam &scale,
                     float boxScoreThresh, float boxThresh, float unClipRatio,
                     bool doAngle, bool mostAngle);
};

#endif //__OCR_LITE_H__
