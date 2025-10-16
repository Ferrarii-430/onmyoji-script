#include "ScreenCaptureWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>

ScreenCaptureWidget::ScreenCaptureWidget(QWidget *parent)
    : QWidget(nullptr), selecting(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::CrossCursor);

    screen = QGuiApplication::primaryScreen();
    if (screen) {
        fullPixmap = screen->grabWindow(0);
        fullPixmap.setDevicePixelRatio(screen->devicePixelRatio());
    }

    setGeometry(screen->geometry());
    show();
}

void ScreenCaptureWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, fullPixmap);

    if (selecting) {
        painter.setPen(QPen(Qt::red, 2));
        painter.setBrush(QColor(255, 0, 0, 50));
        painter.drawRect(QRect(startPoint, currentPoint).normalized());
    }
}

void ScreenCaptureWidget::mousePressEvent(QMouseEvent *event)
{
    selecting = true;
    startPoint = event->pos();
    currentPoint = startPoint;
    update();
}

void ScreenCaptureWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (selecting) {
        currentPoint = event->pos();
        update();
    }
}

void ScreenCaptureWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (!selecting) return;

    selecting = false;

    // 获取设备像素比
    qreal devicePixelRatio = screen->devicePixelRatio();

    // 坐标转换：逻辑坐标 → 物理坐标
    QPoint physicalStart = startPoint * devicePixelRatio;
    QPoint physicalEnd = event->pos() * devicePixelRatio;

    QRect selectedRect(physicalStart, physicalEnd);
    selectedRect = selectedRect.normalized();

    // 边界检查
    QRect availableRect(0, 0, fullPixmap.width(), fullPixmap.height());
    selectedRect = selectedRect.intersected(availableRect);

    if (selectedRect.isValid() && !selectedRect.isEmpty()) {
        captured = fullPixmap.copy(selectedRect);
        captured.setDevicePixelRatio(1.0); // 重置DPI

        emit captureFinished(captured);
    }

    close();
}
