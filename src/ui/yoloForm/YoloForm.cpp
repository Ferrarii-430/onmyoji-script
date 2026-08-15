#include "YoloForm.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
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

    // 边框排除：随机点击时在匹配框内按比例排除四边区域，全 0 表示不排除。
    // 界面用百分比（0~90%），保存时换算为 0~0.9 的两位小数比值。
    auto makeExcludeBox = [this]() {
        auto* box = new QDoubleSpinBox(this);
        box->setDecimals(0);
        box->setSuffix("%");
        box->setMinimum(0.0);
        box->setMaximum(90.0);
        box->setSingleStep(5.0);
        box->setValue(0.0);
        return box;
    };
    excludeLeftBox = makeExcludeBox();
    excludeRightBox = makeExcludeBox();
    excludeTopBox = makeExcludeBox();
    excludeBottomBox = makeExcludeBox();

    // 左右边框一行
    auto* lrLayout = new QHBoxLayout;
    lrLayout->setContentsMargins(0, 0, 0, 0);
    lrLayout->setSpacing(4);
    lrLayout->addWidget(new QLabel("左", this));
    lrLayout->addWidget(excludeLeftBox);
    lrLayout->addWidget(new QLabel("右", this));
    lrLayout->addWidget(excludeRightBox);
    auto* lrWidget = new QWidget(this);
    lrWidget->setLayout(lrLayout);
    auto* lrLabel = new QLabel("左右边框排除", this);
    lrLabel->setToolTip("随机点击时排除匹配框左/右两侧的比例区域（0~0.9），全 0 不排除");
    layout->addRow(lrLabel, lrWidget);

    // 上下边框一行
    auto* tbLayout = new QHBoxLayout;
    tbLayout->setContentsMargins(0, 0, 0, 0);
    tbLayout->setSpacing(4);
    tbLayout->addWidget(new QLabel("上", this));
    tbLayout->addWidget(excludeTopBox);
    tbLayout->addWidget(new QLabel("下", this));
    tbLayout->addWidget(excludeBottomBox);
    auto* tbWidget = new QWidget(this);
    tbWidget->setLayout(tbLayout);
    auto* tbLabel = new QLabel("上下边框排除", this);
    tbLabel->setToolTip("随机点击时排除匹配框上/下两侧的比例区域（0~0.9），全 0 不排除");
    layout->addRow(tbLabel, tbWidget);

    errorHandleBox = new QComboBox(this);
    errorHandleBox->addItem("继续执行任务","next");
    errorHandleBox->addItem("跳转步骤","jump");
    errorHandleBox->addItem("跳过本次循环","continue");
    errorHandleBox->addItem("停止执行任务","break");
    errorHandleBox->addItem("重试","retry");
    errorHandleBox->setCurrentIndex(errorHandleBox->findData("break"));
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
    // 比值(0~0.9) -> 百分比(0~90)
    excludeLeftBox->setValue(qRound(obj["excludeLeft"].toDouble(0.0) * 100.0));
    excludeRightBox->setValue(qRound(obj["excludeRight"].toDouble(0.0) * 100.0));
    excludeTopBox->setValue(qRound(obj["excludeTop"].toDouble(0.0) * 100.0));
    excludeBottomBox->setValue(qRound(obj["excludeBottom"].toDouble(0.0) * 100.0));

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
        // 如果配置值不在选项中，使用默认值（停止执行任务）
        errorHandleBox->setCurrentIndex(errorHandleBox->findData("break"));
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
    // 百分比(0~90) -> 比值(0~0.9)，保留两位小数，避免浮点尾数（如 0.15000000000000002）
    auto percentToRatio = [](double percent) {
        return QString::number(percent / 100.0, 'f', 2).toDouble();
    };
    obj["excludeLeft"] = percentToRatio(excludeLeftBox->value());
    obj["excludeRight"] = percentToRatio(excludeRightBox->value());
    obj["excludeTop"] = percentToRatio(excludeTopBox->value());
    obj["excludeBottom"] = percentToRatio(excludeBottomBox->value());
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
