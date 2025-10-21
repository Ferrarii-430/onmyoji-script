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
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        selecting = true;
        startPoint = event->pos();
        currentPoint = startPoint;

        // 最小化窗口避免遮挡
        showMinimized();
        update();
    }
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

    if (event->button() == Qt::RightButton) {
        // 右键释放：返回空图片
        emit captureFinished(QPixmap());
    } else if (event->button() == Qt::LeftButton) {
        // 左键释放：正常截图
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
        } else {
            emit captureFinished(QPixmap()); // 无效区域返回空图片
        }
    }

    close();
}

// 可选：添加键盘事件处理，按ESC也可取消
void ScreenCaptureWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit captureFinished(QPixmap());
        close();
    } else {
        QWidget::keyPressEvent(event);
    }
}