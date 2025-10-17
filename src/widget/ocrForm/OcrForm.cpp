//
// Created by CZY on 2025/10/11.
//

// You may need to build the project (run Qt uic code generator) to get "ui_temp.h" resolved

#include "OcrForm.h"

#include <QJsonObject>

#include "ui_OcrForm.h"
//TODO form窗体模板

OcrForm::OcrForm(QWidget *parent) :
    QWidget(parent), ui(new Ui::OcrForm) {
    ui->setupUi(this);
}

OcrForm::~OcrForm() {
    delete ui;
}

void OcrForm::loadFromJson(const QJsonObject &obj)
{
    stepDataCopy = obj;
    // ui->lineTaskNameEdit->setText(obj["taskName"].toString());
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
    obj["type"] = "TEMP";
    // obj["taskName"] = ui->lineEditTaskName->text();
    return obj;
}