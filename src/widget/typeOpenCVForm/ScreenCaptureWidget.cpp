#include "ScreenCaptureWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QDebug>

ScreenCaptureWidget::ScreenCaptureWidget(QWidget *parent)
    : QWidget(nullptr), selecting(false)
{
    // 全屏显示
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::CrossCursor);

    screen = QGuiApplication::primaryScreen();
    if (screen)
        fullPixmap = screen->grabWindow(0); // 截全屏
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
    QRect selectedRect(startPoint, event->pos());
    selectedRect = selectedRect.normalized();
    captured = fullPixmap.copy(selectedRect);

    emit captureFinished(captured); // 发信号给外部
    close();
}
