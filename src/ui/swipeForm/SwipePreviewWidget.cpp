//
// Created by CZY on 2026/10/15.
//

#include "SwipePreviewWidget.h"

#include <QColor>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QSizePolicy>
#include <QtGlobal>

#include <cmath>

SwipePreviewWidget::SwipePreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // 点击画布取点，使用十字光标提示可选取坐标
    setCursor(Qt::CrossCursor);
}

void SwipePreviewWidget::setImage(const QPixmap &pix)
{
    image_ = pix;
    update();
}

void SwipePreviewWidget::clearImage()
{
    image_ = QPixmap();
    update();
}

void SwipePreviewWidget::setPoint1Percent(double x, double y)
{
    point1_ = QPointF(x, y);
    update();
}

void SwipePreviewWidget::setPoint2Percent(double x, double y)
{
    point2_ = QPointF(x, y);
    update();
}

void SwipePreviewWidget::setActivePoint(int index)
{
    activePoint_ = (index == 2) ? 2 : 1;
    update();
}

QRectF SwipePreviewWidget::canvasRect() const
{
    const QRectF bounds = rect().adjusted(1, 1, -1, -1);
    if (bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return {};
    }

    // 画布代表游戏窗口：上传了图片按图片原始宽高比，否则按 16:9。
    // 必须保持比例，否则百分比坐标在预览里的位置与实际截图区域不一致，会误导取点。
    double targetAspect = 16.0 / 9.0;
    if (!image_.isNull() && image_.height() > 0) {
        targetAspect = static_cast<double>(image_.width()) / image_.height();
    }

    // 在控件内按目标比例居中做 letterbox（多余方向留边）
    double w = bounds.width();
    double h = w / targetAspect;
    if (h > bounds.height()) {
        h = bounds.height();
        w = h * targetAspect;
    }

    return QRectF(bounds.x() + (bounds.width() - w) / 2.0,
                  bounds.y() + (bounds.height() - h) / 2.0,
                  w, h);
}

QPointF SwipePreviewWidget::pointIn(const QPointF &percentPoint, const QRectF &canvas) const
{
    return QPointF(canvas.x() + canvas.width() * percentPoint.x() / 100.0,
                   canvas.y() + canvas.height() * percentPoint.y() / 100.0);
}

void SwipePreviewWidget::drawPointMarker(QPainter &painter, const QPointF &center,
                                         const int index, const QColor &color) const
{
    constexpr double kRadius = 8.0;

    // 当前编辑点外圈白色高亮，便于区分正在设置哪个点
    if (index == activePoint_) {
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, kRadius + 3.0, kRadius + 3.0);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(center, kRadius, kRadius);

    painter.setPen(Qt::white);
    painter.drawText(QRectF(center.x() - kRadius, center.y() - kRadius,
                            kRadius * 2, kRadius * 2),
                     Qt::AlignCenter, QString::number(index));
}

void SwipePreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform,
                           true);
    painter.fillRect(rect(), QColor("#2b2b2b"));

    const QRectF canvas = canvasRect();
    if (canvas.isEmpty()) {
        return;
    }

    if (!image_.isNull()) {
        painter.drawPixmap(canvas, image_, image_.rect());
    } else {
        painter.fillRect(canvas, QColor("#3c3f41"));
        painter.setPen(QPen(QColor("#808080"), 1, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(canvas);

        painter.setPen(QColor("#b0b0b0"));
        painter.drawText(canvas, Qt::AlignCenter, tr("未上传图片"));
    }

    const QPointF p1 = pointIn(point1_, canvas);
    const QPointF p2 = pointIn(point2_, canvas);

    // 滑动方向箭头：起点 → 终点
    painter.setPen(QPen(QColor("#2ecc71"), 2, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(p1, p2);
    // 箭头头部：按方向画两条短线，距终点留出标记点半径避免被遮挡
    const double dx = p2.x() - p1.x();
    const double dy = p2.y() - p1.y();
    const double len = std::hypot(dx, dy);
    if (len > 1.0) {
        constexpr double kHeadLen = 10.0;
        constexpr double kHeadAngle = 0.5; // 弧度，约 28.6°
        const double ux = dx / len;
        const double uy = dy / len;
        const QPointF tip = p2 - QPointF(ux, uy) * 10.0;
        const QPointF head1(tip.x() - (ux * std::cos(kHeadAngle) - uy * std::sin(kHeadAngle)) * kHeadLen,
                            tip.y() - (uy * std::cos(kHeadAngle) + ux * std::sin(kHeadAngle)) * kHeadLen);
        const QPointF head2(tip.x() - (ux * std::cos(kHeadAngle) + uy * std::sin(kHeadAngle)) * kHeadLen,
                            tip.y() - (uy * std::cos(kHeadAngle) - ux * std::sin(kHeadAngle)) * kHeadLen);
        painter.drawLine(tip, head1);
        painter.drawLine(tip, head2);
    }

    drawPointMarker(painter, p1, 1, QColor("#e74c3c")); // 起点：红
    drawPointMarker(painter, p2, 2, QColor("#3498db")); // 终点：蓝
}

void SwipePreviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QRectF canvas = canvasRect();
    if (canvas.isEmpty()) {
        return;
    }

    // 点击落在画布内才算取点；把位置换算为百分比并对齐 1 位小数，
    // 与表单 QDoubleSpinBox(decimals=1) 的取值精度保持一致
    if (!canvas.contains(event->position())) {
        return;
    }

    double x = (event->position().x() - canvas.x()) / canvas.width() * 100.0;
    double y = (event->position().y() - canvas.y()) / canvas.height() * 100.0;
    x = std::round(x * 10.0) / 10.0;
    y = std::round(y * 10.0) / 10.0;
    x = qBound(0.0, x, 100.0);
    y = qBound(0.0, y, 100.0);

    emit pointPicked(activePoint_, x, y);
    event->accept();
}
