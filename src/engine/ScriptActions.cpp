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
#include "src/vision/ImageIo.h"
#include "src/vision/OcrEngine.h"
#include "src/vision/TemplateMatcher.h"

namespace {

struct CachedTemplate {
    qint64 fileSize = 0;
    qint64 modifiedTime = 0;
    cv::Mat image;
};

struct CachedTemplateWithMask {
    qint64 fileSize = 0;
    qint64 modifiedTime = 0;
    cv::Mat gray;
    cv::Mat mask;
};

// 加载模板并提取灰度图 + Alpha 掩码（若模板为 PNG 透明图）。
// 返回的 gray 可直接传给 findTemplate，mask 用于忽略透明区域。
CachedTemplateWithMask loadTemplateWithMask(const QString& templatePath, bool& cacheHit)
{
    static QHash<QString, CachedTemplateWithMask> cache;

    const QFileInfo fileInfo(templatePath);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    const QString cacheKey = canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
    const qint64 fileSize = fileInfo.size();
    const qint64 modifiedTime = fileInfo.lastModified().toMSecsSinceEpoch();

    const auto cached = cache.constFind(cacheKey);
    if (cached != cache.cend()
        && cached->fileSize == fileSize
        && cached->modifiedTime == modifiedTime
        && !cached->gray.empty()) {
        cacheHit = true;
        return cached.value();
    }

    cacheHit = false;
    cv::Mat src = vision::imreadQt(templatePath, cv::IMREAD_UNCHANGED);
    if (src.empty()) {
        return CachedTemplateWithMask{};
    }

    cv::Mat gray, mask;
    vision::extractTemplateMask(src, gray, mask);
    if (gray.empty()) {
        return CachedTemplateWithMask{};
    }

    if (cache.size() >= 64 && !cache.contains(cacheKey)) {
        cache.clear();
    }
    CachedTemplateWithMask item{fileSize, modifiedTime, gray, mask};
    cache.insert(cacheKey, item);
    return item;
}

// 加载彩色模板(BGR)并带缓存，供 HSV 颜色校验使用。
cv::Mat loadTemplateColor(const QString& templatePath, bool& cacheHit)
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
    cv::Mat image = vision::imreadQt(templatePath, cv::IMREAD_COLOR);
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

// RapidOCR 预处理白边宽度：裁剪模式增大以给检测网络更多边缘上下文，整图模式用较小值避免无谓开销
constexpr int OCR_PADDING_ROI = 200;
constexpr int OCR_PADDING_FULL = 50;

// 对裁剪图按开关组合做预处理，提高小字号/暗底文字的识别率。
// 返回处理后的图像；outScale 输出放大倍数（未放大时为 1.0）；
// outApplied 输出实际生效的处理步骤描述，用于日志排查（未开启或条件不满足的项不会记录）。
cv::Mat enhanceOcrRoiImage(const cv::Mat& roiImg, const ocr::Enhance enhance, double& outScale,
                           QStringList& outApplied)
{
    outScale = 1.0;
    outApplied.clear();
    // 深拷贝，避免后续原地处理污染截图原图（roiImg 是全图的视图）
    cv::Mat img = roiImg.clone();

    if (ocr::hasFlag(enhance, ocr::Enhance::Upscale)) {
        // 裁剪图最小边不足时等比放大，保证文字像素落入检测网络友好区间；
        // 放大倍率过大会导致插值失真反而识别不准，故设上限 MAX_OCR_SCALE。
        constexpr double MIN_OCR_SIDE = 320.0;
        constexpr double MAX_OCR_SCALE = 3.0;
        const double bySide = MIN_OCR_SIDE / std::min(img.cols, img.rows);
        const double scale = std::min(std::max(bySide, 1.0), MAX_OCR_SCALE);
        if (scale > 1.0) {
            outScale = scale;
            cv::Mat scaledImg;
            cv::resize(img, scaledImg, cv::Size(), scale, scale, cv::INTER_CUBIC);
            img = scaledImg;
            outApplied << QString("放大x%1(%2x%3)")
                              .arg(scale, 0, 'f', 2).arg(img.cols).arg(img.rows);
            if (scale >= MAX_OCR_SCALE) {
                outApplied << QStringLiteral("已达放大上限");
            }
        } else {
            outApplied << QStringLiteral("放大(尺寸已足够,跳过)");
        }
    }

    const bool toGray = ocr::hasFlag(enhance, ocr::Enhance::Grayscale)
                        || ocr::hasFlag(enhance, ocr::Enhance::Contrast)
                        || ocr::hasFlag(enhance, ocr::Enhance::AutoInvert);
    if (toGray && img.channels() > 1) {
        cv::Mat grayImg;
        cv::cvtColor(img, grayImg, cv::COLOR_BGR2GRAY);
        img = grayImg;
        outApplied << QStringLiteral("灰度");
    }

    if (ocr::hasFlag(enhance, ocr::Enhance::Contrast)) {
        // CLAHE 只支持单通道，上面已确保转灰度
        const cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        cv::Mat equalizedImg;
        clahe->apply(img, equalizedImg);
        img = equalizedImg;
        outApplied << QStringLiteral("CLAHE对比度");
    }

    if (ocr::hasFlag(enhance, ocr::Enhance::Sharpen)) {
        // USM：原图 - 高斯模糊，恢复插值放大后被抹平的笔画边缘
        cv::Mat blurred;
        cv::GaussianBlur(img, blurred, cv::Size(0, 0), 1.2);
        cv::addWeighted(img, 1.5, blurred, -0.5, 0, img);
        outApplied << QStringLiteral("USM锐化");
    }

    if (ocr::hasFlag(enhance, ocr::Enhance::AutoInvert)) {
        // 暗底亮字反色成白底黑字：均值低于中灰即判定为暗底
        const double meanValue = cv::mean(img)[0];
        if (meanValue < 110.0) {
            cv::bitwise_not(img, img);
            outApplied << QString("反色(均值%1)").arg(meanValue, 0, 'f', 1);
        } else {
            outApplied << QString("反色(亮底均值%1,跳过)").arg(meanValue, 0, 'f', 1);
        }
    }

    if (outApplied.isEmpty()) {
        outApplied << QStringLiteral("无");
    }
    return img;
}

// 识别整张图片时按开关做逐像素增强并落盘，返回可送入 OCR 的图片路径。
// 未开启任何逐像素项时返回空字符串，调用方沿用原有的整图识别路径（不改变旧行为）。
// 整图不做放大：截图本身尺寸足够，放大只会拖慢识别且无收益。
QString prepareFullImageForOcr(const cv::Mat& winImg, const ocr::Enhance enhance,
                               const QString& saveDir)
{
    const ocr::Enhance pixelEnhance = enhance & ocr::PixelEnhanceMask;
    if (pixelEnhance == ocr::Enhance::None) {
        return QString();
    }

    QDir dir(saveDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    double ignoredScale = 1.0;
    QStringList applied;
    const cv::Mat ocrImg = enhanceOcrRoiImage(winImg, pixelEnhance, ignoredScale, applied);
    const QString ocrImagePath = saveDir + "ocr_full_capture.png";
    if (!vision::imwriteQt(ocrImagePath, ocrImg)) {
        Logger::log(QString("整图增强图片保存失败，改用原始截图识别"));
        return QString();
    }

    Logger::log(QString("OCR识别整张图片: 像素(%1x%2) 增强[%3]")
                    .arg(winImg.cols).arg(winImg.rows)
                    .arg(applied.join(" + ")));
    return ocrImagePath;
}

// 按百分比(0~100)计算 OCR 识别区域并在需要裁剪时保存裁剪图。
// 返回换算回全图坐标所需的 roiRect（未裁剪时为整张图，原点为 0）。
// ocrImagePath 为送入 OCR 的图片路径；为空表示直接识别原始截图。
// outScale 输出裁剪图送入 OCR 前的放大倍数（坐标还原需除以该值）；未裁剪时为 1.0。
// outCropped 输出是否真的裁剪了（决定 padding 与坐标偏移），整图增强时为 false。
// enhance 为预处理开关组合：裁剪模式下全部项生效；识别整图时只应用逐像素项（不放大）。
cv::Rect computeOcrRoi(const cv::Mat& winImg, const QRectF& roiPercent,
                       const QString& saveDir, QString& ocrImagePath, double& outScale,
                       bool& outCropped, const ocr::Enhance enhance)
{
    ocrImagePath.clear();
    outScale = 1.0;
    outCropped = false;
    const cv::Rect fullRect(0, 0, winImg.cols, winImg.rows);
    if (roiPercent.width() <= 0.0 || roiPercent.height() <= 0.0) {
        ocrImagePath = prepareFullImageForOcr(winImg, enhance, saveDir);
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
        ocrImagePath = prepareFullImageForOcr(winImg, enhance, saveDir);
        return fullRect;
    }
    if (roiRect == fullRect) {
        // 区域即整张图，无需裁剪，但仍按开关做逐像素增强
        ocrImagePath = prepareFullImageForOcr(winImg, enhance, saveDir);
        return fullRect;
    }

    QDir dir(saveDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    ocrImagePath = saveDir + "ocr_roi_capture.png";
    outCropped = true;
    QStringList applied;
    const cv::Mat ocrImg = enhanceOcrRoiImage(winImg(roiRect), enhance, outScale, applied);
    vision::imwriteQt(ocrImagePath, ocrImg);
    Logger::log(QString("OCR识别区域: 百分比(%1%,%2%,%3%,%4%) -> 像素(%5,%6,%7x%8) 增强[%9]")
                    .arg(roiPercent.x()).arg(roiPercent.y())
                    .arg(roiPercent.width()).arg(roiPercent.height())
                    .arg(roiRect.x).arg(roiRect.y)
                    .arg(roiRect.width).arg(roiRect.height)
                    .arg(applied.join(" + ")));
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

QString ScriptActions::opencvRecognizesAndClickByBase64(const QString& base64, const double threshold,
                                                        const bool randomClick, const bool colorCheck,
                                                        const cv::Size& captureSize, const ClickExclude& exclude)
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

    // 若提供了模板截取分辨率，写入临时侧载文件供 findTemplate 使用
    if (captureSize.width > 0 && captureSize.height > 0) {
        vision::saveTemplateCaptureSize(tempFile.fileName(), captureSize);
    }

    return opencvRecognizesAndClick(tempFile.fileName(), threshold, randomClick, colorCheck, exclude);
}

QString ScriptActions::opencvRecognizesAndClick(const QString& templPath, const double threshold, const bool randomClick,
                                                const bool colorCheck, const ClickExclude& exclude)
{
    cv::Mat winImg = capture::captureGameWindow();
    if (winImg.empty())
    {
        return nullptr;
    }

    //加载模板文件（同时提取 Alpha 掩码，透明区域不参与匹配）
    QString tempSavePath = resolveTemplatePath(templPath, AppPaths::instance().screenshotPath());
    bool templateCacheHit = false;
    const auto templWithMask = loadTemplateWithMask(tempSavePath, templateCacheHit);
    if (templWithMask.gray.empty()) {
        Logger::log("模板图片加载失败: " + tempSavePath);
        return nullptr;
    }
    if (!templateCacheHit) {
        Logger::log("已加载模板图片: " + tempSavePath +
                    (templWithMask.mask.empty() ? "" : " (含透明掩码)"));
    }
    const cv::Mat& templ = templWithMask.gray;
    const cv::Mat& templMask = templWithMask.mask;

    // 读取模板截取时的游戏窗口分辨率（侧载 JSON），若存在则按该分辨率归一化截图
    const cv::Size templCaptureSize = vision::loadTemplateCaptureSize(tempSavePath);
    if (templCaptureSize.width > 0 && templCaptureSize.height > 0) {
        Logger::log(QString("使用模板截取分辨率: %1x%2").arg(templCaptureSize.width).arg(templCaptureSize.height));
    }

    // 加载彩色模板用于 HSV 颜色校验（灰度匹配无法区分同形状不同色的模板）
    // 仅在 colorCheck 开启时加载与校验
    cv::Mat templColor;
    if (colorCheck) {
        bool colorCacheHit = false;
        templColor = loadTemplateColor(tempSavePath, colorCacheHit);
    }

    // 在窗口图像中查找模板（返回基准坐标系下的矩形 + ScaleInfo）
    Logger::log(QString("OpenCV识图 模板: %1  截图: %2x%3  阈值: %4")
                .arg(tempSavePath).arg(winImg.cols).arg(winImg.rows).arg(threshold, 0, 'f', 2));
    double score = 0.0;
    cv::Rect matchRectBase;
    vision::ScaleInfo scaleInfo;
    bool found = vision::findTemplate(winImg, templ, matchRectBase, score, threshold,
                                      &scaleInfo, templMask, templCaptureSize);

    if (!found) {
        Logger::log(QString("未找到匹配区域! score=%1").arg(score));
        return nullptr;
    }

    // 基准坐标 -> DX11 原始捕获坐标，用于绘制结果图、HSV 校验和点击
    cv::Rect matchRect = vision::convertNormalizedRectToCapture(matchRectBase, scaleInfo);

    // HSV 颜色校验：形状匹配高分但颜色不符(同形状不同色模板)则判为未找到
    if (colorCheck && !vision::verifyHsvColorMatch(winImg, templColor, matchRect)) {
        Logger::log(QString("形状匹配 score=%1 但 HSV 颜色校验不通过，判为未找到").arg(score));
        return nullptr;
    }

    cv::Point clickPt;
    if (randomClick) {
        if (exclude.empty()) {
            clickPt = vision::randomPointInRect(matchRect);
        } else {
            // 按比例生成边框排除条带（同结界突破的排除逻辑），
            // 超过最大尝试次数时备选中心点
            std::vector<cv::Rect> excludeAreas;
            if (exclude.left > 0.0) {
                excludeAreas.push_back(vision::widthExcludeRect(matchRect, 0.0, exclude.left));
            }
            if (exclude.right > 0.0) {
                // 右侧条带直接延伸到框边缘，+1 补偿整型截断，避免最右 1px 漏网
                const int stripW = static_cast<int>(matchRect.width * exclude.right) + 1;
                excludeAreas.emplace_back(matchRect.x + matchRect.width - stripW, matchRect.y, stripW, matchRect.height);
            }
            if (exclude.top > 0.0) {
                excludeAreas.push_back(vision::heightExcludeRect(matchRect, 0.0, exclude.top));
            }
            if (exclude.bottom > 0.0) {
                // 下侧条带同理延伸到框底边缘
                const int stripH = static_cast<int>(matchRect.height * exclude.bottom) + 1;
                excludeAreas.emplace_back(matchRect.x, matchRect.y + matchRect.height - stripH, matchRect.width, stripH);
            }
            clickPt = vision::randomPointInRectExcludeAreas(matchRect, excludeAreas);
        }
    } else {
        clickPt = cv::Point(matchRect.x + matchRect.width / 2,
                           matchRect.y + matchRect.height / 2);
    }

    // 记录点击点在匹配框内的相对位置（百分比），用于验证边框排除是否生效
    Logger::log(QString("OpenCV点击点相对匹配框: x=%1%% y=%2%%")
                .arg(matchRect.width > 0 ? (clickPt.x - matchRect.x) * 100 / matchRect.width : 0)
                .arg(matchRect.height > 0 ? (clickPt.y - matchRect.y) * 100 / matchRect.height : 0));

    // clickPt 为截图坐标系坐标，由 GameWindow::mapCapturePointToClient 负责
    // 截图坐标 -> 客户区坐标 的换算（基于实际捕获尺寸与客户区尺寸比例），
    // 此处不应再除以 DPI 缩放因子，否则在 4k 等高 DPI 屏幕上会二次缩放，
    // 导致点击点偏移到 ROI 框左上方。

    // 保存带识别框和点击位置的图片
    cv::Mat resultImg = winImg.clone();
    cv::rectangle(resultImg, matchRect, cv::Scalar(0, 255, 0), 2);
    drawClickMarker(resultImg, clickPt);

    QString savePath = AppPaths::instance().matchResultPath();
    vision::imwriteQt(savePath, resultImg);

    Logger::log(QString("转换后点击点: (%1, %2)").arg(clickPt.x).arg(clickPt.y));
    GameWindow::instance().clickInWindow(clickPt);
    processAndShowImage(savePath);

    return savePath;
}

std::vector<OpenCvMatch> ScriptActions::opencvFindAll(const QString& templPath, const double threshold, const bool colorCheck)
{
    std::vector<OpenCvMatch> results;

    cv::Mat winImg = capture::captureGameWindow();
    if (winImg.empty()) {
        Logger::log(QString("opencvFindAll: 截图失败"));
        return results;
    }

    // 加载模板（灰度 + Alpha 掩码）
    QString tempSavePath = resolveTemplatePath(templPath, AppPaths::instance().screenshotPath());
    bool templateCacheHit = false;
    const auto templWithMask = loadTemplateWithMask(tempSavePath, templateCacheHit);
    if (templWithMask.gray.empty()) {
        Logger::log("opencvFindAll: 模板图片加载失败: " + tempSavePath);
        return results;
    }
    if (!templateCacheHit) {
        Logger::log("opencvFindAll: 已加载模板图片: " + tempSavePath +
                    (templWithMask.mask.empty() ? "" : " (含透明掩码)"));
    }
    const cv::Mat& templ = templWithMask.gray;
    const cv::Mat& templMask = templWithMask.mask;

    // 模板截取分辨率侧载
    const cv::Size templCaptureSize = vision::loadTemplateCaptureSize(tempSavePath);
    if (templCaptureSize.width > 0 && templCaptureSize.height > 0) {
        Logger::log(QString("opencvFindAll: 使用模板截取分辨率: %1x%2")
                    .arg(templCaptureSize.width).arg(templCaptureSize.height));
    }

    // 多目标匹配
    Logger::log(QString("opencvFindAll 模板: %1  截图: %2x%3  阈值: %4")
                .arg(tempSavePath).arg(winImg.cols).arg(winImg.rows).arg(threshold, 0, 'f', 2));

    std::vector<vision::TemplateMatch> matches;
    vision::ScaleInfo scaleInfo;
    bool found = vision::findTemplateAll(winImg, templ, matches, threshold,
                                         &scaleInfo, templMask, templCaptureSize);

    if (!found || matches.empty()) {
        Logger::log(QString("opencvFindAll: 未找到匹配区域"));
        // 仍然回显截图，让用户看到当前画面
        QString savePath = AppPaths::instance().matchResultPath();
        vision::imwriteQt(savePath, winImg);
        processAndShowImage(savePath);
        return results;
    }

    // 加载彩色模板用于 HSV 颜色校验
    cv::Mat templColor;
    if (colorCheck) {
        bool colorCacheHit = false;
        templColor = loadTemplateColor(tempSavePath, colorCacheHit);
    }

    // 基准坐标 -> DX11 原始捕获坐标，并做 HSV 颜色校验
    cv::Mat resultImg = winImg.clone();
    for (const auto& m : matches) {
        cv::Rect matchRect = vision::convertNormalizedRectToCapture(m.rect, scaleInfo);

        // HSV 颜色校验：不通过则跳过该匹配
        if (colorCheck && !vision::verifyHsvColorMatch(winImg, templColor, matchRect)) {
            Logger::log(QString("opencvFindAll: score=%1 但 HSV 颜色校验不通过，跳过").arg(m.score, 0, 'f', 2));
            continue;
        }

        cv::Point center(matchRect.x + matchRect.width / 2,
                         matchRect.y + matchRect.height / 2);
        results.push_back(OpenCvMatch{matchRect, m.score, center});

        // 在结果图上绘制 ROI 框 + 分数标签
        cv::rectangle(resultImg, matchRect, cv::Scalar(0, 255, 0), 2);
        QString label = QString("%1").arg(m.score, 0, 'f', 2);
        cv::putText(resultImg, label.toStdString(),
                    cv::Point(matchRect.x, matchRect.y - 8),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
    }

    Logger::log(QString("opencvFindAll: 共识别到 %1 个目标").arg(results.size()));

    // 保存并回显结果图（不点击）
    QString savePath = AppPaths::instance().matchResultPath();
    vision::imwriteQt(savePath, resultImg);
    processAndShowImage(savePath);

    return results;
}

QJsonArray ScriptActions::ocrRecognizes(const QRectF& roiPercent, const ocr::Enhance enhance)
{
    cv::Mat winImg = capture::captureGameWindow();

    if (winImg.empty())
    {
        return QJsonArray();
    }

    const QString saveDir = AppPaths::instance().thumbnailPath();

    QString ocrImagePath;
    double roiScale = 1.0;
    bool cropped = false;
    const cv::Rect roiRect = computeOcrRoi(winImg, roiPercent, saveDir, ocrImagePath, roiScale, cropped, enhance);

    // 裁剪模式增大 padding，给检测网络更多边缘上下文
    QJsonObject result = vision::runRapidOCR(ocrImagePath, cropped ? OCR_PADDING_ROI : OCR_PADDING_FULL);
    QJsonArray dataArray = result["data"].toArray();

    if (cropped) {
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
                // OCR 坐标基于放大后的裁剪图，先除以 roiScale 还原裁剪图原始像素，再加 roiRect 偏移
                adjustedPoint.append(static_cast<int>(point[0].toInt() / roiScale) + roiRect.x);
                adjustedPoint.append(static_cast<int>(point[1].toInt() / roiScale) + roiRect.y);
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
                                           const bool useRoi, const double roiScale,
                                           const bool randomClick, const QString& saveDir)
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

    // OCR 坐标基于（可能放大后的）裁剪图，需先除以 roiScale 还原到裁剪图原始像素，再加 roiRect 偏移
    int x1 = static_cast<int>(point1[0].toInt() / roiScale);
    int y1 = static_cast<int>(point1[1].toInt() / roiScale);
    int x2 = static_cast<int>(point2[0].toInt() / roiScale);
    int y2 = static_cast<int>(point2[1].toInt() / roiScale);
    int x3 = static_cast<int>(point3[0].toInt() / roiScale);
    int y3 = static_cast<int>(point3[1].toInt() / roiScale);
    int x4 = static_cast<int>(point4[0].toInt() / roiScale);
    int y4 = static_cast<int>(point4[1].toInt() / roiScale);

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
    vision::imwriteQt(savePath, resultImg);

    GameWindow::instance().clickInWindow(clickPt);
    processAndShowImage(savePath);
    return savePath;
}

QString ScriptActions::ocrRecognizesAndClick(const QString& ocrText, const double threshold, const bool randomClick,
                                             const QRectF& roiPercent, const ocr::Enhance enhance)
{
    cv::Mat winImg = capture::captureGameWindow();
    bool hasOcrText = false;

    if (winImg.empty())
    {
        return nullptr;
    }

    const QString saveDir = AppPaths::instance().thumbnailPath();

    QString ocrImagePath;
    double roiScale = 1.0;
    bool cropped = false;
    const cv::Rect roiRect = computeOcrRoi(winImg, roiPercent, saveDir, ocrImagePath, roiScale, cropped, enhance);

    // 裁剪模式增大 padding，给检测网络更多边缘上下文
    QJsonObject result = vision::runRapidOCR(ocrImagePath, cropped ? OCR_PADDING_ROI : OCR_PADDING_FULL);
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
                    savePath = ocrClickMatchedItem(winImg, item, roiRect, cropped, roiScale, randomClick, saveDir);
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
                                                const bool randomClick, const QRectF& roiPercent,
                                                const ocr::Enhance enhance)
{
    cv::Mat winImg = capture::captureGameWindow();

    if (winImg.empty())
    {
        return QString();
    }

    const QString saveDir = AppPaths::instance().thumbnailPath();

    QString ocrImagePath;
    double roiScale = 1.0;
    bool cropped = false;
    const cv::Rect roiRect = computeOcrRoi(winImg, roiPercent, saveDir, ocrImagePath, roiScale, cropped, enhance);

    // 只做一次 OCR，多个文字按填入顺序作为优先级，命中首个即点击并返回命中的文字
    // 裁剪模式增大 padding，给检测网络更多边缘上下文
    QJsonObject result = vision::runRapidOCR(ocrImagePath, cropped ? OCR_PADDING_ROI : OCR_PADDING_FULL);
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
            if (!ocrClickMatchedItem(winImg, item, roiRect, cropped, roiScale, randomClick, saveDir).isEmpty()) {
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
 * @param exclude 随机点击时排除的边框区域比例，全部为 0 表示不排除
 */
QString ScriptActions::yoloRecognizesAndClick(const double threshold, const bool randomClick, const QString& targetLabelName,
                                              const ClickExclude& exclude)
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
            if (!exclude.empty()) {
                // 四边排除：与 OpenCV 识别点击相同的边框排除逻辑，
                // 超过最大尝试次数时备选中心点
                std::vector<cv::Rect> excludeAreas;
                if (exclude.left > 0.0) {
                    excludeAreas.push_back(vision::widthExcludeRect(matchRect, 0.0, exclude.left));
                }
                if (exclude.right > 0.0) {
                    // 右侧条带直接延伸到框边缘，+1 补偿整型截断，避免最右 1px 漏网
                    const int stripW = static_cast<int>(matchRect.width * exclude.right) + 1;
                    excludeAreas.emplace_back(matchRect.x + matchRect.width - stripW, matchRect.y, stripW, matchRect.height);
                }
                if (exclude.top > 0.0) {
                    excludeAreas.push_back(vision::heightExcludeRect(matchRect, 0.0, exclude.top));
                }
                if (exclude.bottom > 0.0) {
                    // 下侧条带同理延伸到框底边缘
                    const int stripH = static_cast<int>(matchRect.height * exclude.bottom) + 1;
                    excludeAreas.emplace_back(matchRect.x, matchRect.y + matchRect.height - stripH, matchRect.width, stripH);
                }
                clickPt = vision::randomPointInRectExcludeAreas(matchRect, excludeAreas);
            } else {
                clickPt = vision::randomPointInRect(matchRect);
            }
        } else {
            clickPt = cv::Point(matchRect.x + matchRect.width / 2,
                               matchRect.y + matchRect.height / 2);
        }

        // 记录点击点在匹配框内的相对位置（百分比），用于验证边框排除是否生效
        Logger::log(QString("YOLO点击点相对匹配框: x=%1%% y=%2%%")
                    .arg(matchRect.width > 0 ? (clickPt.x - matchRect.x) * 100 / matchRect.width : 0)
                    .arg(matchRect.height > 0 ? (clickPt.y - matchRect.y) * 100 / matchRect.height : 0));

        // 在图像上标记点击点
        drawClickMarker(captureImg, clickPt);

        GameWindow::instance().clickInWindow(clickPt);
    }else
    {
        Logger::log(QString("未识别到指定目标"));
    }

    QString savePath = AppPaths::instance().matchResultPath();
    vision::imwriteQt(savePath, captureImg);
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
 */
std::vector<Detection> ScriptActions::yoloRecognizes(const double threshold)
{
    cv::Mat winImg = capture::captureGameWindow();

    if (winImg.empty())
    {
        Logger::log(QString("游戏画面获取失败，请查看日志！"));
        return {};
    }

    cv::Mat captureImg = winImg.clone();

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

    QString savePath = AppPaths::instance().matchResultPath();
    vision::imwriteQt(savePath, captureImg);
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

    const auto detections = yoloRecognizes(threshold);
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

void ScriptActions::clickDetection(const Detection& det, bool randomClick, const ClickExclude& exclude)
{
    cv::Point physicalClickPt;
    if (randomClick) {
        if (!exclude.empty()) {
            // 四边排除：与 yoloRecognizesAndClick 相同的边框排除逻辑
            std::vector<cv::Rect> excludeAreas;
            if (exclude.left > 0.0) {
                excludeAreas.push_back(vision::widthExcludeRect(det.bbox, 0.0, exclude.left));
            }
            if (exclude.right > 0.0) {
                // 右侧条带直接延伸到框边缘，+1 补偿整型截断，避免最右 1px 漏网
                const int stripW = static_cast<int>(det.bbox.width * exclude.right) + 1;
                excludeAreas.emplace_back(det.bbox.x + det.bbox.width - stripW, det.bbox.y, stripW, det.bbox.height);
            }
            if (exclude.top > 0.0) {
                excludeAreas.push_back(vision::heightExcludeRect(det.bbox, 0.0, exclude.top));
            }
            if (exclude.bottom > 0.0) {
                // 下侧条带同理延伸到框底边缘
                const int stripH = static_cast<int>(det.bbox.height * exclude.bottom) + 1;
                excludeAreas.emplace_back(det.bbox.x, det.bbox.y + det.bbox.height - stripH, det.bbox.width, stripH);
            }
            physicalClickPt = vision::randomPointInRectExcludeAreas(det.bbox, excludeAreas);
        } else {
            physicalClickPt = vision::randomPointInRect(det.bbox);
        }
    } else {
        physicalClickPt = cv::Point(det.bbox.x + det.bbox.width / 2,
                                    det.bbox.y + det.bbox.height / 2);
    }

    QString capturePath = AppPaths::instance().dx11CapturePath();
    QString matchPath = AppPaths::instance().matchResultPath();
    cv::Mat captureImg = vision::imreadQt(capturePath);
    QString labelName = ClassNameCache::getClassName(det.class_id);
    if (!captureImg.empty()) {
        cv::rectangle(captureImg, det.bbox, cv::Scalar(0, 255, 0), 2);
        std::string label = labelName.toStdString();
        cv::putText(captureImg, label, cv::Point(det.bbox.x, det.bbox.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        drawClickMarker(captureImg, physicalClickPt);
        vision::imwriteQt(matchPath, captureImg);
        processAndShowImage(matchPath);
    } else {
        Logger::log(QString("共享内存截图缺失，跳过调试图保存"));
    }

    GameWindow::instance().clickInWindow(physicalClickPt);
}

QString ScriptActions::clickFirstDetectionByLabels(const QStringList& targetLabels, double threshold,
                                                   bool randomClick, const ClickExclude& exclude)
{
    // 只截图/识别一次，按 targetLabels 的填入顺序作为优先级，命中哪个就点哪个（只点一次）
    const auto detections = yoloRecognizes(threshold);

    for (const QString& targetLabel : targetLabels) {
        for (const auto& det : detections) {
            if (comparesEqual(det.className, targetLabel)) {
                clickDetection(det, randomClick, exclude);
                return targetLabel;
            }
        }
    }

    return QString();
}

bool ScriptActions::clickDetectionByLabel(const QString& targetLabel, double threshold,
                                          bool randomClick, const ClickExclude& exclude)
{
    return !clickFirstDetectionByLabels(QStringList{targetLabel}, threshold, randomClick, exclude).isEmpty();
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
