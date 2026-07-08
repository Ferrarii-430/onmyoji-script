#ifndef SCRIPTACTIONS_H
#define SCRIPTACTIONS_H

#include <QJsonArray>
#include <QObject>
#include <QRectF>
#include <QString>
#include <opencv2/core/mat.hpp>
#include <vector>

#include "src/vision/YOLODetector.h"

// 脚本动作层：对外提供"识别 + 点击"的高层原子动作，
// 由 TaskRunner / 场景逻辑调用。
// 截图、窗口、输入、识别等实现分别位于 game/、vision/ 模块。
class ScriptActions : public QObject
{
    Q_OBJECT

public:
    static ScriptActions& instance();

    ScriptActions(const ScriptActions&) = delete;
    ScriptActions& operator=(const ScriptActions&) = delete;

    // OpenCV 模板匹配识别并点击，成功返回结果图路径，失败返回空
    QString opencvRecognizesAndClick(const QString& templPath, double threshold, bool randomClick);
    QString opencvRecognizesAndClickByBase64(const QString& base64, double threshold, bool randomClick);

    // OCR 识别；roiPercent 为识别区域（左/上/宽/高，单位为图片尺寸的百分比 0~100）；
    // 宽或高 <= 0 时表示识别整张图
    QJsonArray ocrRecognizes(const QRectF& roiPercent = QRectF());
    QString ocrRecognizesAndClick(const QString& ocrText, double threshold, bool randomClick,
                                  const QRectF& roiPercent = QRectF());

    // YOLO 识别
    std::vector<Detection> yoloRecognizes(double threshold, double excludeStartWidth, double excludeEndWidth);
    QString yoloRecognizesAndClick(double threshold, bool randomClick, const QString& targetLabelName,
                                   double excludeStartWidth, double excludeEndWidth);

    // 在 YOLO 识别结果中点击指定标签，成功返回 true
    bool clickDetectionByLabel(const QString& targetLabel, double threshold,
                               double excludeStart, double excludeEnd);
    static bool hasDetectionWithLabel(const std::vector<Detection>& detections, const QString& targetLabel);

    // 发射信号让 UI 显示识别结果图
    void processAndShowImage(const QString& imagePath);

signals:
    void requestShowImage(const QString& imagePath);

private:
    ScriptActions() = default;

    static QString resolveTemplatePath(const QString& templatePath, const QString& basePath);
    static double dpiScalingFactor();
};

#endif // SCRIPTACTIONS_H
