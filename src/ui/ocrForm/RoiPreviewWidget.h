//
// Created by CZY on 2025/10/11.
//

#ifndef ROI_PREVIEW_WIDGET_H
#define ROI_PREVIEW_WIDGET_H

#include <QWidget>
#include <QPixmap>

// 识别区域预览控件：
// - 上传了参考图片时，按图片原始宽高比展示图片，并在其上叠加 ROI 区域；
// - 未上传图片时，按 16:9 画一个占位画布（代表游戏窗口）再叠加 ROI；
// 画布始终保持比例（多余方向留边），否则 ROI 百分比框出的形状会与实际截图区域不一致。
// ROI 使用百分比(0~100)，与 ScriptActions::ocrRecognizesAndClick 的语义一致。
// 支持鼠标交互：框内拖拽移动，边框/角点拖拽缩放；变化通过 roiChanged 信号通知表单。
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

signals:
    // 鼠标拖拽修改 ROI 后发出（百分比 0~100），表单据此同步数值框
    void roiChanged(double x, double y, double w, double h);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // 拖拽交互模式
    enum class DragMode {
        None, Move,
        TopLeft, Top, TopRight,
        Right, BottomRight, Bottom, BottomLeft, Left
    };

    // 代表游戏窗口的绘制区域（在控件内按比例居中的 letterbox 矩形）
    QRectF canvasRect() const;
    // 根据百分比在给定画布上计算 ROI（并裁剪到画布内），与识别逻辑保持一致
    QRectF roiRectIn(const QRectF &canvas) const;
    // 判断鼠标位置命中的拖拽目标（角点/边框/内部），未命中返回 None
    DragMode hitTest(const QPointF &pos, const QRectF &canvas, const QRectF &roi) const;

    QPixmap image_;
    double roiX_ = 0.0;
    double roiY_ = 0.0;
    double roiW_ = 100.0;
    double roiH_ = 100.0;

    DragMode dragMode_ = DragMode::None; // 当前拖拽模式
    QPointF pressPos_;                   // 按下时的鼠标位置
    double pressX_ = 0.0;                // 按下时的 ROI 百分比（拖拽基准）
    double pressY_ = 0.0;
    double pressW_ = 100.0;
    double pressH_ = 100.0;
};

#endif // ROI_PREVIEW_WIDGET_H
