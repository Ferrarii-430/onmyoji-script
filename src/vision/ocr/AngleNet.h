// RapidOCR (RapidAI/RapidOcrOnnx, MIT) 文本方向分类网络（cls）。
// 搬运自上游并按本项目精简：去掉 CUDA 与调试图片输出。
#ifndef __OCR_ANGLENET_H__
#define __OCR_ANGLENET_H__

#include "OcrStruct.h"
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

class AngleNet {
public:
    ~AngleNet();

    void setNumThread(int numOfThread);

    void initModel(const std::string &pathStr);

    std::vector<Angle> getAngles(std::vector<cv::Mat> &partImgs, bool doAngle, bool mostAngle);

private:
    Ort::Session *session = nullptr;
    Ort::Env env = Ort::Env(ORT_LOGGING_LEVEL_ERROR, "AngleNet");
    Ort::SessionOptions sessionOptions = Ort::SessionOptions();
    int numThread = 0;

    std::vector<Ort::AllocatedStringPtr> inputNamesPtr;
    std::vector<Ort::AllocatedStringPtr> outputNamesPtr;

    const float meanValues[3] = {127.5, 127.5, 127.5};
    const float normValues[3] = {1.0 / 127.5, 1.0 / 127.5, 1.0 / 127.5};
    const int dstWidth = 192;
    const int dstHeight = 48;

    Angle getAngle(cv::Mat &src);
};

#endif //__OCR_ANGLENET_H__
