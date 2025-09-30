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

}

QJsonObject WaitForm::toJson() const {
    QJsonObject obj;
    // obj["type"] = "OPENCV";
    // obj["taskName"] = ui->lineEditTaskName->text();
    // obj["threshold"] = ui->spinBoxThreshold->value();
    return obj;
}
