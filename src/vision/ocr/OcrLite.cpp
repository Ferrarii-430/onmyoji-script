// RapidOCR (RapidAI/RapidOcrOnnx, MIT) OCR 编排器实现。
// 搬运自上游并按本项目精简：去掉 pImpl/JNI/CAPI/CUDA/文件 IO 与调试日志。
#include "OcrLite.h"
#include "OcrUtils.h"

void OcrLite::setNumThread(int numOfThread) {
    dbNet.setNumThread(numOfThread);
    angleNet.setNumThread(numOfThread);
    crnnNet.setNumThread(numOfThread);
}

bool OcrLite::initModels(const std::string &detPath, const std::string &clsPath,
                         const std::string &recPath, const std::string &keysPath) {
    dbNet.initModel(detPath);
    angleNet.initModel(clsPath);
    crnnNet.initModel(recPath, keysPath);
    return true;
}

cv::Mat makePadding(cv::Mat &src, const int padding) {
    if (padding <= 0) return src;
    cv::Scalar paddingScalar = {255, 255, 255};
    cv::Mat paddingSrc;
    cv::copyMakeBorder(src, paddingSrc, padding, padding, padding, padding, cv::BORDER_ISOLATED, paddingScalar);
    return paddingSrc;
}

OcrResult OcrLite::detect(const cv::Mat &mat, int padding, int maxSideLen,
                          float boxScoreThresh, float boxThresh, float unClipRatio,
                          bool doAngle, bool mostAngle) {
    int originMaxSide = (std::max)(mat.cols, mat.rows);
    int resize;
    if (maxSideLen <= 0 || maxSideLen > originMaxSide) {
        resize = originMaxSide;
    } else {
        resize = maxSideLen;
    }
    resize += 2 * padding;
    cv::Rect paddingRect(padding, padding, mat.cols, mat.rows);
    cv::Mat paddingSrc = makePadding(const_cast<cv::Mat &>(mat), padding);
    ScaleParam scale = getScaleParam(paddingSrc, resize);
    return detect(paddingSrc, paddingRect, scale,
                  boxScoreThresh, boxThresh, unClipRatio, doAngle, mostAngle);
}

std::vector<cv::Mat> OcrLite::getPartImages(cv::Mat &src, std::vector<TextBox> &textBoxes) {
    std::vector<cv::Mat> partImages;
    for (size_t i = 0; i < textBoxes.size(); ++i) {
        cv::Mat partImg = getRotateCropImage(src, textBoxes[i].boxPoint);
        partImages.emplace_back(partImg);
    }
    return partImages;
}

OcrResult OcrLite::detect(cv::Mat &src, cv::Rect &originRect, ScaleParam &scale,
                          float boxScoreThresh, float boxThresh, float unClipRatio,
                          bool doAngle, bool mostAngle) {
    const double startTime = getCurrentTime();

    //---------- step: dbNet getTextBoxes ----------
    std::vector<TextBox> textBoxes = dbNet.getTextBoxes(src, scale, boxScoreThresh, boxThresh, unClipRatio);
    const double dbNetTime = getCurrentTime() - startTime;

    //---------- getPartImages ----------
    std::vector<cv::Mat> partImages = getPartImages(src, textBoxes);

    //---------- step: angleNet getAngles ----------
    std::vector<Angle> angles = angleNet.getAngles(partImages, doAngle, mostAngle);

    //Rotate partImgs
    for (size_t i = 0; i < partImages.size(); ++i) {
        if (angles[i].index == 1) {
            partImages.at(i) = matRotateClockWise180(partImages[i]);
        }
    }

    //---------- step: crnnNet getTextLine ----------
    std::vector<TextLine> textLines = crnnNet.getTextLines(partImages);

    std::vector<TextBlock> textBlocks;
    for (size_t i = 0; i < textLines.size(); ++i) {
        std::vector<cv::Point> boxPoint = std::vector<cv::Point>(4);
        const int padding = originRect.x;//padding conversion
        boxPoint[0] = cv::Point(textBoxes[i].boxPoint[0].x - padding, textBoxes[i].boxPoint[0].y - padding);
        boxPoint[1] = cv::Point(textBoxes[i].boxPoint[1].x - padding, textBoxes[i].boxPoint[1].y - padding);
        boxPoint[2] = cv::Point(textBoxes[i].boxPoint[2].x - padding, textBoxes[i].boxPoint[2].y - padding);
        boxPoint[3] = cv::Point(textBoxes[i].boxPoint[3].x - padding, textBoxes[i].boxPoint[3].y - padding);
        TextBlock textBlock{boxPoint, textBoxes[i].score, angles[i].index, angles[i].score,
                            angles[i].time, textLines[i].text, textLines[i].charScores, textLines[i].time,
                            angles[i].time + textLines[i].time};
        textBlocks.emplace_back(textBlock);
    }

    const double endTime = getCurrentTime();

    std::string strRes;
    for (auto &textBlock: textBlocks) {
        strRes.append(textBlock.text);
        strRes.append("\n");
    }

    return OcrResult{dbNetTime, textBlocks, cv::Mat(), endTime - startTime, strRes};
}
