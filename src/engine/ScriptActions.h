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

// OpenCV 多目标识别结果（DX11 原始捕获坐标系，可直接用于点击）
struct OpenCvMatch {
    cv::Rect rect;      // 匹配区域矩形
    double score = 0.0; // 匹配分数
    cv::Point center;   // 矩形中心点
};

// 点击位置的边框排除区域：相对匹配矩形的比例（0~1）。
// 例如 left=0.3 表示匹配框左侧 30% 的竖条带内不取点击点。
// 仅在随机点击（randomClick=true）时生效；全部为 0 表示不排除。
struct ClickExclude {
    double left = 0.0;   // 排除左侧 [0, left) 宽度比例的竖条带
    double right = 0.0;  // 排除右侧 (1-right, 1] 宽度比例的竖条带
    double top = 0.0;    // 排除上侧 [0, top) 高度比例的横条带
    double bottom = 0.0; // 排除下侧 (1-bottom, 1] 高度比例的横条带

    bool empty() const { return left <= 0.0 && right <= 0.0 && top <= 0.0 && bottom <= 0.0; }
};

namespace ocr {

// OCR 裁剪图预处理开关，可按位组合，仅在裁剪模式（roiPercent 有效）下生效。
// 小图文字像素少时逐项开启可显著提高识别率，也可只开需要的项避免副作用。
enum class Enhance : unsigned int
{
    None       = 0,
    Upscale    = 1u << 0, // 按目标文字高度等比放大（双三次插值）
    Grayscale  = 1u << 1, // 转灰度，去掉彩色背景干扰
    Contrast   = 1u << 2, // CLAHE 局部对比度增强，提亮暗底文字
    Sharpen    = 1u << 3, // USM 锐化，恢复放大后的笔画边缘
    AutoInvert = 1u << 4, // 暗底亮字自动反色为白底黑字，贴近模型训练分布
    All        = 0x1Fu,
};

constexpr Enhance operator|(const Enhance a, const Enhance b)
{
    return static_cast<Enhance>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

constexpr Enhance operator&(const Enhance a, const Enhance b)
{
    return static_cast<Enhance>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}

constexpr bool hasFlag(const Enhance value, const Enhance flag)
{
    return (static_cast<unsigned int>(value) & static_cast<unsigned int>(flag)) != 0;
}

// 逐像素处理项（不含 Upscale）。识别整张图片时截图尺寸本就足够，
// 放大只会拖慢识别，故整图模式只保留这些项。
constexpr Enhance PixelEnhanceMask = Enhance::Grayscale | Enhance::Contrast
                                     | Enhance::Sharpen | Enhance::AutoInvert;

} // namespace ocr

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
    // exclude 为随机点击时排除的边框区域比例，仅在 randomClick=true 时生效
    QString opencvRecognizesAndClick(const QString& templPath, double threshold, bool randomClick,
                                     bool colorCheck = false, const ClickExclude& exclude = ClickExclude{});
    QString opencvRecognizesAndClickByBase64(const QString& base64, double threshold, bool randomClick,
                                             bool colorCheck = false, const cv::Size& captureSize = cv::Size(),
                                             const ClickExclude& exclude = ClickExclude{});

    // OpenCV 多目标识别：找出截图中所有匹配模板的区域，回显带 ROI 框的结果图，不点击。
    // 返回所有匹配结果（DX11 原始捕获坐标系），按分数降序排列，供调用方做下一步判断。
    std::vector<OpenCvMatch> opencvFindAll(const QString& templPath, double threshold, bool colorCheck = false);

    // OCR 识别；roiPercent 为识别区域（左/上/宽/高，单位为图片尺寸的百分比 0~100）；
    // 宽或高 <= 0 时表示识别整张图
    // enhance 默认 Upscale：小裁剪图自动放大是本就存在的行为，保持不变；
    // 其余增强项（灰度/对比度/锐化/反色）默认关闭，需显式开启。
    // 裁剪模式下全部项生效；识别整张图片时只应用逐像素项，不做放大。
    QJsonArray ocrRecognizes(const QRectF& roiPercent = QRectF(),
                             ocr::Enhance enhance = ocr::Enhance::Upscale);
    QString ocrRecognizesAndClick(const QString& ocrText, double threshold, bool randomClick,
                                  const QRectF& roiPercent = QRectF(),
                                  ocr::Enhance enhance = ocr::Enhance::Upscale);
    // 单次 OCR 识别，多个文字按填入顺序作为优先级，命中首个即点击并返回命中的文字，
    // 全部未命中返回空。用于「只识别一次、多关键字择一点击」的场景。
    QString ocrRecognizesAndClickAny(const QStringList& ocrTexts, double threshold, bool randomClick,
                                     const QRectF& roiPercent = QRectF(),
                                     ocr::Enhance enhance = ocr::Enhance::Upscale);

    // YOLO 识别（纯识别，不点击）。返回所有检测结果，由调用方决定如何处理
    std::vector<Detection> yoloRecognizes(double threshold);
    // YOLO 是否包含指定标签；matchAll 为 false 时任一标签命中即返回 true
    bool yoloContainsLabels(double threshold, const QStringList& targetLabels, bool matchAll = false);
    QString yoloRecognizesAndClick(double threshold, bool randomClick, const QString& targetLabelName,
                                   const ClickExclude& exclude = ClickExclude{});

    // 在 YOLO 识别结果中点击指定标签，成功返回 true
    // exclude 为随机点击时排除的边框区域比例，仅在 randomClick=true 时生效
    bool clickDetectionByLabel(const QString& targetLabel, double threshold,
                               bool randomClick = true,
                               const ClickExclude& exclude = ClickExclude{});
    // 单次 YOLO 识别，多个标签按填入顺序作为优先级，命中首个即点击并返回命中的标签，
    // 全部未命中返回空。用于「只识别一次、多标签择一点击」的场景。
    QString clickFirstDetectionByLabels(const QStringList& targetLabels, double threshold,
                                        bool randomClick = true,
                                        const ClickExclude& exclude = ClickExclude{});
    static bool hasDetectionWithLabel(const std::vector<Detection>& detections, const QString& targetLabel);

    // 发射信号让 UI 显示识别结果图
    void processAndShowImage(const QString& imagePath);

signals:
    void requestShowImage(const QString& imagePath);

private:
    ScriptActions() = default;

    static QString resolveTemplatePath(const QString& templatePath, const QString& basePath);

    // 命中某个 YOLO 检测框后：画框、保存调试图、回显并点击
    // exclude 为随机点击时排除的边框区域比例，仅在 randomClick=true 时生效
    void clickDetection(const Detection& det, bool randomClick = true,
                        const ClickExclude& exclude = ClickExclude{});
    // 命中某条 OCR 文字后：换算坐标、画框、保存调试图、回显并点击，返回结果图路径
    // roiScale 为裁剪图送入 OCR 前的放大倍数，用于把 OCR 坐标还原回裁剪图原始像素
    QString ocrClickMatchedItem(const cv::Mat& winImg, const QJsonObject& item, const cv::Rect& roiRect,
                                bool useRoi, double roiScale, bool randomClick, const QString& saveDir);
};

#endif // SCRIPTACTIONS_H
