#include "YoloForm.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QUuid>

#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/vision/DetectionCatalog.h"

YoloForm::YoloForm(QWidget *parent) :
    QWidget(parent) {
    auto* layout = new QFormLayout(this);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(20);

    lineTaskNameEdit = new QLineEdit(this);
    layout->addRow(new QLabel("任务名称", this), lineTaskNameEdit);

    labelBox = new QComboBox(this);
    populateLabelBox();
    layout->addRow(new QLabel("识别标签", this), labelBox);

    spinScoreBox = new QDoubleSpinBox(this);
    spinScoreBox->setMaximum(1.0);
    spinScoreBox->setSingleStep(0.01);
    spinScoreBox->setValue(0.55);
    layout->addRow(new QLabel("分数阈值", this), spinScoreBox);

    randomClickCheckBox = new QCheckBox(this);
    layout->addRow(new QLabel("是否随机点击", this), randomClickCheckBox);

    errorHandleBox = new QComboBox(this);
    errorHandleBox->addItem("继续执行任务","next");
    errorHandleBox->addItem("跳转步骤","jump");
    errorHandleBox->addItem("跳过本次循环","continue");
    errorHandleBox->addItem("停止执行任务","break");
    errorHandleBox->addItem("重试","retry");
    layout->addRow(new QLabel("识别失败处理", this), errorHandleBox);

    stepInputLabel = new QLabel("跳转步骤", this);
    stepInputBox = new QComboBox(this);
    layout->addRow(stepInputLabel, stepInputBox);
    stepInputLabel->hide();
    stepInputBox->hide();

    connect(errorHandleBox, &QComboBox::currentIndexChanged, this, [this](int index)
    {
        // 当值为1(跳转步骤)时显示stepInput，其他值隐藏
        if (index == 1) {
            initStepInputBoxSelect(currentConfigId, stepDataCopy["stepsId"].toString());
            stepInputBox->show();
            stepInputLabel->show();
        } else {
            stepInputBox->hide();
            stepInputLabel->hide();
        }
    });
}

void YoloForm::populateLabelBox()
{
    labelBox->clear();
    const QList<LabelInfo> labels = DetectionCatalog::allLabels();
    if (labels.isEmpty()) {
        Logger::log(QString("YOLO标签目录为空，无法填充标签下拉框"));
        return;
    }
    for (const LabelInfo& info : labels) {
        QString display = info.className;
        if (!info.function.isEmpty()) {
            display += QString("（%1）").arg(info.function);
        }
        labelBox->addItem(display, info.className);
        if (!info.scenes.isEmpty()) {
            labelBox->setItemData(labelBox->count() - 1,
                                  QString("场景: %1").arg(info.scenes.join("、")),
                                  Qt::ToolTipRole);
        }
    }
}

void YoloForm::loadFromJson(const QString &configId, const QJsonObject &obj)
{
    currentConfigId = configId;
    stepDataCopy = obj;
    lineTaskNameEdit->setText(obj["taskName"].toString());
    spinScoreBox->setValue(obj["score"].toDouble());
    randomClickCheckBox->setChecked(obj["randomClick"].toBool());

    const QString yoloLabel = obj["yoloLabel"].toString();
    int labelIndex = labelBox->findData(yoloLabel);
    if (labelIndex >= 0) {
        labelBox->setCurrentIndex(labelIndex);
    } else if (!yoloLabel.isEmpty()) {
        // 配置里的标签不在目录中，仍保留原值避免保存时丢失
        labelBox->addItem(yoloLabel, yoloLabel);
        labelBox->setCurrentIndex(labelBox->count() - 1);
    }

    const QString currentIdentifyErrorHandle = obj["identifyErrorHandle"].toString();
    int identifyErrorHandleIndex = errorHandleBox->findData(currentIdentifyErrorHandle);
    if (identifyErrorHandleIndex >= 0) {
        errorHandleBox->setCurrentIndex(identifyErrorHandleIndex);
        if (identifyErrorHandleIndex == 1)
        {
            initStepInputBoxSelect(configId, obj["stepsId"].toString());
            const QString targetId = obj["jumpStepsId"].toString().trimmed();
            const int stepsInputBoxIndex = stepInputBox->findData(targetId);
            if (stepsInputBoxIndex != -1) {
                stepInputBox->setCurrentIndex(stepsInputBoxIndex);
            }
        }
    } else {
        // 如果配置值不在选项中，使用默认值
        errorHandleBox->setCurrentIndex(0);
    }
}

void YoloForm::initStepInputBoxSelect(QString configId, const QString &stepsId)
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
        stepInputBox->clear();
        for (auto it = stepSelect.cbegin(); it != stepSelect.cend(); ++it) {
            stepInputBox->addItem(it.key(), it.value());
        }
    }
}

QJsonObject YoloForm::toJson() const {
    QJsonObject obj;
    if (stepDataCopy.isEmpty())
    {
        obj["stepsId"] = QUuid::createUuid().toString(QUuid::WithoutBraces);  // UUID;
    }else
    {
        obj["stepsId"] = stepDataCopy["stepsId"]; //复制原始UUID
    }
    obj["type"] = "YOLO";
    obj["taskName"] = lineTaskNameEdit->text();
    obj["yoloLabel"] = labelBox->currentData().toString();
    obj["score"] = spinScoreBox->value();
    obj["randomClick"] = randomClickCheckBox->isChecked();
    obj["identifyErrorHandle"] = errorHandleBox->currentData().toString();

    //如果是跳转
    if (comparesEqual(obj["identifyErrorHandle"].toString(), "jump"))
    {
        obj["jumpStepsId"] = stepInputBox->currentData().toString();
    }else
    {
        obj["jumpStepsId"] = QJsonValue::Null;
    }
    return obj;
}
