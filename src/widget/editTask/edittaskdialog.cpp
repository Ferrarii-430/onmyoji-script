//
// Created by CZY on 2025/9/30.
//

// You may need to build the project (run Qt uic code generator) to get "ui_EditTaskDialog.h" resolved

#include "edittaskdialog.h"
#include <QJsonObject>
#include <../common/ConfigTypeEnum.h>
#include "../typeOpenCVForm/typeopencvform.h"
#include "../waitForm/waitform.h"
#include "ui_EditTaskDialog.h"

EditTaskDialog::EditTaskDialog(EditMode mode, const QJsonObject &stepData, QWidget *parent) :
    QDialog(parent), ui(new Ui::EditTaskDialog) {
    setWindowTitle(mode == EditMode::Add ? "新增任务" : "编辑任务");
    ui->setupUi(this);

    ui->comboBox->addItem("OpenCV识图");
    ui->comboBox->addItem("等待");

    // 构造函数里
    typeForm = new TypeOpenCVForm(this);
    waitForm = new WaitForm(this);

    ui->stackedWidget->addWidget(typeForm);  // index 0
    ui->stackedWidget->addWidget(waitForm);  // index 1

    // 假设 comboBox 有两个选项： "OpenCV" 和 "等待"
    connect(ui->comboBox, &QComboBox::currentIndexChanged, this, [this](int index){
        if (index == 0) {
            ui->stackedWidget->setCurrentIndex(0);
        } else if (index == 1) {
            ui->stackedWidget->setCurrentIndex(1);
        }
    });

    // 初始化数据
    if (mode == EditMode::Edit && !stepData.isEmpty()) {
        QString type = stepData["type"].toString();
        if (type == "OPENCV") {
            ui->comboBox->setCurrentIndex(0);
            typeForm->loadFromJson(stepData);
        } else if (type == "WAIT") {
            ui->comboBox->setCurrentIndex(1);
            waitForm->loadFromJson(stepData);
        }
    } else {
        // 默认新增时显示第一个
        ui->comboBox->setCurrentIndex(0);
    }

    // 保存按钮
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        m_resultData = collectData();
        accept();  // 关闭对话框，返回 QDialog::Accepted
    });

    // 取消按钮
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, [this]() {
        reject();
    });
}

QJsonObject EditTaskDialog::collectData() const {
    int index = ui->stackedWidget->currentIndex();
    if (index == 0) {
        return typeForm->toJson();
    } else if (index == 1) {
        return waitForm->toJson();
    }
    return QJsonObject();
}

QJsonObject EditTaskDialog::resultData() const {
    return m_resultData;
}

EditTaskDialog::~EditTaskDialog() {
    delete ui;
}


// void EditTaskDialog::setCurrentIndex()
// {
//
// }