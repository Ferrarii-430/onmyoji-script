//
// Created by CZY on 2025/9/30.
//

// You may need to build the project (run Qt uic code generator) to get "ui_EditTaskDialog.h" resolved

#include "edittaskdialog.h"
#include <QJsonObject>
#include <QPushButton>
#include <../common/ConfigTypeEnum.h>

#include "ExecutionSteps.h"
#include "Logger.h"
#include "../typeOpenCVForm/typeopencvform.h"
#include "../waitForm/waitform.h"
#include "ui_EditTaskDialog.h"

EditTaskDialog::EditTaskDialog(EditMode mode, const QJsonObject &stepData, const QString &configId, QWidget *parent) :
    QDialog(parent), ui(new Ui::EditTaskDialog) {
    setWindowTitle(mode == EditMode::Add ? "新增任务" : "编辑任务");
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("保存");
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");

    QPushButton* testButton = ui->buttonBox->addButton(tr("测试"), QDialogButtonBox::ActionRole);

    ui->comboBox->addItem("OpenCV识图");
    ui->comboBox->addItem("等待");
    ui->comboBox->addItem("OCR识别");

    // 构造函数里
    typeForm = new TypeOpenCVForm(this);
    waitForm = new WaitForm(this);
    ocrForm = new OcrForm(this);

    ui->stackedWidget->addWidget(typeForm);  // index 0
    ui->stackedWidget->addWidget(waitForm);  // index 1
    ui->stackedWidget->addWidget(ocrForm);  // index 2

    connect(ui->comboBox, &QComboBox::currentIndexChanged, this, [this, testButton](int index){
        ui->stackedWidget->setCurrentIndex(index);
        if (index == 0) {
            testButton->setDisabled(false);
        } else if (index == 1) {
            testButton->setDisabled(true);
        } else if (index == 2) {
            testButton->setDisabled(false);
        }
    });

    // 初始化数据
    if (mode == EditMode::Edit && !stepData.isEmpty()) {
        QString type = stepData["type"].toString();
        if (type == "OPENCV") {
            ui->comboBox->setCurrentIndex(0);
            typeForm->loadFromJson(configId,stepData);
        } else if (type == "WAIT") {
            ui->comboBox->setCurrentIndex(1);
            waitForm->loadFromJson(configId,stepData);
        } else if (type == "OCR") {
            ui->comboBox->setCurrentIndex(2);
            ocrForm->loadFromJson(configId,stepData);
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

    connect(testButton, &QPushButton::clicked, this, &EditTaskDialog::onTestButtonClick);
}

QJsonObject EditTaskDialog::collectData() const {
    int index = ui->stackedWidget->currentIndex();
    if (index == 0) {
        return typeForm->toJson();
    } else if (index == 1) {
        return waitForm->toJson();
    } else if (index == 2) {
        return ocrForm->toJson();
    }
    return QJsonObject();
}

QJsonObject EditTaskDialog::resultData() const {
    return m_resultData;
}

EditTaskDialog::~EditTaskDialog() {
    delete ui;
}


bool isBase64(const QString &str) {
    if (str.isEmpty()) {
        return false;
    }

    QByteArray data;
    // 检查是否是数据 URI（以 "data:image/" 开头）
    if (str.startsWith("data:image/")) {
        // 查找 "base64," 部分（不区分大小写）
        QString lowerStr = str.toLower();
        int index = lowerStr.indexOf("base64,");
        if (index == -1) {
            return false; // 没有找到 "base64,"，不是有效的 base64 数据 URI
        }
        data = str.mid(index + 7).toUtf8(); // 提取 "base64," 之后的部分
    } else {
        data = str.toUtf8(); // 直接处理为纯 base64 字符串
    }

    // 尝试解码 base64
    QByteArray decoded = QByteArray::fromBase64(data, QByteArray::Base64Encoding);
    return !decoded.isEmpty(); // 解码成功则为 base64
}

void EditTaskDialog::onTestButtonClick()
{
    bool hasHWND = ExecutionSteps::getInstance().checkHWNDHandle();
    if (!hasHWND)
    {
        Logger::log(QString("窗口未找到"));
        return;
    }

    int index = ui->stackedWidget->currentIndex();
    if (index == 0) {
        //OpenCV
        QString savePath;
        QJsonObject json = typeForm->toJson();
        QString imagePath = json["image"].toString(); //此时还是image 保存到config文件后是imagePath
        const double score = json["score"].toDouble();
        const bool randomClick = json["randomClick"].toBool();
        if (isBase64(imagePath))
        {
            savePath  = ExecutionSteps::getInstance().opencvRecognizesAndClickByBase64(imagePath, score, randomClick);
        }else
        {
            savePath = ExecutionSteps::getInstance().opencvRecognizesAndClick(imagePath, score, randomClick);
        }
        if (savePath.isEmpty())
        {
            Logger::log(QString("测试OpenCV识别失败"));
            return;
        }
        emit imagePathRequested(savePath); // 发射信号
    } else if (index == 1) {
        //等待不用管
    } else if (index == 2) {
        //OCR
        QJsonObject json = ocrForm->toJson();
        QString ocrText = json["ocrText"].toString();
        const double score = json["score"].toDouble();
        const bool randomClick = json["randomClick"].toBool();
        QString savePath = ExecutionSteps::getInstance().ocrRecognizesAndClick(ocrText, score, randomClick);
        if (savePath.isEmpty())
        {
            Logger::log(QString("测试OCR识别失败"));
            return;
        }
        emit imagePathRequested(savePath); // 发射信号
    }
}