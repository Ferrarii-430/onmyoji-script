//
// Created by CZY on 2025/9/30.
//

// You may need to build the project (run Qt uic code generator) to get "ui_TypeOpenCVForm.h" resolved

#include "typeopencvform.h"
#include <QJsonObject>
#include <QPushButton>
#include <qscreen.h>
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

}

QJsonObject TypeOpenCVForm::toJson() const {
    QJsonObject obj;
    // obj["type"] = "OPENCV";
    // obj["taskName"] = ui->lineEditTaskName->text();
    // obj["threshold"] = ui->spinBoxThreshold->value();
    return obj;
}

void TypeOpenCVForm::onCaptureButtonClicked() {
    ScreenCaptureWidget* sc = new ScreenCaptureWidget(nullptr); // 父窗口设 nullptr 避免阻塞
    sc->show();

    connect(sc, &ScreenCaptureWidget::captureFinished, this, [this, sc](const QPixmap &pix){
        ui->labelPreview->setPixmap(pix.scaled(ui->labelPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        qDebug() << "截图完成";
        sc->deleteLater();
    });
}