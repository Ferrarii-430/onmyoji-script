//
// Created by CZY on 2025/9/30.
//

// You may need to build the project (run Qt uic code generator) to get "ui_TypeOpenCVForm.h" resolved

#include "typeopencvform.h"
#include <QJsonObject>
#include <QPushButton>
#include <QBuffer>
#include <QPainter>
#include <qscreen.h>
#include <QTimer>
#include <windows.h>
#include "src/core/AppPaths.h"
#include "src/game/GameWindow.h"
#include "ui_TypeOpenCVForm.h"
#include "ScreenCaptureWidget.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/vision/TemplateMatcher.h"

TypeOpenCVForm::TypeOpenCVForm(QWidget *parent) :
    QWidget(parent), ui(new Ui::TypeOpenCVForm) {
    ui->setupUi(this);
    ui->stepInputBox->hide(); // 初始状态隐藏
    ui->stepInputLabel->hide(); // 初始状态隐藏

    ui->spinScoreBox->setValue(0.55);
    ui->colorCheckCheckBox->setChecked(false); // 默认不开启颜色判断
    ui->excludeLeftBox->setValue(0.0);   // 边框排除默认全 0（不排除）
    ui->excludeRightBox->setValue(0.0);
    ui->excludeTopBox->setValue(0.0);
    ui->excludeBottomBox->setValue(0.0);

    ui->opencvErrorHandle->addItem("继续执行任务","next");
    ui->opencvErrorHandle->addItem("跳转步骤","jump");
    ui->opencvErrorHandle->addItem("跳过本次循环","continue");
    ui->opencvErrorHandle->addItem("停止执行任务","break");
    ui->opencvErrorHandle->addItem("重试","retry");
    ui->opencvErrorHandle->setCurrentIndex(ui->opencvErrorHandle->findData("break"));

    connect(ui->btnCapture, &QPushButton::clicked, this, &TypeOpenCVForm::onCaptureButtonClicked);

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
}

TypeOpenCVForm::~TypeOpenCVForm() {
    delete ui;
}

void TypeOpenCVForm::loadFromJson(const QString &configId, const QJsonObject &obj)
{
    currentConfigId = configId;
    stepDataCopy = obj;
    ui->lineTaskNameEdit->setText(obj["taskName"].toString());
    ui->spinScoreBox->setValue(obj["score"].toDouble());
    ui->randomClickCheckBox->setChecked(obj["randomClick"].toBool());
    ui->colorCheckCheckBox->setChecked(obj["colorCheck"].toBool());
    // 比值(0~0.9) -> 百分比(0~90)
    ui->excludeLeftBox->setValue(qRound(obj["excludeLeft"].toDouble(0.0) * 100.0));
    ui->excludeRightBox->setValue(qRound(obj["excludeRight"].toDouble(0.0) * 100.0));
    ui->excludeTopBox->setValue(qRound(obj["excludeTop"].toDouble(0.0) * 100.0));
    ui->excludeBottomBox->setValue(qRound(obj["excludeBottom"].toDouble(0.0) * 100.0));
    templateCaptureWidth_ = obj["captureWidth"].toInt(0);
    templateCaptureHeight_ = obj["captureHeight"].toInt(0);

    QString currentIdentifyErrorHandle = obj["identifyErrorHandle"].toString();
    int identifyErrorHandleIndex = ui->opencvErrorHandle->findData(currentIdentifyErrorHandle);
    if (identifyErrorHandleIndex >= 0) {
        ui->opencvErrorHandle->setCurrentIndex(identifyErrorHandleIndex);
        if (identifyErrorHandleIndex == 1)
        {
            QString targetId = obj["jumpStepsId"].toString().trimmed();
            int stepsInputBoxIndex = ui->stepInputBox->findData(targetId);
            if (stepsInputBoxIndex != -1) {
                ui->stepInputBox->setCurrentIndex(stepsInputBoxIndex);
                qDebug() << "成功回显ID:" << targetId;
            } else {
                qDebug() << "未找到ID，当前ComboBox内容:" << targetId;
                for (int i = 0; i < ui->stepInputBox->count(); ++i) {
                    qDebug() << "索引" << i << "显示文本:" << ui->stepInputBox->itemText(i)
                             << "用户数据:" << ui->stepInputBox->itemData(i).toString();
                }
            }
        }
    } else {
        // 如果配置值不在选项中，使用默认值（停止执行任务）
        ui->opencvErrorHandle->setCurrentIndex(ui->opencvErrorHandle->findData("break"));
    }

    QString imagePath = obj["imagePath"].toString();
    if (!imagePath.isEmpty())
    {
        QString savePath = AppPaths::instance().screenshotPath() + imagePath;
        if (!savePath.isEmpty()) {
            QPixmap pixmap(savePath);
            if (!pixmap.isNull()) {
                originalPixmap_ = pixmap;
                QTimer::singleShot(100, this, &TypeOpenCVForm::updatePreview);
            } else {
                ui->labelPreview->setText("图片加载失败: " + savePath);
            }
        }
    }
}

