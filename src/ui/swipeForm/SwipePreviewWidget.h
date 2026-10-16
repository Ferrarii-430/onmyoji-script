//
// Created by CZY on 2026/10/15.
//

#ifndef SWIPE_PREVIEW_WIDGET_H
#define SWIPE_PREVIEW_WIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPointF>

// 滑动两点预览控件：
// - 上传了参考图片时，按图片原始宽高比展示图片；未上传时按 16:9 画占位画布；
// - 在画布上绘制起点（按下）/终点（抬起）两个标记点及滑动方向箭头；
// - 点击画布任意位置即取该点坐标（百分比 0~100），通过 pointPicked 通知表单
//   同步到当前编辑的点，与 ScriptActions::swipeBetweenPoints 的语义一致。
class SwipePreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SwipePreviewWidget(QWidget *parent = nullptr);

    void setImage(const QPixmap &pix); // 设置参考图片；传空则回到 16:9 占位
    void clearImage();

    // 设置起止点（单位：百分比 0~100），与滑动逻辑一致
    void setPoint1Percent(double x, double y);
    void setPoint2Percent(double x, double y);

    // 设置当前编辑点（1=起点按下 / 2=终点抬起），点击画布时取点落到该点；
    // 当前编辑点在预览中高亮显示
    void setActivePoint(int index);

    QSize sizeHint() const override { return QSize(268, 151); }

signals:
    // 点击画布取点后发出（百分比 0~100，已对齐 1 位小数），
    // pointIndex 为当前编辑点（1=起点 / 2=终点），表单据此同步数值框
    void pointPicked(int pointIndex, double x, double y);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    // 代表游戏窗口的绘制区域（在控件内按比例居中的 letterbox 矩形）
    QRectF canvasRect() const;
    // 根据百分比在给定画布上计算标记点位置
    QPointF pointIn(const QPointF &percentPoint, const QRectF &canvas) const;
    // 绘制单个标记点（编号 + 高亮圈）
    void drawPointMarker(QPainter &painter, const QPointF &center,
                         int index, const QColor &color) const;

    QPixmap image_;
    QPointF point1_{25.0, 70.0}; // 起点（按下），默认左下区域
    QPointF point2_{75.0, 30.0}; // 终点（抬起），默认右上区域
    int activePoint_ = 1;        // 当前编辑点：1 或 2
};

#endif // SWIPE_PREVIEW_WIDGET_H
