#include "src/engine/ScriptActions.h"

#include <windows.h>
#include <cmath>
#include <algorithm>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonObject>
#include <QTemporaryFile>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "src/core/AppPaths.h"
#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/game/GameWindow.h"
#include "src/game/capture/CaptureService.h"
#include "src/vision/ClassNameCache.h"
#include "src/vision/Geometry.h"
#include "src/vision/OcrEngine.h"
#include "src/vision/TemplateMatcher.h"

namespace {

struct CachedTemplate {
    qint64 fileSize = 0;
    qint64 modifiedTime = 0;
    cv::Mat image;
};

cv::Mat loadTemplateGrayscale(const QString& templatePath, bool& cacheHit)
{
    static QHash<QString, CachedTemplate> cache;

    const QFileInfo fileInfo(templatePath);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    const QString cacheKey = canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
    const qint64 fileSize = fileInfo.size();
    const qint64 modifiedTime = fileInfo.lastModified().toMSecsSinceEpoch();

    const auto cached = cache.constFind(cacheKey);
    if (cached != cache.cend()
        && cached->fileSize == fileSize
        && cached->modifiedTime == modifiedTime
        && !cached->image.empty()) {
        cacheHit = true;
        return cached->image;
    }

    cacheHit = false;
    cv::Mat image = cv::imread(templatePath.toStdString(), cv::IMREAD_GRAYSCALE);
    if (image.empty()) {
        return image;
    }

    if (cache.size() >= 64 && !cache.contains(cacheKey)) {
        cache.clear();
    }
    cache.insert(cacheKey, CachedTemplate{fileSize, modifiedTime, image});
    return image;
}

// 在结果图上绘制点击标记：白色描边 + 红点 + 十字线
void drawClickMarker(cv::Mat& img, const cv::Point& clickPt)
{
    constexpr int radius = 7;
    cv::circle(img, clickPt, radius + 2, cv::Scalar(255, 255, 255), 2);
    cv::circle(img, clickPt, radius, cv::Scalar(0, 0, 255), -1);
    cv::drawMarker(img, clickPt, cv::Scalar(255, 255, 255),
                   cv::MARKER_CROSS, radius * 2, 1);
}

// 按百分比(0~100)计算 OCR 识别区域并在需要裁剪时保存裁剪图。
// 返回换算回全图坐标所需的 roiRect（未裁剪时为整张图，原点为 0）。
// roiPercent 宽/高<=0、区域无效、或区域即整张图时识别整张图并令 ocrImagePath 为空。
cv::Rect computeOcrRoi(const cv::Mat& winImg, const QRectF& roiPercent,
                       const QString& saveDir, QString& ocrImagePath)
{
    ocrImagePath.clear();
    const cv::Rect fullRect(0, 0, winImg.cols, winImg.rows);
    if (roiPercent.width() <= 0.0 || roiPercent.height() <= 0.0) {
        return fullRect;
    }

    const int x = static_cast<int>(std::round(winImg.cols * roiPercent.x() / 100.0));
    const int y = static_cast<int>(std::round(winImg.rows * roiPercent.y() / 100.0));
    const int w = static_cast<int>(std::round(winImg.cols * roiPercent.width() / 100.0));
    const int h = static_cast<int>(std::round(winImg.rows * roiPercent.height() / 100.0));
    const cv::Rect roiRect = cv::Rect(x, y, w, h) & fullRect;

    if (roiRect.width <= 0 || roiRect.height <= 0) {
        Logger::log(QString("OCR识别区域无效: (%1%%,%2%%,%3%%,%4%%)，改为识别整张图片")
                        .arg(roiPercent.x()).arg(roiPercent.y())
                        .arg(roiPercent.width()).arg(roiPercent.height()));
        return fullRect;
    }
    if (roiRect == fullRect) {
        return fullRect; // 区域即整张图，无需裁剪
    }

    QDir dir(saveDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    ocrImagePath = saveDir + "ocr_roi_capture.png";
    cv::imwrite(ocrImagePath.toStdString(), winImg(roiRect));
    Logger::log(QString("OCR识别区域: 百分比(%1%%,%2%%,%3%%,%4%%) -> 像素(%5,%6,%7x%8)")
                    .arg(roiPercent.x()).arg(roiPercent.y())
                    .arg(roiPercent.width()).arg(roiPercent.height())
                    .arg(roiRect.x).arg(roiRect.y)
                    .arg(roiRect.width).arg(roiRect.height));
    return roiRect;
}

} // namespace

ScriptActions& ScriptActions::instance()
{
    static ScriptActions instance;
    return instance;
}

void ScriptActions::processAndShowImage(const QString& imagePath)
{
    qDebug() << imagePath;
    core::waitWithEventProcessing(500);
    emit requestShowImage(imagePath);
}

QString ScriptActions::opencvRecognizesAndClickByBase64(const QString& base64, const double threshold, const bool randomClick)
{
    if (base64.isEmpty()) {
        return "错误: base64 图像数据为空";
    }

    QByteArray imageData = QByteArray::fromBase64(base64.toUtf8());
    if (imageData.isEmpty()) {
        return "错误: base64 数据解码失败";
    }

    QTemporaryFile tempFile;
    tempFile.setFileTemplate("recognizes_template.png");
    if (!tempFile.open()) {
        return "错误: 无法创建临时文件";
    }

    if (tempFile.write(imageData) == -1) {
        tempFile.close();
        return "错误: 无法写入临时文件";
    }
    tempFile.close(); // 关闭文件以确保数据刷新

    return opencvRecognizesAndClick(tempFile.fileName(), threshold, randomClick);
}

QString ScriptActions::opencvRecognizesAndClick(const QString& templPath, const double threshold, const bool randomClick)
{
    cv::Mat winImg = capture::captureGameWindow();
    if (winImg.empty())
    {
        return nullptr;
    }

    //加载模板文件
    QString tempSavePath = resolveTemplatePath(templPath, AppPaths::instance().screenshotPath());
    bool templateCacheHit = false;
    cv::Mat templ = loadTemplateGrayscale(tempSavePath, templateCacheHit);
    if (templ.empty()) {
        Logger::log("模板图片加载失败: " + tempSavePath);
        return nullptr;
    }
    if (!templateCacheHit) {
        Logger::log("已加载模板图片: " + tempSavePath);
    }

    // 在窗口图像中查找模板
    double score = 0.0;
    cv::Rect matchRect;
    bool found = vision::findTemplateMultiScale(winImg, templ, matchRect, score,
                                                0.4, 0.9, 0.1, threshold);

    if (!found) {
        Logger::log(QString("未找到匹配区域! score=%1").arg(score));
        return nullptr;
    }

    cv::Point clickPt;
    if (randomClick) {
        clickPt = vision::randomPointInRect(matchRect);
    } else {
        clickPt = cv::Point(matchRect.x + matchRect.width / 2,
                           matchRect.y + matchRect.height / 2);
    }

    // 将物理坐标转换为逻辑坐标：除以缩放因子
    double scaleFactor = dpiScalingFactor();
    clickPt.x = static_cast<int>(clickPt.x / scaleFactor);
    clickPt.y = static_cast<int>(clickPt.y / scaleFactor);

    // 保存带识别框和点击位置的图片
    cv::Mat resultImg = winImg.clone();
    cv::rectangle(resultImg, matchRect, cv::Scalar(0, 255, 0), 2);
    drawClickMarker(resultImg, clickPt);

    QString savePath = AppPaths::instance().matchResultPath();
    cv::imwrite(savePath.toStdString(), resultImg);

    Logger::log(QString("转换后点击点: (%1, %2)").arg(clickPt.x).arg(clickPt.y));
    GameWindow::instance().clickInWindow(clickPt);
    processAndShowImage(savePath);

    return savePath;
}

QJsonArray ScriptActions::ocrRecognizes(const QRectF& roiPercent)
{
    cv::Mat winImg = capture::captureGameWindow();

    if (winImg.empty())
    {
        return QJsonArray();
    }

    const QString saveDir = AppPaths::instance().thumbnailPath();

    QString ocrImagePath;
    const cv::Rect roiRect = computeOcrRoi(winImg, roiPercent, saveDir, ocrImagePath);
    const bool useRoi = !ocrImagePath.isEmpty();

    QJsonObject result = vision::runRapidOCR(ocrImagePath);
    QJsonArray dataArray = result["data"].toArray();

    if (useRoi) {
        for (int i = 0; i < dataArray.size(); ++i) {
            QJsonObject item = dataArray[i].toObject();
            QJsonArray box = item["box"].toArray();
            if (box.size() != 4) {
                continue;
            }

            QJsonArray adjustedBox;
            for (const QJsonValue& pointValue : box) {
                QJsonArray point = pointValue.toArray();
                if (point.size() < 2) {
                    adjustedBox.append(pointValue);
                    continue;
                }

                QJsonArray adjustedPoint;
                adjustedPoint.append(point[0].toInt() + roiRect.x);
                adjustedPoint.append(point[1].toInt() + roiRect.y);
                for (int j = 2; j < point.size(); ++j) {
                    adjustedPoint.append(point[j]);
                }
                adjustedBox.append(adjustedPoint);
            }

            item["box"] = adjustedBox;
            dataArray[i] = item;
        }
    }

    return dataArray;
}

QString ScriptActions::ocrClickMatchedItem(const cv::Mat& winImg, const QJsonObject& item, const cv::Rect& roiRect,
                                           const bool useRoi, const bool randomClick, const QString& saveDir)
{
    QJsonArray box = item["box"].toArray();
    if (box.size() != 4) {
        qWarning() << "box数组大小不正确，期望4个点，实际:" << box.size();
        return QString();
    }

    // 提取四个点的坐标
    QJsonArray point1 = box[0].toArray();
    QJsonArray point2 = box[1].toArray();
    QJsonArray point3 = box[2].toArray();
    QJsonArray point4 = box[3].toArray();

    int x1 = point1[0].toInt();
    int y1 = point1[1].toInt();
    int x2 = point2[0].toInt();
    int y2 = point2[1].toInt();
    int x3 = point3[0].toInt();
    int y3 = point3[1].toInt();
    int x4 = point4[0].toInt();
    int y4 = point4[1].toInt();

    // 计算矩形的最小外接矩形
    int minX = std::min({x1, x2, x3, x4});
    int minY = std::min({y1, y2, y3, y4});
    int maxX = std::max({x1, x2, x3, x4});
    int maxY = std::max({y1, y2, y3, y4});

    // OCR 坐标相对于裁剪区域，换算回全图坐标
    cv::Rect matchRect(minX + roiRect.x, minY + roiRect.y, maxX - minX, maxY - minY);

    cv::Point clickPt;
    if (randomClick) {
        clickPt = vision::randomPointInRect(matchRect);
    } else {
        clickPt = cv::Point(matchRect.x + matchRect.width / 2,
                           matchRect.y + matchRect.height / 2);
    }

    // 保存带识别框和点击位置的图片
    cv::Mat resultImg = winImg.clone();
    if (useRoi) {
        cv::rectangle(resultImg, roiRect, cv::Scalar(255, 128, 0), 2);
    }
    cv::rectangle(resultImg, matchRect, cv::Scalar(0, 255, 0), 2);
    drawClickMarker(resultImg, clickPt);

    QDir dir(saveDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString savePath = AppPaths::instance().matchResultPath();
    cv::imwrite(savePath.toStdString(), resultImg);

    GameWindow::instance().clickInWindow(clickPt);
    processAndShowImage(savePath);
    return savePath;
}

QString ScriptActions::ocrRecognizesAndClick(const QString& ocrText, const double threshold, const bool randomClick,
                                             const QRectF& roiPercent)
{
    cv::Mat winImg = capture::captureGameWindow();
    bool hasOcrText = false;

    if (winImg.empty())
    {
        return nullptr;
    }

    const QString saveDir = AppPaths::instance().thumbnailPath();

    QString ocrImagePath;
    const cv::Rect roiRect = computeOcrRoi(winImg, roiPercent, saveDir, ocrImagePath);
    const bool useRoi = !ocrImagePath.isEmpty();

    QJsonObject result = vision::runRapidOCR(ocrImagePath);
    QString savePath;

    if (!result.isEmpty()) {
        QJsonArray dataArray = result["data"].toArray();

        for (int i = 0; i < dataArray.size(); ++i) {
            QJsonObject item = dataArray[i].toObject();
            QString text = item["text"].toString();
            double score = item["score"].toDouble();

            if (comparesEqual(text,ocrText))
            {
                hasOcrText = true;
                if (score >= threshold)
                {
                    savePath = ocrClickMatchedItem(winImg, item, roiRect, useRoi, randomClick, saveDir);
                    if (!savePath.isEmpty()) {
                        break;
                    }
                }else
                {
                    Logger::log(QString("[OCR] 已识别到:" + text + " 但分数过低"));
                }
            }
        }

        if (!hasOcrText)
        {
            Logger::log(QString("[OCR] 未识别到文字：" + ocrText));
            qDebug() << result;
        }
    }

    return savePath;
}

QString ScriptActions::ocrRecognizesAndClickAny(const QStringList& ocrTexts, const double threshold,
                                                const bool randomClick, const QRectF& roiPercent)
{
    cv::Mat winImg = capture::captureGameWindow();

    if (winImg.empty())
    {
        return QString();
    }

    const QString saveDir = AppPaths::instance().thumbnailPath();

    QString ocrImagePath;
    const cv::Rect roiRect = computeOcrRoi(winImg, roiPercent, saveDir, ocrImagePath);
    const bool useRoi = !ocrImagePath.isEmpty();

    // 只做一次 OCR，多个文字按填入顺序作为优先级，命中首个即点击并返回命中的文字
    QJsonObject result = vision::runRapidOCR(ocrImagePath);
    if (result.isEmpty()) {
        return QString();
    }

    QJsonArray dataArray = result["data"].toArray();

    for (const QString& ocrText : ocrTexts) {
        for (int i = 0; i < dataArray.size(); ++i) {
            QJsonObject item = dataArray[i].toObject();
            if (!comparesEqual(item["text"].toString(), ocrText)) {
                continue;
            }
            if (item["score"].toDouble() < threshold) {
                Logger::log(QString("[OCR] 已识别到:" + item["text"].toString() + " 但分数过低"));
                continue;
            }
            if (!ocrClickMatchedItem(winImg, item, roiRect, useRoi, randomClick, saveDir).isEmpty()) {
                return ocrText;
            }
        }
    }

    Logger::log(QString("[OCR] 未识别到文字：" + ocrTexts.join("/")));
    qDebug() << result;
    return QString();
}

/**
 *
 * @param threshold 得分
 * @param randomClick 是否随机点击
 * @param targetLabelName 需要识别的标签名称 名称列表参考 :resource/classes.txt，可以看缩略图
 * @param excludeStartWidth 排除区域起始宽度百分比 (0.0-1.0)
 * @param excludeEndWidth 排除区域结束宽度百分比 (0.0-1.0)
 */
QString ScriptActions::yoloRecognizesAndClick(const double threshold, const bool randomClick, const QString& targetLabelName, double excludeStartWidth, double excludeEndWidth)
{
    cv::Mat winImg = capture::captureGameWindow();

    if (winImg.empty())
    {
        return nullptr;
    }

    cv::Mat captureImg = winImg.clone();

    cv::Point clickPt;
    cv::Rect matchRect;
    auto final_detections = YOLODetector::getInstance().detect(captureImg, threshold);

    // 在图像上绘制检测结果
    for (const auto& det : final_detections) {
        QString labelName = ClassNameCache::getClassName(det.class_id);
        // Logger::log(labelName);
        // qDebug() << det.confidence << ", id:" << det.class_id;
        cv::rectangle(captureImg, det.bbox, cv::Scalar(0, 255, 0), 2);
        std::string label = labelName.toStdString();
        cv::putText(captureImg, label, cv::Point(det.bbox.x, det.bbox.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

        if (comparesEqual(targetLabelName, labelName))
        {
            //找到需要的目标标签
            matchRect = det.bbox;
            //此处写法默认最后一个 也可视作随机
        }
    }

    if (!matchRect.empty())
    {
        if (randomClick) {
            clickPt = vision::randomPointInRectExcludeWidth(matchRect, excludeStartWidth, excludeEndWidth, 10);
        } else {
            clickPt = cv::Point(matchRect.x + matchRect.width / 2,
                               matchRect.y + matchRect.height / 2);
        }

        // 在图像上标记点击点
        drawClickMarker(captureImg, clickPt);

        GameWindow::instance().clickInWindow(clickPt);
    }else
    {
        Logger::log(QString("未识别到指定目标"));
    }

    // 如果指定了排除区域，也在图像上标记出来（蓝色矩形）
    if (excludeStartWidth < excludeEndWidth && matchRect.width > 0) {
        int excludeX = matchRect.x + matchRect.width * excludeStartWidth;
        int excludeWidth = matchRect.width * (excludeEndWidth - excludeStartWidth);
        cv::Rect excludeRect(excludeX, matchRect.y, excludeWidth, matchRect.height);
        cv::rectangle(captureImg, excludeRect, cv::Scalar(255, 0, 0), 2);
        cv::putText(captureImg, "Exclude Area", cv::Point(excludeRect.x, excludeRect.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 1);
    }

    QString savePath = AppPaths::instance().matchResultPath();
    cv::imwrite(savePath.toStdString(), captureImg);
    processAndShowImage(savePath);

    if (matchRect.empty())
    {
        return {};
    }
    return savePath;
}

/**
 * 返回识别数据
 * @param threshold 得分
 * @param excludeStartWidth 排除区域起始宽度百分比 (0.0-1.0)
 * @param excludeEndWidth 排除区域结束宽度百分比 (0.0-1.0)
 */
std::vector<Detection> ScriptActions::yoloRecognizes(const double threshold, double excludeStartWidth,
                                                     double excludeEndWidth)
{
    cv::Mat winImg = capture::captureGameWindow();

    if (winImg.empty())
    {
        Logger::log(QString("游戏画面获取失败，请查看日志！"));
        return {};
    }

    cv::Mat captureImg = winImg.clone();

    cv::Rect matchRect;
    auto final_detections = YOLODetector::getInstance().detect(captureImg, threshold);

    // 在图像上绘制检测结果
    for (auto& det : final_detections) {
        QString labelName = ClassNameCache::getClassName(det.class_id);
        cv::rectangle(captureImg, det.bbox, cv::Scalar(0, 255, 0), 2);
        std::string label = labelName.toStdString();
        cv::putText(captureImg, label, cv::Point(det.bbox.x, det.bbox.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        det.className = labelName;
    }

    // 输出识别结果日志（置信度阈值=threshold，低于该分数的已在检测阶段过滤）
    Logger::log(QString("YOLO 识别完成：共 %1 个目标（置信度阈值=%2）")
                    .arg(final_detections.size())
                    .arg(threshold, 0, 'f', 2));
    for (const auto& det : final_detections) {
        Logger::log(QString("  - %1  置信度=%2  位置=[%3,%4,%5x%6]")
                        .arg(det.className)
                        .arg(det.confidence, 0, 'f', 4)
                        .arg(det.bbox.x).arg(det.bbox.y)
                        .arg(det.bbox.width).arg(det.bbox.height));
    }

    // 如果指定了排除区域，也在图像上标记出来（蓝色矩形）
    if (excludeStartWidth < excludeEndWidth && matchRect.width > 0) {
        int excludeX = matchRect.x + matchRect.width * excludeStartWidth;
        int excludeWidth = matchRect.width * (excludeEndWidth - excludeStartWidth);
        cv::Rect excludeRect(excludeX, matchRect.y, excludeWidth, matchRect.height);
        cv::rectangle(captureImg, excludeRect, cv::Scalar(255, 0, 0), 2);
        cv::putText(captureImg, "Exclude Area", cv::Point(excludeRect.x, excludeRect.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 1);
    }

    QString savePath = AppPaths::instance().matchResultPath();
    cv::imwrite(savePath.toStdString(), captureImg);
    processAndShowImage(savePath);

    return final_detections;
}

/**
 * @param threshold 得分
 * @param targetLabels 需要包含的标签列表
 * @param matchAll 是否需要全部标签都命中
 */
bool ScriptActions::yoloContainsLabels(const double threshold, const QStringList& targetLabels, const bool matchAll)
{
    if (targetLabels.isEmpty()) {
        Logger::log(QString("YOLO标签为空"));
        return false;
    }

    const auto detections = yoloRecognizes(threshold, 0.0, 0.0);
    const auto matchesLabel = [&detections](const QString& label) {
        return hasDetectionWithLabel(detections, label);
    };

    const bool matched = matchAll
        ? std::all_of(targetLabels.cbegin(), targetLabels.cend(), matchesLabel)
        : std::any_of(targetLabels.cbegin(), targetLabels.cend(), matchesLabel);

    Logger::log(QString("YOLO标签%1: %2")
                    .arg(matched ? "命中" : "未命中")
                    .arg(targetLabels.join(", ")));
    return matched;
}

bool ScriptActions::hasDetectionWithLabel(const std::vector<Detection>& detections, const QString& targetLabel)
{
    return std::any_of(detections.begin(), detections.end(),
        [&](const Detection& det) {
            return comparesEqual(det.className, targetLabel);
        });
}

void ScriptActions::clickDetection(const Detection& det, bool randomClick)
{
    cv::Point physicalClickPt;
    if (randomClick) {
        physicalClickPt = vision::randomPointInRectExcludeWidth(det.bbox, 0.0, 0.0, 3);
    } else {
        physicalClickPt = cv::Point(det.bbox.x + det.bbox.width / 2,
                                    det.bbox.y + det.bbox.height / 2);
    }

    QString capturePath = AppPaths::instance().dx11CapturePath();
    QString matchPath = AppPaths::instance().matchResultPath();
    cv::Mat captureImg = cv::imread(capturePath.toStdString());
    QString labelName = ClassNameCache::getClassName(det.class_id);
    if (!captureImg.empty()) {
        cv::rectangle(captureImg, det.bbox, cv::Scalar(0, 255, 0), 2);
        std::string label = labelName.toStdString();
        cv::putText(captureImg, label, cv::Point(det.bbox.x, det.bbox.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        drawClickMarker(captureImg, physicalClickPt);
        cv::imwrite(matchPath.toStdString(), captureImg);
        processAndShowImage(matchPath);
    } else {
        Logger::log(QString("共享内存截图缺失，跳过调试图保存"));
    }

    GameWindow::instance().clickInWindow(physicalClickPt);
}

QString ScriptActions::clickFirstDetectionByLabels(const QStringList& targetLabels, double threshold,
                                                   double excludeStart, double excludeEnd,
                                                   bool randomClick)
{
    // 只截图/识别一次，按 targetLabels 的填入顺序作为优先级，命中哪个就点哪个（只点一次）
    const auto detections = yoloRecognizes(threshold, excludeStart, excludeEnd);

    for (const QString& targetLabel : targetLabels) {
        for (const auto& det : detections) {
            if (comparesEqual(det.className, targetLabel)) {
                clickDetection(det, randomClick);
                return targetLabel;
            }
        }
    }

    return QString();
}

bool ScriptActions::clickDetectionByLabel(const QString& targetLabel, double threshold,
                                          double excludeStart, double excludeEnd,
                                          bool randomClick)
{
    return !clickFirstDetectionByLabels(QStringList{targetLabel}, threshold, excludeStart, excludeEnd, randomClick).isEmpty();
}

// 智能路径处理：绝对路径直接使用，相对路径拼接基础路径
QString ScriptActions::resolveTemplatePath(const QString& templatePath, const QString& basePath)
{
    QFileInfo fileInfo(templatePath);
    if (fileInfo.isAbsolute()) {
        return templatePath;
    }
    return basePath + templatePath;
}

double ScriptActions::dpiScalingFactor()
{
    HDC hdc = GetDC(NULL);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    return dpiX / 96.0; // 标准DPI为96
}
