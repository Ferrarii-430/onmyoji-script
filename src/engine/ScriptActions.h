#ifndef SCRIPTACTIONS_H
#define SCRIPTACTIONS_H

#include <QJsonArray>
#include <QObject>
#include <QRectF>
#include <QString>
#include <QStringList>
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
    // colorCheck 为是否做 HSV 颜色校验（用于区分同形状不同色的模板）
    QString opencvRecognizesAndClick(const QString& templPath, double threshold, bool randomClick, bool colorCheck = false);
    QString opencvRecognizesAndClickByBase64(const QString& base64, double threshold, bool randomClick,
                                             bool colorCheck = false, const cv::Size& captureSize = cv::Size());

    // OCR 识别；roiPercent 为识别区域（左/上/宽/高，单位为图片尺寸的百分比 0~100）；
    // 宽或高 <= 0 时表示识别整张图
    QJsonArray ocrRecognizes(const QRectF& roiPercent = QRectF());
    QString ocrRecognizesAndClick(const QString& ocrText, double threshold, bool randomClick,
                                  const QRectF& roiPercent = QRectF());
    // 单次 OCR 识别，多个文字按填入顺序作为优先级，命中首个即点击并返回命中的文字，
    // 全部未命中返回空。用于「只识别一次、多关键字择一点击」的场景。
    QString ocrRecognizesAndClickAny(const QStringList& ocrTexts, double threshold, bool randomClick,
                                     const QRectF& roiPercent = QRectF());

    // YOLO 识别
    std::vector<Detection> yoloRecognizes(double threshold, double excludeStartWidth, double excludeEndWidth);
    // YOLO 是否包含指定标签；matchAll 为 false 时任一标签命中即返回 true
    bool yoloContainsLabels(double threshold, const QStringList& targetLabels, bool matchAll = false);
    QString yoloRecognizesAndClick(double threshold, bool randomClick, const QString& targetLabelName,
                                   double excludeStartWidth, double excludeEndWidth);

    // 在 YOLO 识别结果中点击指定标签，成功返回 true
    bool clickDetectionByLabel(const QString& targetLabel, double threshold,
                               double excludeStart, double excludeEnd,
                               bool randomClick = true);
    // 单次 YOLO 识别，多个标签按填入顺序作为优先级，命中首个即点击并返回命中的标签，
    // 全部未命中返回空。用于「只识别一次、多标签择一点击」的场景。
    QString clickFirstDetectionByLabels(const QStringList& targetLabels, double threshold,
                                        double excludeStart, double excludeEnd,
                                        bool randomClick = true);
    static bool hasDetectionWithLabel(const std::vector<Detection>& detections, const QString& targetLabel);

    // 发射信号让 UI 显示识别结果图
    void processAndShowImage(const QString& imagePath);

signals:
    void requestShowImage(const QString& imagePath);

private:
    ScriptActions() = default;

    static QString resolveTemplatePath(const QString& templatePath, const QString& basePath);

    // 命中某个 YOLO 检测框后：画框、保存调试图、回显并点击
    void clickDetection(const Detection& det, bool randomClick = true);
    // 命中某条 OCR 文字后：换算坐标、画框、保存调试图、回显并点击，返回结果图路径
    // roiScale 为裁剪图送入 OCR 前的放大倍数，用于把 OCR 坐标还原回裁剪图原始像素
    QString ocrClickMatchedItem(const cv::Mat& winImg, const QJsonObject& item, const cv::Rect& roiRect,
                                bool useRoi, double roiScale, bool randomClick, const QString& saveDir);
};

#endif // SCRIPTACTIONS_H
