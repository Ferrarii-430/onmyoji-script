//
// Created by CZY on 2026/10/15.
//

#include "SwipeForm.h"

#include <QJsonObject>
#include <QMessageBox>
#include <QUuid>
#include <opencv2/imgproc.hpp>

#include "src/core/Logger.h"
#include "ui_SwipeForm.h"
#include "src/core/ProfileStore.h"
#include "src/game/GameWindow.h"
#include "src/game/capture/CaptureService.h"
#include "src/ui/swipeForm/SwipePreviewWidget.h"

SwipeForm::SwipeForm(QWidget *parent) :
    QWidget(parent), ui(new Ui::SwipeForm) {
    ui->setupUi(this);

    ui->stepInputBox->hide(); // 初始状态隐藏
    ui->stepInputLabel->hide(); // 初始状态隐藏

    ui->errorHandleBox->addItem("继续执行任务","next");
    ui->errorHandleBox->addItem("跳转步骤","jump");
    ui->errorHandleBox->addItem("跳过本次循环","continue");
    ui->errorHandleBox->addItem("停止执行任务","break");
    ui->errorHandleBox->addItem("重试","retry");
    ui->errorHandleBox->setCurrentIndex(ui->errorHandleBox->findData("break"));

    connect(ui->btnCaptureImage, &QToolButton::clicked, this, &SwipeForm::onCaptureImageClicked);
    connect(ui->swipeFromXBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SwipeForm::updateSwipePreview);
    connect(ui->swipeFromYBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SwipeForm::updateSwipePreview);
    connect(ui->swipeToXBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SwipeForm::updateSwipePreview);
    connect(ui->swipeToYBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SwipeForm::updateSwipePreview);

    // 单选框切换当前编辑点，预览中高亮对应的标记
    connect(ui->radioPoint1, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            ui->swipePreview->setActivePoint(1);
        }
    });
    connect(ui->radioPoint2, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            ui->swipePreview->setActivePoint(2);
        }
    });

    // 点击预览取点：坐标同步到当前编辑点的数值框；阻断信号避免 valueChanged 回环
    connect(ui->swipePreview, &SwipePreviewWidget::pointPicked, this,
            [this](int pointIndex, double x, double y)
    {
        QDoubleSpinBox *xBox = (pointIndex == 2) ? ui->swipeToXBox : ui->swipeFromXBox;
        QDoubleSpinBox *yBox = (pointIndex == 2) ? ui->swipeToYBox : ui->swipeFromYBox;
        const QSignalBlocker bx(xBox);
        const QSignalBlocker by(yBox);
        xBox->setValue(x);
        yBox->setValue(y);
        updateSwipePreview();
    });

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

    updateSwipePreview();
}

SwipeForm::~SwipeForm() {
    delete ui;
}

void SwipeForm::loadFromJson(const QString &configId, const QJsonObject &obj)
{
    currentConfigId = configId;
    stepDataCopy = obj;
    ui->lineTaskNameEdit->setText(obj["taskName"].toString());
    ui->autoCancelCollabCheckBox->setChecked(obj["autoCancelCollab"].toBool(false));

    // 起点（按下）/终点（抬起），单位为图片尺寸百分比
    ui->swipeFromXBox->setValue(obj["swipeFromX"].toDouble(0.0));
    ui->swipeFromYBox->setValue(obj["swipeFromY"].toDouble(0.0));
    ui->swipeToXBox->setValue(obj["swipeToX"].toDouble(100.0));
    ui->swipeToYBox->setValue(obj["swipeToY"].toDouble(100.0));
    updateSwipePreview();

    ui->swipeDurationBox->setValue(obj["swipeDurationMs"].toInt(500));

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

void SwipeForm::updateSwipePreview()
{
    if (!ui || !ui->swipePreview) {
        return;
    }

    ui->swipePreview->setPoint1Percent(ui->swipeFromXBox->value(),
                                       ui->swipeFromYBox->value());
    ui->swipePreview->setPoint2Percent(ui->swipeToXBox->value(),
                                       ui->swipeToYBox->value());
}

void SwipeForm::onCaptureImageClicked()
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

    // 与滑动链路使用同一份截图，因此预览比例即实际滑动坐标比例
    cv::Mat rgb;
    cv::cvtColor(winImg, rgb, cv::COLOR_BGR2RGB);
    const QImage qimg(rgb.data, rgb.cols, rgb.rows,
                      static_cast<int>(rgb.step), QImage::Format_RGB888);

    // QImage 未拷贝 rgb 的数据，转成 QPixmap 前必须深拷贝，避免 rgb 析构后悬垂
    ui->swipePreview->setImage(QPixmap::fromImage(qimg.copy()));
    updateSwipePreview();
    Logger::log(QString("已获取游戏截图作为预览底图：%1x%2").arg(winImg.cols).arg(winImg.rows));
}

void SwipeForm::initStepInputBoxSelect(QString configId, const QString &stepsId)
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

QJsonObject SwipeForm::toJson() const {
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
    obj["type"] = "SWIPE";
    obj["taskName"] = ui->lineTaskNameEdit->text();
    obj["autoCancelCollab"] = ui->autoCancelCollabCheckBox->isChecked();
    obj["swipeFromX"] = ui->swipeFromXBox->value();
    obj["swipeFromY"] = ui->swipeFromYBox->value();
    obj["swipeToX"] = ui->swipeToXBox->value();
    obj["swipeToY"] = ui->swipeToYBox->value();
    obj["swipeDurationMs"] = ui->swipeDurationBox->value();
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
