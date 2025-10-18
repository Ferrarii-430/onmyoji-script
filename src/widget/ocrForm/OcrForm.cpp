//
// Created by CZY on 2025/10/11.
//

// You may need to build the project (run Qt uic code generator) to get "ui_temp.h" resolved

#include "OcrForm.h"

#include <QJsonObject>

#include "Logger.h"
#include "ui_OcrForm.h"
//TODO form窗体模板

OcrForm::OcrForm(QWidget *parent) :
    QWidget(parent), ui(new Ui::OcrForm) {
    ui->setupUi(this);

    ui->stepInputBox->hide(); // 初始状态隐藏
    ui->stepInputLabel->hide(); // 初始状态隐藏

    ui->opencvErrorHandle->addItem("继续执行任务","next");
    ui->opencvErrorHandle->addItem("跳转步骤","jump");
    ui->opencvErrorHandle->addItem("跳过本次循环","continue");
    ui->opencvErrorHandle->addItem("停止执行任务","break");

    ui->spinScoreBox->setValue(0.55);

    connect(ui->opencvErrorHandle, &QComboBox::currentIndexChanged, this, [this](int index)
    {
        // 当值为1时显示stepInput，其他值隐藏
        if (index == 1) {
            ui->stepInputBox->show();
            ui->stepInputLabel->show();
        } else {
            ui->stepInputBox->hide();
            ui->stepInputLabel->hide();
        }
    });
}

OcrForm::~OcrForm() {
    delete ui;
}

void OcrForm::loadFromJson(const QJsonObject &obj)
{
    stepDataCopy = obj;
    ui->lineTaskNameEdit->setText(obj["taskName"].toString());
    ui->ocrTextEdit->setText(obj["ocrText"].toString());
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
}

QJsonObject OcrForm::toJson() const {
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
    obj["type"] = "OCR";
    obj["taskName"] = ui->lineTaskNameEdit->text();
    obj["ocrText"] = ui->ocrTextEdit->text();
    obj["score"] = ui->spinScoreBox->value();
    obj["randomClick"] = ui->randomClickCheckBox->isChecked();
    obj["identifyErrorHandle"] = ui->opencvErrorHandle->currentData().toString();

    //如果是跳转
    if (comparesEqual(obj["identifyErrorHandle"], "jump"))
    {
        obj["jumpStepsIndex"] = ui->stepInputBox->currentText().toInt();
    }
    return obj;
}