#include "src/engine/ScriptActions.h"

#include <windows.h>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
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
    cv::Mat templ = cv::imread(tempSavePath.toStdString());
    if (templ.empty()) {
        Logger::log("模板图片加载失败: " + tempSavePath);
        return nullptr;
    }else {
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
    cv::circle(resultImg, clickPt, 5, cv::Scalar(0, 0, 255), -1);

    QString savePath = AppPaths::instance().matchResultPath();
    cv::imwrite(savePath.toStdString(), resultImg);

    Logger::log(QString("转换后点击点: (%1, %2)").arg(clickPt.x).arg(clickPt.y));
    GameWindow::instance().clickInWindow(clickPt);

    return savePath;
}

QJsonArray ScriptActions::ocrRecognizes()
{
    cv::Mat winImg = capture::captureGameWindow();

    if (winImg.empty())
    {
        return QJsonArray();
    }

    QJsonObject result = vision::runRapidOCR();
    return result["data"].toArray();
}

QString ScriptActions::ocrRecognizesAndClick(const QString& ocrText, const double threshold, const bool randomClick)
{
    cv::Mat winImg = capture::captureGameWindow();
    bool hasOcrText = false;

    if (winImg.empty())
    {
        return nullptr;
    }

    QString saveDir = AppPaths::instance().thumbnailPath();
    QJsonObject result = vision::runRapidOCR();
    QString savePath;

    if (!result.isEmpty()) {
        QJsonArray dataArray = result["data"].toArray();

        for (int i = 0; i < dataArray.size(); ++i) {
            QJsonObject item = dataArray[i].toObject();
            QString text = item["text"].toString();
            QJsonArray box = item["box"].toArray();
            double score = item["score"].toDouble();

            if (comparesEqual(text,ocrText))
            {
                hasOcrText = true;
                if (score >= threshold)
                {
                    cv::Rect matchRect;
                    cv::Point clickPt;
                    if (box.size() == 4) {
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

                        matchRect = cv::Rect(minX, minY, maxX - minX, maxY - minY);
                    } else {
                        qWarning() << "box数组大小不正确，期望4个点，实际:" << box.size();
                        continue;
                    }

                    if (randomClick) {
                        clickPt = vision::randomPointInRect(matchRect);
                    } else {
                        clickPt = cv::Point(matchRect.x + matchRect.width / 2,
                                           matchRect.y + matchRect.height / 2);
                    }

                    // 保存带识别框和点击位置的图片
                    cv::Mat resultImg = winImg.clone();
                    cv::rectangle(resultImg, matchRect, cv::Scalar(0, 255, 0), 2);
                    cv::circle(resultImg, clickPt, 5, cv::Scalar(0, 0, 255), -1);

                    QDir dir(saveDir);
                    if (!dir.exists()) {
                        dir.mkpath(".");
                    }

                    savePath = AppPaths::instance().matchResultPath();
                    cv::imwrite(savePath.toStdString(), resultImg);

                    GameWindow::instance().clickInWindow(clickPt);
                    break;
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

        // 在图像上标记点击点（红色圆点）
        cv::circle(captureImg, clickPt, 5, cv::Scalar(0, 0, 255), -1);

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

bool ScriptActions::hasDetectionWithLabel(const std::vector<Detection>& detections, const QString& targetLabel)
{
    return std::any_of(detections.begin(), detections.end(),
        [&](const Detection& det) {
            return comparesEqual(det.className, targetLabel);
        });
}

bool ScriptActions::clickDetectionByLabel(const QString& targetLabel, double threshold,
                                          double excludeStart, double excludeEnd)
{
    const auto detections_ = yoloRecognizes(threshold, excludeStart, excludeEnd);

    for (auto& det_ : detections_) {
        if (comparesEqual(det_.className, targetLabel)) {
            cv::Point physicalClickPt = vision::randomPointInRectExcludeWidth(det_.bbox, 0.0, 0.0, 10);

            QString capturePath = AppPaths::instance().dx11CapturePath();
            QString matchPath = AppPaths::instance().matchResultPath();
            cv::Mat captureImg = cv::imread(capturePath.toStdString());
            QString labelName = ClassNameCache::getClassName(det_.class_id);
            cv::rectangle(captureImg, det_.bbox, cv::Scalar(0, 255, 0), 2);
            std::string label = labelName.toStdString();
            cv::putText(captureImg, label, cv::Point(det_.bbox.x, det_.bbox.y - 10),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
            cv::imwrite(matchPath.toStdString(), captureImg);
            processAndShowImage(matchPath);

            GameWindow::instance().clickInWindow(physicalClickPt);
            return true;
        }
    }

    return false;
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
