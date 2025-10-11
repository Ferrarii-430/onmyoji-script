//
// Created by CZY on 2025/9/30.
//

// You may need to build the project (run Qt uic code generator) to get "ui_waitForm.h" resolved

#include "waitform.h"

#include <QJsonObject>

#include "ui_waitForm.h"


WaitForm::WaitForm(QWidget *parent) :
    QWidget(parent), ui(new Ui::WaitForm) {
    ui->setupUi(this);
}

WaitForm::~WaitForm() {
    delete ui;
}

void WaitForm::loadFromJson(const QJsonObject &obj)
{
    stepDataCopy = obj;
    ui->lineTaskNameEdit->setText(obj["taskName"].toString());
    ui->spinBox->setValue(obj["time"].toInt());
}

QJsonObject WaitForm::toJson() const {
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
    obj["type"] = "WAIT";
    obj["taskName"] = ui->lineTaskNameEdit->text();
    obj["time"] = ui->spinBox->value();

    return obj;
}
