//
// Created by CZY on 2025/9/30.
//

// You may need to build the project (run Qt uic code generator) to get "ui_TypeOpenCVForm.h" resolved

#include "typeopencvform.h"
#include <QJsonObject>
#include <QPushButton>
#include <QBuffer>
#include <QPainter>
#include <qscreen.h>
#include <QTimer>
#include "ui_TypeOpenCVForm.h"
#include "ScreenCaptureWidget.h"

TypeOpenCVForm::TypeOpenCVForm(QWidget *parent) :
    QWidget(parent), ui(new Ui::TypeOpenCVForm) {
    ui->setupUi(this);

    connect(ui->btnCapture, &QPushButton::clicked, this, &TypeOpenCVForm::onCaptureButtonClicked);
}

TypeOpenCVForm::~TypeOpenCVForm() {
    delete ui;
}

void TypeOpenCVForm::loadFromJson(const QJsonObject &obj)
{
    stepDataCopy = obj;
    ui->lineTaskNameEdit->setText(obj["taskName"].toString());
    ui->spinScoreBox->setValue(obj["score"].toDouble());
    ui->randomClickCheckBox->setChecked(obj["randomClick"].toBool());

    QString imagePath = obj["imagePath"].toString();
    if (!imagePath.isEmpty()) {
        QPixmap pixmap(imagePath);
        if (!pixmap.isNull()) {
            originalPixmap_ = pixmap;
            QTimer::singleShot(100, this, &TypeOpenCVForm::updatePreview);
        } else {
            ui->labelPreview->setText("图片加载失败: " + imagePath);
        }
    }
}

QJsonObject TypeOpenCVForm::toJson() const {
    if (!ui) {
        qWarning() << "ui 是空指针";
        return {};
    }

    QJsonObject obj;
    if (stepDataCopy.isEmpty())
    {
        obj["stepsId"] = QUuid::createUuid().toString(QUuid::WithoutBraces);  // UUID;
    }else
    {
        obj["stepsId"] = stepDataCopy["stepsId"]; //复制原始UUID
    }
    obj["type"] = "OPENCV";
    obj["taskName"] = ui->lineTaskNameEdit->text();
    obj["score"] = ui->spinScoreBox->value();
    obj["randomClick"] = ui->randomClickCheckBox->isChecked();

    // 如果 label 里有图像
    QPixmap pix = ui->labelPreview->pixmap();
    if (!pix.isNull()) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        pix.save(&buffer, "PNG");
        buffer.close();
        obj["image"] = QString::fromLatin1(bytes.toBase64());
    }
    return obj;
}

void TypeOpenCVForm::onCaptureButtonClicked() {
    ScreenCaptureWidget* sc = new ScreenCaptureWidget(nullptr); // 父窗口设 nullptr 避免阻塞
    sc->show();

    connect(sc, &ScreenCaptureWidget::captureFinished, this, [this, sc](const QPixmap &pix){
        originalPixmap_ = pix; // 保存原始截图
        updatePreview();       // 显示一次
        // qDebug() << "截图完成";
        sc->deleteLater();
    });
}

void TypeOpenCVForm::updatePreview() const
{
    if (originalPixmap_.isNull()) return;

    QLabel* label = ui->labelPreview;
    QSize labelSize = label->size();

    // 计算按比例缩放后的图片
    QPixmap scaled = originalPixmap_.scaled(
        labelSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    // 创建与label一样大的pixmap背景，并居中绘制scaled图
    QPixmap finalPixmap(labelSize);
    finalPixmap.fill(Qt::transparent);  // 背景透明或你可以用 Qt::black

    QPainter painter(&finalPixmap);
    QPoint center((labelSize.width() - scaled.width()) / 2,
                  (labelSize.height() - scaled.height()) / 2);
    painter.drawPixmap(center, scaled);
    painter.end();

    label->setPixmap(finalPixmap);
}