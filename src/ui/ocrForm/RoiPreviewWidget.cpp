//
// Created by CZY on 2025/10/11.
//

#include "RoiPreviewWidget.h"

#include <QColor>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QSizePolicy>
#include <QtGlobal>

#include <cmath>

namespace {
constexpr double kEdgeHitPx = 5.0; // 边框/角点命中容差（控件像素）
constexpr double kMinRoiPx = 8.0;  // ROI 最小宽高（控件像素），防止拖成一团
}

RoiPreviewWidget::RoiPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true); // 无按键悬停也接收 move 事件，用于切换光标
}

void RoiPreviewWidget::setImage(const QPixmap &pix)
{
    image_ = pix;
    update();
}

void RoiPreviewWidget::clearImage()
{
    image_ = QPixmap();
    update();
}

void RoiPreviewWidget::setRoiPercent(double x, double y, double w, double h)
{
    roiX_ = x;
    roiY_ = y;
    roiW_ = w;
    roiH_ = h;
    update();
}

QRectF RoiPreviewWidget::canvasRect() const
{
    const QRectF bounds = rect().adjusted(1, 1, -1, -1);
    if (bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return {};
    }

    // 画布代表被识别的那张图：上传了图片按图片原始宽高比，否则按游戏窗口的 16:9。
    // 必须保持比例，否则 ROI 百分比在预览里的形状与实际截图区域不一致，会误导框选。
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

QRectF RoiPreviewWidget::roiRectIn(const QRectF &canvas) const
{
    if (roiW_ <= 0.0 || roiH_ <= 0.0) {
        return canvas;
    }

    const QRectF roi(canvas.x() + canvas.width() * roiX_ / 100.0,
                     canvas.y() + canvas.height() * roiY_ / 100.0,
                     canvas.width() * roiW_ / 100.0,
                     canvas.height() * roiH_ / 100.0);
    return roi.intersected(canvas);
}

RoiPreviewWidget::DragMode RoiPreviewWidget::hitTest(const QPointF &pos,
                                                     const QRectF &canvas,
                                                     const QRectF &roi) const
{
    if (canvas.isEmpty() || !canvas.contains(pos)) {
        return DragMode::None;
    }

    // 宽或高 <= 0 视为「整张画布」，按画布矩形参与命中
    const QRectF r = (roiW_ <= 0.0 || roiH_ <= 0.0) ? canvas : roi;

    const bool nearL = std::abs(pos.x() - r.left()) <= kEdgeHitPx;
    const bool nearR = std::abs(pos.x() - r.right()) <= kEdgeHitPx;
    const bool nearT = std::abs(pos.y() - r.top()) <= kEdgeHitPx;
    const bool nearB = std::abs(pos.y() - r.bottom()) <= kEdgeHitPx;

    if (nearL && nearT) return DragMode::TopLeft;
    if (nearR && nearT) return DragMode::TopRight;
    if (nearL && nearB) return DragMode::BottomLeft;
    if (nearR && nearB) return DragMode::BottomRight;
    if (nearL) return DragMode::Left;
    if (nearR) return DragMode::Right;
    if (nearT) return DragMode::Top;
    if (nearB) return DragMode::Bottom;
    if (r.contains(pos)) return DragMode::Move;
    return DragMode::None;
}

void RoiPreviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QRectF canvas = canvasRect();
    if (canvas.isEmpty()) {
        return;
    }

    const DragMode hit = hitTest(event->position(), canvas, roiRectIn(canvas));
    if (hit == DragMode::None) {
        return;
    }

    dragMode_ = hit;
    pressPos_ = event->position();
    pressX_ = roiX_;
    pressY_ = roiY_;
    pressW_ = roiW_;
    pressH_ = roiH_;
    event->accept();
}

void RoiPreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    const QRectF canvas = canvasRect();
    if (canvas.isEmpty()) {
        return;
    }

    if (dragMode_ == DragMode::None) {
        // 悬停时按命中位置切换光标，提示可拖拽的方向
        switch (hitTest(event->position(), canvas, roiRectIn(canvas))) {
        case DragMode::TopLeft:
        case DragMode::BottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case DragMode::TopRight:
        case DragMode::BottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case DragMode::Top:
        case DragMode::Bottom:
            setCursor(Qt::SizeVerCursor);
            break;
        case DragMode::Left:
        case DragMode::Right:
            setCursor(Qt::SizeHorCursor);
            break;
        case DragMode::Move:
            setCursor(Qt::SizeAllCursor);
            break;
        default:
            unsetCursor();
            break;
        }
        return;
    }

    // 拖拽中：把位移换算为画布百分比，相对按下时的 ROI 基准值计算新位置
    const double dxPct = (event->position().x() - pressPos_.x()) / canvas.width() * 100.0;
    const double dyPct = (event->position().y() - pressPos_.y()) / canvas.height() * 100.0;
    const double minWPct = kMinRoiPx / canvas.width() * 100.0;
    const double minHPct = kMinRoiPx / canvas.height() * 100.0;

    double x = pressX_;
    double y = pressY_;
    double w = pressW_;
    double h = pressH_;
    if (w <= 0.0 || h <= 0.0) {
        // 「整张画布」状态首次拖拽时落到具体数值
        x = 0.0;
        y = 0.0;
        w = 100.0;
        h = 100.0;
    }

    const bool dragLeft = dragMode_ == DragMode::Left || dragMode_ == DragMode::TopLeft
                          || dragMode_ == DragMode::BottomLeft;
    const bool dragRight = dragMode_ == DragMode::Right || dragMode_ == DragMode::TopRight
                           || dragMode_ == DragMode::BottomRight;
    const bool dragTop = dragMode_ == DragMode::Top || dragMode_ == DragMode::TopLeft
                         || dragMode_ == DragMode::TopRight;
    const bool dragBottom = dragMode_ == DragMode::Bottom || dragMode_ == DragMode::BottomLeft
                            || dragMode_ == DragMode::BottomRight;

    if (dragMode_ == DragMode::Move) {
        x = qBound(0.0, x + dxPct, 100.0 - w);
        y = qBound(0.0, y + dyPct, 100.0 - h);
    } else {
        if (dragLeft) {
            const double right = x + w;
            const double left = qBound(0.0, x + dxPct, right - minWPct);
            x = left;
            w = right - left;
        } else if (dragRight) {
            w = qBound(minWPct, w + dxPct, 100.0 - x);
        }
        if (dragTop) {
            const double bottom = y + h;
            const double top = qBound(0.0, y + dyPct, bottom - minHPct);
            y = top;
            h = bottom - top;
        } else if (dragBottom) {
            h = qBound(minHPct, h + dyPct, 100.0 - y);
        }
    }

    // 对齐到 1 位小数，与表单 QDoubleSpinBox(decimals=1) 的取值精度保持一致
    x = std::round(x * 10.0) / 10.0;
    y = std::round(y * 10.0) / 10.0;
    w = std::round(w * 10.0) / 10.0;
    h = std::round(h * 10.0) / 10.0;
    x = qBound(0.0, x, 100.0);
    y = qBound(0.0, y, 100.0);
    w = qBound(0.0, w, 100.0 - x);
    h = qBound(0.0, h, 100.0 - y);

    if (qFuzzyCompare(x, roiX_) && qFuzzyCompare(y, roiY_)
        && qFuzzyCompare(w, roiW_) && qFuzzyCompare(h, roiH_)) {
        return;
    }

    setRoiPercent(x, y, w, h);
    emit roiChanged(x, y, w, h);
    event->accept();
}

void RoiPreviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && dragMode_ != DragMode::None) {
        dragMode_ = DragMode::None;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void RoiPreviewWidget::leaveEvent(QEvent *event)
{
    if (dragMode_ == DragMode::None) {
        unsetCursor();
    }
    QWidget::leaveEvent(event);
}

void RoiPreviewWidget::paintEvent(QPaintEvent *)
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

    const QRectF roi = roiRectIn(canvas);
    QPainterPath canvasPath;
    canvasPath.addRect(canvas);
    QPainterPath roiPath;
    roiPath.addRect(roi);
    painter.fillPath(canvasPath.subtracted(roiPath), QColor(0, 0, 0, 120));

    painter.setPen(QPen(QColor("#2ecc71"), 2));
    painter.setBrush(QColor(46, 204, 113, 40));
    painter.drawRect(roi);

    const bool wholeImage = roiW_ <= 0.0 || roiH_ <= 0.0;
    QString info;
    if (wholeImage) {
        info = tr("识别整张图片");
    } else if (!image_.isNull()) {
        const int x = qRound(roiX_ * image_.width() / 100.0);
        const int y = qRound(roiY_ * image_.height() / 100.0);
        const int w = qRound(roiW_ * image_.width() / 100.0);
        const int h = qRound(roiH_ * image_.height() / 100.0);
        info = QStringLiteral("(%1, %2)  %3 x %4 px").arg(x).arg(y).arg(w).arg(h);
    } else {
        info = QStringLiteral("左上(%1%, %2%)  宽高 %3% x %4%").arg(roiX_).arg(roiY_).arg(roiW_).arg(roiH_);
    }

    if (!info.isEmpty()) {
        const QFontMetrics fm(painter.font());
        QRect textRect = fm.boundingRect(info).adjusted(-8, -5, 8, 5);
        QPoint topLeft(qRound(roi.left()), qRound(roi.top()) - textRect.height() - 6);
        if (topLeft.y() < 0) {
            topLeft.setY(qRound(roi.bottom()) + 6);
        }
        if (topLeft.x() + textRect.width() > width()) {
            topLeft.setX(width() - textRect.width());
        }
        if (topLeft.x() < 0) {
            topLeft.setX(0);
        }
        if (topLeft.y() + textRect.height() > height()) {
            topLeft.setY(height() - textRect.height());
        }
        if (topLeft.y() < 0) {
            topLeft.setY(0);
        }
        textRect.moveTopLeft(topLeft);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 170));
        painter.drawRoundedRect(textRect, 4, 4);
        painter.setPen(Qt::white);
        painter.drawText(textRect, Qt::AlignCenter, info);
    }
}
