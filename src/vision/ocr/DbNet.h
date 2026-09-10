// RapidOCR (RapidAI/RapidOcrOnnx, MIT) 文本检测网络（DBNet）。
// 搬运自上游并按本项目精简：去掉 CUDA 与 onnxruntime 路径适配。
#ifndef __OCR_DBNET_H__
#define __OCR_DBNET_H__

#include "OcrStruct.h"
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

class DbNet {
public:
    ~DbNet();

    void setNumThread(int numOfThread);

    void initModel(const std::string &pathStr);

    std::vector<TextBox> getTextBoxes(cv::Mat &src, ScaleParam &s, float boxScoreThresh,
                                      float boxThresh, float unClipRatio);

private:
    Ort::Session *session = nullptr;
    Ort::Env env = Ort::Env(ORT_LOGGING_LEVEL_ERROR, "DbNet");
    Ort::SessionOptions sessionOptions = Ort::SessionOptions();
    int numThread = 0;

    std::vector<Ort::AllocatedStringPtr> inputNamesPtr;
    std::vector<Ort::AllocatedStringPtr> outputNamesPtr;

    const float meanValues[3] = {0.485 * 255, 0.456 * 255, 0.406 * 255};
    const float normValues[3] = {1.0 / 0.229 / 255.0, 1.0 / 0.224 / 255.0, 1.0 / 0.225 / 255.0};
};

#endif //__OCR_DBNET_H__
