#pragma once
#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QScreen>
#include <QGuiApplication>

class ScreenCaptureWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ScreenCaptureWidget(QWidget *parent = nullptr);

    QPixmap getCaptured() const { return captured; }

    signals:
        void captureFinished(const QPixmap &pix);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    bool selecting;
    QPoint startPoint;
    QPoint currentPoint;
    QPixmap fullPixmap;
    QPixmap captured;
    QScreen* screen;
};
