//
// Created by CZY on 2026/9/10.
//

#include "RoiClickForm.h"

#include <QFileDialog>
#include <QJsonObject>
#include <QMessageBox>
#include <QUuid>
#include <opencv2/imgproc.hpp>

#include "src/core/Logger.h"
#include "ui_RoiClickForm.h"
#include "src/core/ProfileStore.h"
#include "src/game/GameWindow.h"
#include "src/game/capture/CaptureService.h"

RoiClickForm::RoiClickForm(QWidget *parent) :
    QWidget(parent), ui(new Ui::RoiClickForm) {
    ui->setupUi(this);

    ui->stepInputBox->hide(); // 初始状态隐藏
    ui->stepInputLabel->hide(); // 初始状态隐藏

    ui->errorHandleBox->addItem("继续执行任务","next");
    ui->errorHandleBox->addItem("跳转步骤","jump");
    ui->errorHandleBox->addItem("跳过本次循环","continue");
    ui->errorHandleBox->addItem("停止执行任务","break");
    ui->errorHandleBox->addItem("重试","retry");
    ui->errorHandleBox->setCurrentIndex(ui->errorHandleBox->findData("break"));

    connect(ui->btnUploadImage, &QToolButton::clicked, this, &RoiClickForm::onUploadImageClicked);
    connect(ui->btnCaptureImage, &QToolButton::clicked, this, &RoiClickForm::onCaptureImageClicked);
    connect(ui->roiXBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RoiClickForm::updateRoiPreview);
    connect(ui->roiYBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RoiClickForm::updateRoiPreview);
    connect(ui->roiWBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RoiClickForm::updateRoiPreview);
    connect(ui->roiHBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RoiClickForm::updateRoiPreview);

    connect(ui->errorHandleBox, &QComboBox::currentIndexChanged, this, [this](int index)
    {
        // 当值为1时显示stepInput，其他值隐藏
        if (index == 1) {
            initStepInputBoxSelect(currentConfigId,stepDataCopy["stepsId"].toString());
            ui->stepInputBox->show();
            ui->stepInputLabel->show();
        } else {
            ui->stepInputBox->hide();
            ui->stepInputLabel->hide();
        }
    });

    updateRoiPreview();
}

RoiClickForm::~RoiClickForm() {
    delete ui;
}

void RoiClickForm::loadFromJson(const QString &configId, const QJsonObject &obj)
{
    currentConfigId = configId;
    stepDataCopy = obj;
    ui->lineTaskNameEdit->setText(obj["taskName"].toString());
    ui->randomClickCheckBox->setChecked(obj["randomClick"].toBool(true));

    // 点击区域（百分比）；旧配置无该字段时默认整张图片
    ui->roiXBox->setValue(obj["clickRoiX"].toDouble(0.0));
    ui->roiYBox->setValue(obj["clickRoiY"].toDouble(0.0));
    ui->roiWBox->setValue(obj["clickRoiW"].toDouble(100.0));
    ui->roiHBox->setValue(obj["clickRoiH"].toDouble(100.0));
    updateRoiPreview();

    QString currentIdentifyErrorHandle = obj["identifyErrorHandle"].toString();
    int identifyErrorHandleIndex = ui->errorHandleBox->findData(currentIdentifyErrorHandle);
    if (identifyErrorHandleIndex >= 0) {
        ui->errorHandleBox->setCurrentIndex(identifyErrorHandleIndex);
        if (identifyErrorHandleIndex == 1)
        {
            initStepInputBoxSelect(configId, obj["stepsId"].toString());
            const QString targetId = obj["jumpStepsId"].toString().trimmed();
            const int stepInputBoxIndex = ui->stepInputBox->findData(targetId);
            if (stepInputBoxIndex != -1) {
                ui->stepInputBox->setCurrentIndex(stepInputBoxIndex);
            }
        }
    } else {
        // 如果配置值不在选项中，使用默认值（停止执行任务）
        ui->errorHandleBox->setCurrentIndex(ui->errorHandleBox->findData("break"));
    }
}

void RoiClickForm::updateRoiPreview()
{
    if (!ui || !ui->roiPreview) {
        return;
    }

    ui->roiPreview->setRoiPercent(ui->roiXBox->value(),
                                  ui->roiYBox->value(),
                                  ui->roiWBox->value(),
                                  ui->roiHBox->value());
}

void RoiClickForm::onUploadImageClicked()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("选择参考图片"),
        QString(),
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp)"));
    if (fileName.isEmpty()) {
        return;
    }

    const QPixmap pix(fileName);
    if (pix.isNull()) {
        QMessageBox::warning(this, tr("打开失败"), tr("无法加载所选图片。"));
        return;
    }

    ui->roiPreview->setImage(pix);
    updateRoiPreview();
}

void RoiClickForm::onCaptureImageClicked()
{
    if (!GameWindow::instance().locate()) {
        QMessageBox::warning(this, tr("截图失败"), tr("未找到游戏窗口，请先启动游戏。"));
        return;
    }

    const cv::Mat winImg = capture::captureGameWindow();
    if (winImg.empty()) {
        QMessageBox::warning(this, tr("截图失败"), tr("获取游戏画面失败，请检查截图模式设置。"));
        return;
    }

    // 与点击链路使用同一份截图，因此预览比例即实际点击区域比例
    cv::Mat rgb;
    cv::cvtColor(winImg, rgb, cv::COLOR_BGR2RGB);
    const QImage qimg(rgb.data, rgb.cols, rgb.rows,
                      static_cast<int>(rgb.step), QImage::Format_RGB888);

    // QImage 未拷贝 rgb 的数据，转成 QPixmap 前必须深拷贝，避免 rgb 析构后悬垂
    ui->roiPreview->setImage(QPixmap::fromImage(qimg.copy()));
    updateRoiPreview();
    Logger::log(QString("已获取游戏截图作为预览底图：%1x%2").arg(winImg.cols).arg(winImg.rows));
}

void RoiClickForm::initStepInputBoxSelect(QString configId, const QString &stepsId)
{
    if (configId.isEmpty())
    {
        configId = currentItem.id;
    }

    if (stepSelect.empty())
    {
        stepSelect = getStepsSelect(configId, stepsId);
    }

    if (!stepSelect.empty())
    {
        ui->stepInputBox->clear();
        for (auto it = stepSelect.cbegin(); it != stepSelect.cend(); ++it) {
            ui->stepInputBox->addItem(it.key(), it.value());
        }
    }
}

QJsonObject RoiClickForm::toJson() const {
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
    obj["type"] = "CLICK_ROI";
    obj["taskName"] = ui->lineTaskNameEdit->text();
    obj["randomClick"] = ui->randomClickCheckBox->isChecked();
    obj["clickRoiX"] = ui->roiXBox->value();
    obj["clickRoiY"] = ui->roiYBox->value();
    obj["clickRoiW"] = ui->roiWBox->value();
    obj["clickRoiH"] = ui->roiHBox->value();
    obj["identifyErrorHandle"] = ui->errorHandleBox->currentData().toString();

    //如果是跳转
    if (comparesEqual(obj["identifyErrorHandle"].toString(), "jump"))
    {
        obj["jumpStepsId"] = ui->stepInputBox->currentData().toString();
    }else
    {
        obj["jumpStepsId"] = QJsonValue::Null;
    }
    return obj;
}
