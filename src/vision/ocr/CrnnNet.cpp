// RapidOCR (RapidAI/RapidOcrOnnx, MIT) 文本识别网络（CRNN）实现。
// 搬运自上游并按本项目精简：去掉 CUDA 与调试图片输出。
#include "CrnnNet.h"
#include "OcrUtils.h"
#include <cstdio>
#include <fstream>
#include <numeric>
#include <stdexcept>

CrnnNet::~CrnnNet() {
    delete session;
    inputNamesPtr.clear();
    outputNamesPtr.clear();
}

void CrnnNet::setNumThread(int numOfThread) {
    numThread = numOfThread;
    sessionOptions.SetInterOpNumThreads(numThread);
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
}

void CrnnNet::initModel(const std::string &pathStr, const std::string &keysPath) {
#ifdef _WIN32
    std::wstring crnnPath = strToWstr(pathStr);
    session = new Ort::Session(env, crnnPath.c_str(), sessionOptions);
#else
    session = new Ort::Session(env, pathStr.c_str(), sessionOptions);
#endif
    inputNamesPtr = getInputNames(session);
    outputNamesPtr = getOutputNames(session);

    //load keys
#ifdef _WIN32
    // 必须用宽路径打开：打包目录含中文（如 F:/工具/...）时，std::ifstream 的窄路径
    // 会按 ANSI 代码页解析而打开失败，keys 为空会让 OCR 只出检测框、文字全部为空
    std::FILE *keysFile = _wfopen(strToWstr(keysPath).c_str(), L"rb");
    if (keysFile == nullptr) {
        throw std::runtime_error("OCR 字典文件打开失败: " + keysPath);
    }
    char buffer[1024];
    while (std::fgets(buffer, sizeof(buffer), keysFile) != nullptr) {
        std::string line(buffer);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        keys.push_back(std::move(line));
    }
    std::fclose(keysFile);
#else
    std::ifstream in(keysPath.c_str());
    std::string line;
    if (in) {
        while (getline(in, line)) {// line中不包括每行的换行符
            keys.push_back(line);
        }
    } else {
        return;
    }
#endif
    keys.insert(keys.begin(), "#");
    keys.emplace_back(" ");
}

template<class ForwardIterator>
inline static size_t argmax(ForwardIterator first, ForwardIterator last) {
    return std::distance(first, std::max_element(first, last));
}

TextLine CrnnNet::scoreToTextLine(const std::vector<float> &outputData, size_t h, size_t w) {
    auto keySize = keys.size();
    auto dataSize = outputData.size();
    std::string strRes;
    std::vector<float> scores;
    size_t lastIndex = 0;
    size_t maxIndex;
    float maxValue;

    for (size_t i = 0; i < h; i++) {
        size_t start = i * w;
        size_t stop = (i + 1) * w;
        if (stop > dataSize - 1) {
            stop = (i + 1) * w - 1;
        }
        maxIndex = int(argmax(&outputData[start], &outputData[stop]));
        maxValue = float(*std::max_element(&outputData[start], &outputData[stop]));
        if (maxIndex > 0 && maxIndex < keySize && (!(i > 0 && maxIndex == lastIndex))) {
            scores.emplace_back(maxValue);
            strRes.append(keys[maxIndex]);
        }
        lastIndex = maxIndex;
    }
    return {strRes, scores};
}

TextLine CrnnNet::getTextLine(const cv::Mat &src) {
    float scale = (float) dstHeight / (float) src.rows;
    int dstWidth = int((float) src.cols * scale);
    cv::Mat srcResize;
    resize(src, srcResize, cv::Size(dstWidth, dstHeight));
    std::vector<float> inputTensorValues = substractMeanNormalize(srcResize, meanValues, normValues);
    std::array<int64_t, 4> inputShape{1, srcResize.channels(), srcResize.rows, srcResize.cols};
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

    return scoreToTextLine(outputData, outputShape[1], outputShape[2]);
}

std::vector<TextLine> CrnnNet::getTextLines(std::vector<cv::Mat> &partImg) {
    int size = partImg.size();
    std::vector<TextLine> textLines(size);
    for (int i = 0; i < size; ++i) {
        textLines[i] = getTextLine(partImg[i]);
    }
    return textLines;
}
