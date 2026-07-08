//
// Created by CZY on 2025/10/11.
//

#include "RoiPreviewWidget.h"

#include <QColor>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QSizePolicy>
#include <QtGlobal>

RoiPreviewWidget::RoiPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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
    QRectF bounds = rect().adjusted(1, 1, -1, -1);
    if (bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return {};
    }

    const qreal aspect = (!image_.isNull() && image_.height() > 0)
                             ? qreal(image_.width()) / qreal(image_.height())
                             : 16.0 / 9.0;

    QRectF canvas = bounds;
    const qreal boundsAspect = bounds.width() / bounds.height();
    if (boundsAspect > aspect) {
        canvas.setWidth(bounds.height() * aspect);
    } else {
        canvas.setHeight(bounds.width() / aspect);
    }
    canvas.moveCenter(bounds.center());
    return canvas;
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
        painter.drawText(canvas, Qt::AlignCenter, tr("未上传图片\n按 16:9 预览识别区域"));
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
