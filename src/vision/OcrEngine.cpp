#include "src/vision/OcrEngine.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "src/core/AppPaths.h"
#include "src/core/Logger.h"
#include "src/vision/ocr/OcrLite.h"

namespace vision {

namespace {

// 推理参数与原 RapidOCR-json.exe 的调用参数保持一致（见旧 QProcess 实现与 cmd.txt）
constexpr int kOcrNumThread = 4;
constexpr float kBoxScoreThresh = 0.5f;
constexpr float kBoxThresh = 0.3f;
constexpr float kUnClipRatio = 1.6f;
constexpr int kMaxSideLen = 3084;
constexpr bool kDoAngle = false;
constexpr bool kMostAngle = false;

// OCR 引擎单例：模型常驻，只加载一次；加载失败抛异常并保持未初始化，下次调用重试
OcrLite& ocrLite()
{
    static OcrLite lite;
    static bool initialized = false;
    if (!initialized) {
        lite.setNumThread(kOcrNumThread);
        const QString modelsPath = AppPaths::instance().rapidOCRModelsPath();
        lite.initModels((modelsPath + AppPaths::instance().rapidOCRDetPathV4()).toStdString(),
                        (modelsPath + AppPaths::instance().rapidOCRClsPathV4()).toStdString(),
                        (modelsPath + AppPaths::instance().rapidOCRRecPathV4()).toStdString(),
                        (modelsPath + AppPaths::instance().rapidOCRKeysPath()).toStdString());
        initialized = true;
        // Logger::log(QString("进程内RapidOCR模型加载完成"));
    }
    return lite;
}

// OcrResult -> 与 exe stdout JSON 同构的 QJsonObject
QJsonObject ocrResultToJson(const OcrResult& result)
{
    QJsonArray dataArray;
    for (const auto& block : result.textBlocks) {
        QJsonObject item;
        QJsonArray box;
        for (const auto& point : block.boxPoint) {
            QJsonArray p;
            p.append(point.x);
            p.append(point.y);
            box.append(p);
        }
        item["box"] = box;
        item["score"] = static_cast<double>(block.boxScore);
        item["text"] = QString::fromStdString(block.text);
        dataArray.append(item);
    }
    QJsonObject root;
    root["data"] = dataArray;
    return root;
}

} // namespace

bool initInProcessOcr()
{
    try {
        ocrLite();
        return true;
    } catch (const Ort::Exception& e) {
        qWarning() << "进程内RapidOCR模型加载失败:" << e.what();
        return false;
    } catch (const std::exception& e) {
        qWarning() << "进程内RapidOCR初始化异常:" << e.what();
        return false;
    }
}

QJsonObject runRapidOCR(const cv::Mat& image, int padding)
{
    if (image.empty()) {
        return QJsonObject();
    }

    try {
        const OcrResult result = ocrLite().detect(image, padding, kMaxSideLen,
                                                   kBoxScoreThresh, kBoxThresh, kUnClipRatio,
                                                   kDoAngle, kMostAngle);
        return ocrResultToJson(result);
    } catch (const Ort::Exception& e) {
        qWarning() << "OCR推理失败:" << e.what();
        return QJsonObject();
    } catch (const std::exception& e) {
        qWarning() << "OCR推理异常:" << e.what();
        return QJsonObject();
    }
}

} // namespace vision
