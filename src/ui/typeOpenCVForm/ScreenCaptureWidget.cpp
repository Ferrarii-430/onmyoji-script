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
    }

    setGeometry(screen->geometry());
    show();
}

void ScreenCaptureWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, fullPixmap);

    if (selecting) {
        const QRect rect = QRect(startPoint, currentPoint).normalized();
        painter.setPen(QPen(Qt::red, 2));
        painter.setBrush(QColor(255, 0, 0, 50));
        painter.drawRect(rect);

        // 绘制时实时显示选区位置与尺寸（物理像素，与最终生成图片一致）
        const qreal dpr = fullPixmap.devicePixelRatio();
        const QRect phys(QPoint(qRound(rect.left() * dpr), qRound(rect.top() * dpr)),
                         QSize(qRound(rect.width() * dpr), qRound(rect.height() * dpr)));
        const QString info = QString("(%1, %2)  %3 x %4")
                                 .arg(phys.x()).arg(phys.y())
                                 .arg(phys.width()).arg(phys.height());

        const QFontMetrics fm = painter.fontMetrics();
        QRect textRect = fm.boundingRect(info).adjusted(-6, -3, 6, 3);
        // 默认显示在选区左上角上方，靠近屏幕顶部时改到选区内部下方
        QPoint textPos(rect.left(), rect.top() - textRect.height() - 4);
        if (textPos.y() < 0) textPos.setY(rect.top() + 4);
        if (textPos.x() + textRect.width() > width()) textPos.setX(width() - textRect.width());
        if (textPos.x() < 0) textPos.setX(0);
        textRect.moveTopLeft(textPos);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 160));
        painter.drawRect(textRect);
        painter.setPen(Qt::white);
        painter.drawText(textRect, Qt::AlignCenter, info);
    }
}

void ScreenCaptureWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        selecting = true;
        startPoint = event->pos();
        currentPoint = startPoint;
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
        qreal devicePixelRatio = fullPixmap.devicePixelRatio();

        // 坐标转换：逻辑坐标 → 物理坐标
        currentPoint = event->pos();
        QPoint physicalStart = startPoint * devicePixelRatio;
        QPoint physicalEnd = currentPoint * devicePixelRatio;

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