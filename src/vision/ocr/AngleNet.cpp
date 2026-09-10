// RapidOCR (RapidAI/RapidOcrOnnx, MIT) 文本方向分类网络（cls）实现。
// 搬运自上游并按本项目精简：去掉 CUDA 与调试图片输出。
#include "AngleNet.h"
#include "OcrUtils.h"
#include <numeric>

AngleNet::~AngleNet() {
    delete session;
    inputNamesPtr.clear();
    outputNamesPtr.clear();
}

void AngleNet::setNumThread(int numOfThread) {
    numThread = numOfThread;
    sessionOptions.SetInterOpNumThreads(numThread);
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
}

void AngleNet::initModel(const std::string &pathStr) {
#ifdef _WIN32
    std::wstring clsPath = strToWstr(pathStr);
    session = new Ort::Session(env, clsPath.c_str(), sessionOptions);
#else
    session = new Ort::Session(env, pathStr.c_str(), sessionOptions);
#endif
    inputNamesPtr = getInputNames(session);
    outputNamesPtr = getOutputNames(session);
}

Angle scoreToAngle(const std::vector<float> &outputData) {
    int maxIndex = 0;
    float maxScore = 0;
    for (size_t i = 0; i < outputData.size(); i++) {
        if (outputData[i] > maxScore) {
            maxScore = outputData[i];
            maxIndex = i;
        }
    }
    return {maxIndex, maxScore};
}

Angle AngleNet::getAngle(cv::Mat &src) {
    std::vector<float> inputTensorValues = substractMeanNormalize(src, meanValues, normValues);
    std::array<int64_t, 4> inputShape{1, src.channels(), src.rows, src.cols};
    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(memoryInfo, inputTensorValues.data(),
                                                             inputTensorValues.size(), inputShape.data(),
                                                             inputShape.size());
    assert(inputTensor.IsTensor());

    std::vector<const char *> inputNames = {inputNamesPtr.data()->get()};
    std::vector<const char *> outputNames = {outputNamesPtr.data()->get()};

    auto outputTensor = session->Run(Ort::RunOptions{nullptr}, inputNames.data(), &inputTensor,
                                     inputNames.size(), outputNames.data(), outputNames.size());
    assert(outputTensor.size() == 1 && outputTensor.front().IsTensor());

    std::vector<int64_t> outputShape = outputTensor[0].GetTensorTypeAndShapeInfo().GetShape();
    int64_t outputCount = std::accumulate(outputShape.begin(), outputShape.end(), 1,
                                          std::multiplies<int64_t>());
    float *floatArray = outputTensor.front().GetTensorMutableData<float>();
    std::vector<float> outputData(floatArray, floatArray + outputCount);

    return scoreToAngle(outputData);
}

std::vector<Angle> AngleNet::getAngles(std::vector<cv::Mat> &partImgs, bool doAngle, bool mostAngle) {
    size_t size = partImgs.size();
    std::vector<Angle> angles(size);
    if (doAngle) {
        for (size_t i = 0; i < size; ++i) {
            cv::Mat angleImg;
            cv::resize(partImgs[i], angleImg, cv::Size(dstWidth, dstHeight));
            Angle angle = getAngle(angleImg);
            angle.time = 0.0;
            angles[i] = angle;
        }
    } else {
        for (size_t i = 0; i < size; ++i) {
            angles[i] = Angle{-1, 0.f};
        }
    }
    //Most Possible AngleIndex
    if (doAngle && mostAngle) {
        auto angleIndexes = getAngleIndexes(angles);
        double sum = std::accumulate(angleIndexes.begin(), angleIndexes.end(), 0.0);
        double halfPercent = angles.size() / 2.0f;
        int mostAngleIndex;
        if (sum < halfPercent) {//all angle set to 0
            mostAngleIndex = 0;
        } else {//all angle set to 1
            mostAngleIndex = 1;
        }
        for (size_t i = 0; i < angles.size(); ++i) {
            Angle angle = angles[i];
            angle.index = mostAngleIndex;
            angles.at(i) = angle;
        }
    }
    return angles;
}
