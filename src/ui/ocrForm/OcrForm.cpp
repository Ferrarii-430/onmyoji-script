//
// Created by CZY on 2025/10/11.
//

// You may need to build the project (run Qt uic code generator) to get "ui_temp.h" resolved

#include "OcrForm.h"

#include <QFileDialog>
#include <QJsonObject>
#include <QMessageBox>
#include <opencv2/imgproc.hpp>

#include "src/core/Logger.h"
#include "ui_OcrForm.h"
#include "src/core/ProfileStore.h"
#include "src/engine/ScriptActions.h"
#include "src/game/GameWindow.h"
#include "src/game/capture/CaptureService.h"
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
    ui->opencvErrorHandle->addItem("重试","retry");
    ui->opencvErrorHandle->setCurrentIndex(ui->opencvErrorHandle->findData("break"));

    ui->spinScoreBox->setValue(0.55);

    connect(ui->btnUploadImage, &QToolButton::clicked, this, &OcrForm::onUploadImageClicked);
    connect(ui->btnCaptureImage, &QToolButton::clicked, this, &OcrForm::onCaptureImageClicked);
    connect(ui->roiXBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OcrForm::updateRoiPreview);
    connect(ui->roiYBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OcrForm::updateRoiPreview);
    connect(ui->roiWBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OcrForm::updateRoiPreview);
    connect(ui->roiHBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OcrForm::updateRoiPreview);

    connect(ui->opencvErrorHandle, &QComboBox::currentIndexChanged, this, [this](int index)
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

OcrForm::~OcrForm() {
    delete ui;
}

void OcrForm::loadFromJson(const QString &configId, const QJsonObject &obj)
{
    currentConfigId = configId;
    stepDataCopy = obj;
    ui->lineTaskNameEdit->setText(obj["taskName"].toString());
    ui->ocrTextEdit->setText(obj["ocrText"].toString());
    ui->spinScoreBox->setValue(obj["score"].toDouble());
    ui->randomClickCheckBox->setChecked(obj["randomClick"].toBool());

    // 识别区域（百分比）；旧配置无该字段时默认整张图片
    ui->roiXBox->setValue(obj["ocrRoiX"].toDouble(0.0));
    ui->roiYBox->setValue(obj["ocrRoiY"].toDouble(0.0));
    ui->roiWBox->setValue(obj["ocrRoiW"].toDouble(100.0));
    ui->roiHBox->setValue(obj["ocrRoiH"].toDouble(100.0));
    updateRoiPreview();

    // 识别增强开关。旧配置无该字段时按「仅放大」处理：小裁剪图自动放大是本就存在的
    // 行为，若默认成 0 会让已有的 OCR 步骤失去放大，破坏旧逻辑。
    setEnhanceFlagsToUi(obj["ocrEnhance"].toInt(
        static_cast<int>(ocr::Enhance::Upscale)));

    QString currentIdentifyErrorHandle= obj["identifyErrorHandle"].toString();
    int identifyErrorHandleIndex = ui->opencvErrorHandle->findData(currentIdentifyErrorHandle);
    if (identifyErrorHandleIndex >= 0) {
        ui->opencvErrorHandle->setCurrentIndex(identifyErrorHandleIndex);
        if (identifyErrorHandleIndex == 1)
        {
            initStepInputBoxSelect(configId,obj["stepsId"].toString());
        }
    } else {
        // 如果配置值不在选项中，使用默认值（停止执行任务）
        ui->opencvErrorHandle->setCurrentIndex(ui->opencvErrorHandle->findData("break"));
    }
}

void OcrForm::updateRoiPreview()
{
    if (!ui || !ui->roiPreview) {
        return;
    }

    ui->roiPreview->setRoiPercent(ui->roiXBox->value(),
                                  ui->roiYBox->value(),
                                  ui->roiWBox->value(),
                                  ui->roiHBox->value());
}

int OcrForm::enhanceFlagsFromUi() const
{
    unsigned int flags = static_cast<unsigned int>(ocr::Enhance::None);
    if (ui->enhanceUpscaleBox->isChecked()) {
        flags |= static_cast<unsigned int>(ocr::Enhance::Upscale);
    }
    if (ui->enhanceGrayscaleBox->isChecked()) {
        flags |= static_cast<unsigned int>(ocr::Enhance::Grayscale);
    }
    if (ui->enhanceContrastBox->isChecked()) {
        flags |= static_cast<unsigned int>(ocr::Enhance::Contrast);
    }
    if (ui->enhanceSharpenBox->isChecked()) {
        flags |= static_cast<unsigned int>(ocr::Enhance::Sharpen);
    }
    if (ui->enhanceInvertBox->isChecked()) {
        flags |= static_cast<unsigned int>(ocr::Enhance::AutoInvert);
    }
    return static_cast<int>(flags);
}

void OcrForm::setEnhanceFlagsToUi(const int flags) const
{
    const auto enhance = static_cast<ocr::Enhance>(flags);
    ui->enhanceUpscaleBox->setChecked(ocr::hasFlag(enhance, ocr::Enhance::Upscale));
    ui->enhanceGrayscaleBox->setChecked(ocr::hasFlag(enhance, ocr::Enhance::Grayscale));
    ui->enhanceContrastBox->setChecked(ocr::hasFlag(enhance, ocr::Enhance::Contrast));
    ui->enhanceSharpenBox->setChecked(ocr::hasFlag(enhance, ocr::Enhance::Sharpen));
    ui->enhanceInvertBox->setChecked(ocr::hasFlag(enhance, ocr::Enhance::AutoInvert));
}

void OcrForm::onUploadImageClicked()
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

void OcrForm::onCaptureImageClicked()
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

    // 与识别链路使用同一份截图，因此预览比例即实际识别区域比例
    cv::Mat rgb;
    cv::cvtColor(winImg, rgb, cv::COLOR_BGR2RGB);
    const QImage qimg(rgb.data, rgb.cols, rgb.rows,
                      static_cast<int>(rgb.step), QImage::Format_RGB888);

    // QImage 未拷贝 rgb 的数据，转成 QPixmap 前必须深拷贝，避免 rgb 析构后悬垂
    ui->roiPreview->setImage(QPixmap::fromImage(qimg.copy()));
    updateRoiPreview();
    Logger::log(QString("已获取游戏截图作为预览底图：%1x%2").arg(winImg.cols).arg(winImg.rows));
}

void OcrForm::initStepInputBoxSelect(QString configId, const QString &stepsId)
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
    obj["ocrRoiX"] = ui->roiXBox->value();
    obj["ocrRoiY"] = ui->roiYBox->value();
    obj["ocrRoiW"] = ui->roiWBox->value();
    obj["ocrRoiH"] = ui->roiHBox->value();
    obj["ocrEnhance"] = enhanceFlagsFromUi();
    obj["identifyErrorHandle"] = ui->opencvErrorHandle->currentData().toString();

    //如果是跳转
    if (comparesEqual(obj["identifyErrorHandle"], "jump"))
    {
        obj["jumpStepsId"] = ui->stepInputBox->currentText().toInt();
    }
    return obj;
}
