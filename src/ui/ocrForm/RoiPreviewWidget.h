//
// Created by CZY on 2025/10/11.
//

#ifndef ROI_PREVIEW_WIDGET_H
#define ROI_PREVIEW_WIDGET_H

#include <QWidget>
#include <QPixmap>

// 识别区域预览控件：
// - 上传了参考图片时，按图片比例展示图片，并在其上叠加 ROI 区域；
// - 未上传图片时，按 16:9 画一个占位画布（代表游戏窗口）再叠加 ROI；
// ROI 使用百分比(0~100)，与 ScriptActions::ocrRecognizesAndClick 的语义一致。
class RoiPreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RoiPreviewWidget(QWidget *parent = nullptr);

    void setImage(const QPixmap &pix); // 设置参考图片；传空则回到 16:9 占位
    void clearImage();
    bool hasImage() const { return !image_.isNull(); }

    // 设置 ROI（单位：百分比 0~100），与识别逻辑一致
    void setRoiPercent(double x, double y, double w, double h);

    QSize sizeHint() const override { return QSize(268, 151); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // 代表游戏窗口的绘制区域（在控件内按比例居中的 letterbox 矩形）
    QRectF canvasRect() const;
    // 根据百分比在给定画布上计算 ROI（并裁剪到画布内），与识别逻辑保持一致
    QRectF roiRectIn(const QRectF &canvas) const;

    QPixmap image_;
    double roiX_ = 0.0;
    double roiY_ = 0.0;
    double roiW_ = 100.0;
    double roiH_ = 100.0;
};

#endif // ROI_PREVIEW_WIDGET_H