void TypeOpenCVForm::initStepInputBoxSelect(QString configId, const QString &stepsId)
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

QJsonObject TypeOpenCVForm::toJson() const {
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
    obj["type"] = "OPENCV";
    obj["taskName"] = ui->lineTaskNameEdit->text();
    obj["score"] = ui->spinScoreBox->value();
    obj["randomClick"] = ui->randomClickCheckBox->isChecked();
    obj["colorCheck"] = ui->colorCheckCheckBox->isChecked();
    // 百分比(0~90) -> 比值(0~0.9)，保留两位小数，避免浮点尾数（如 0.15000000000000002）
    auto percentToRatio = [](double percent) {
        return QString::number(percent / 100.0, 'f', 2).toDouble();
    };
    obj["excludeLeft"] = percentToRatio(ui->excludeLeftBox->value());
    obj["excludeRight"] = percentToRatio(ui->excludeRightBox->value());
    obj["excludeTop"] = percentToRatio(ui->excludeTopBox->value());
    obj["excludeBottom"] = percentToRatio(ui->excludeBottomBox->value());
    obj["identifyErrorHandle"] = ui->opencvErrorHandle->currentData().toString();
    obj["captureWidth"] = templateCaptureWidth_;
    obj["captureHeight"] = templateCaptureHeight_;

    //如果是跳转
    if (comparesEqual(obj["identifyErrorHandle"].toString(), "jump"))
    {
        obj["jumpStepsId"] = ui->stepInputBox->currentData().toString();
    }else
    {
        obj["jumpStepsId"] = QJsonValue::Null;
    }

    // 保存原始截图而非预览控件里的 pixmap：预览图为适配 label 尺寸而被
    // 缩放并用透明背景补边，用它做模板会与实际框选区域不一致。
    QPixmap pix = originalPixmap_;
    if (pix.isNull()) pix = ui->labelPreview->pixmap();
    if (!pix.isNull()) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        pix.save(&buffer, "PNG");
        buffer.close();
        obj["image"] = QString::fromLatin1(bytes.toBase64());
    }
    return obj;
}

void TypeOpenCVForm::onCaptureButtonClicked() {
    ScreenCaptureWidget* sc = new ScreenCaptureWidget(nullptr); // 父窗口设 nullptr 避免阻塞
    sc->show();

    connect(sc, &ScreenCaptureWidget::captureFinished, this, [this, sc](const QPixmap &pix){
        originalPixmap_ = pix; // 保存原始截图
        updatePreview();       // 显示一次
        // qDebug() << "截图完成";
        sc->deleteLater();

        // 记录当前游戏窗口客户区分辨率，用于后续按模板原生分辨率匹配
        templateCaptureWidth_ = 0;
        templateCaptureHeight_ = 0;
        // 先尝试定位窗口；若程序刚启动还未定位，handle() 可能为空
        if (!GameWindow::instance().locate()) {
            Logger::log(QString("[WARN] 截图时未能定位游戏窗口，无法记录模板截取分辨率"));
        }
        HWND hwnd = GameWindow::instance().handle();
        if (hwnd) {
            RECT rc;
            if (GetClientRect(hwnd, &rc)) {
                templateCaptureWidth_ = rc.right - rc.left;
                templateCaptureHeight_ = rc.bottom - rc.top;
                Logger::log(QString("记录模板截取分辨率: %1x%2")
                            .arg(templateCaptureWidth_).arg(templateCaptureHeight_));
            } else {
                Logger::log(QString("[WARN] GetClientRect 失败，错误码: %1").arg(GetLastError()));
            }
        } else {
            Logger::log(QString("[WARN] 游戏窗口句柄为空，无法记录模板截取分辨率"));
        }
    });
}

void TypeOpenCVForm::updatePreview() const
{
    if (originalPixmap_.isNull()) return;

    QLabel* label = ui->labelPreview;
    QSize labelSize = label->size();

    // 计算按比例缩放后的图片
    QPixmap scaled = originalPixmap_.scaled(
        labelSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    // 创建与label一样大的pixmap背景，并居中绘制scaled图
    QPixmap finalPixmap(labelSize);
    finalPixmap.fill(Qt::transparent);  // 背景透明或你可以用 Qt::black

    QPainter painter(&finalPixmap);
    QPoint center((labelSize.width() - scaled.width()) / 2,
                  (labelSize.height() - scaled.height()) / 2);
    painter.drawPixmap(center, scaled);
    painter.end();

    label->setPixmap(finalPixmap);
}