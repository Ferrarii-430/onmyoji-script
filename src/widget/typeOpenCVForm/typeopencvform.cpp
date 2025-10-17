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
    ui->stepInputBox->hide(); // 初始状态隐藏
    ui->stepInputLabel->hide(); // 初始状态隐藏

    ui->spinScoreBox->setValue(0.55);

    ui->opencvErrorHandle->addItem("继续执行任务","next");
    ui->opencvErrorHandle->addItem("跳转步骤","jump");
    ui->opencvErrorHandle->addItem("跳过本次循环","continue");
    ui->opencvErrorHandle->addItem("停止执行任务","break");

    connect(ui->btnCapture, &QPushButton::clicked, this, &TypeOpenCVForm::onCaptureButtonClicked);

    connect(ui->opencvErrorHandle, &QComboBox::currentIndexChanged, this, [this](int index)
    {
        // 获取当前选中的用户数据
            int value = ui->opencvErrorHandle->currentData().toInt();

            // 当值为1时显示stepInput，其他值隐藏
            if (value == 1) {
                ui->stepInputBox->show();
                ui->stepInputLabel->show();
            } else {
                ui->stepInputBox->hide();
                ui->stepInputLabel->hide();
            }
    });
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
    ui->opencvErrorHandle->setCurrentIndex(obj["randomClick"].toBool());

    QString currentIdentifyErrorHandle= obj["identifyErrorHandle"].toString();
    int identifyErrorHandleIndex = ui->opencvErrorHandle->findData(currentIdentifyErrorHandle);
    if (identifyErrorHandleIndex >= 0) {
        ui->opencvErrorHandle->setCurrentIndex(identifyErrorHandleIndex);
    } else {
        // 如果配置值不在选项中，使用默认值
        ui->opencvErrorHandle->setCurrentIndex(0);
    }

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
    obj["identifyErrorHandle"] = ui->opencvErrorHandle->currentData().toString();

    //如果是跳转
    if (comparesEqual(obj["identifyErrorHandle"], "jump"))
    {
        obj["jumpStepsIndex"] = ui->stepInputBox->currentText().toInt();
    }

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