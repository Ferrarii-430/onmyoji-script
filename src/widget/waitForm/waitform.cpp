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

    ui->randomSleepTimeSpinBox->hide(); // 初始状态隐藏
    ui->randomSleepTimeLabel->hide(); // 初始状态隐藏

    connect(ui->checkBox, &QCheckBox::checkStateChanged, this, [this](bool value)
    {
        if (value)
        {
            ui->randomSleepTimeSpinBox->show();
            ui->randomSleepTimeLabel->show();
        }else
        {
            ui->randomSleepTimeSpinBox->hide();
            ui->randomSleepTimeLabel->hide();
        }
    });
}

WaitForm::~WaitForm() {
    delete ui;
}

void WaitForm::loadFromJson(const QString &configId, const QJsonObject &obj)
{
    stepDataCopy = obj;
    ui->lineTaskNameEdit->setText(obj["taskName"].toString());
    ui->spinBox->setValue(obj["time"].toInt());
    ui->randomSleepTimeSpinBox->setValue(obj["offsetTime"].toInt());
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
    obj["offsetTime"] = ui->randomSleepTimeSpinBox->value();

    return obj;
}
