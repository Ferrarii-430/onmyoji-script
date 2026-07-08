//
// Created by CZY on 2025/9/30.
//

// You may need to build the project (run Qt uic code generator) to get "ui_EditTaskDialog.h" resolved

#include "edittaskdialog.h"
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include "src/core/ConfigTypeEnum.h"

#include "src/core/Logger.h"
#include "src/engine/ScriptActions.h"
#include "src/game/GameWindow.h"
#include "src/ui/typeOpenCVForm/typeopencvform.h"
#include "src/ui/waitForm/waitform.h"
#include "ui_EditTaskDialog.h"

EditTaskDialog::EditTaskDialog(EditMode mode, const QJsonObject &stepData, const QString &configId, QWidget *parent) :
    QDialog(parent), ui(new Ui::EditTaskDialog) {
    setWindowTitle(mode == EditMode::Add ? "新增任务" : "编辑任务");
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("保存");
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");

    QPushButton* testButton = ui->buttonBox->addButton(tr("测试"), QDialogButtonBox::ActionRole);

    // 直接对按钮设样式，避免 QDialogButtonBox 内部角色/焦点差异导致
    // 圆角、尺寸不一致；三个按钮分别配色：保存绿 / 测试橙 / 取消灰
    auto styleDialogButton = [](QPushButton* btn, const QString& bg,
                                const QString& hover, const QString& pressed) {
        btn->setFixedSize(88, 32);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background-color: %1; color: #ffffff; border: none;"
            " border-radius: 5px; padding: 0; font-weight: bold; }"
            "QPushButton:hover { background-color: %2; }"
            "QPushButton:pressed { background-color: %3; }")
            .arg(bg, hover, pressed));
    };
    styleDialogButton(ui->buttonBox->button(QDialogButtonBox::Ok),
                      "#34a853", "#3fbb60", "#2c8f46");
    styleDialogButton(testButton,
                      "#f59e0b", "#fbad2e", "#d9880a");
    styleDialogButton(ui->buttonBox->button(QDialogButtonBox::Cancel),
                      "#64748b", "#7586a0", "#55637d");

    typeForm = new TypeOpenCVForm(this);
    waitForm = new WaitForm(this);
    ocrForm = new OcrForm(this);
    yoloForm = new YoloForm(this);

    // 下拉项、步骤类型、表单页在此处一一对应；调整顺序只改这里即可，
    // 其余逻辑均按类型字符串（itemData）分发，不依赖下拉项下标
    const QList<QPair<QString, QString>> pages = {
        {"OpenCV识图", "OPENCV"},
        {"OCR识别", "OCR"},
        {"YOLO识别", "YOLO"},
        {"等待", "WAIT"},
    };
    for (const auto& page : pages) {
        ui->comboBox->addItem(page.first, page.second);
        ui->stackedWidget->addWidget(formForType(page.second));
    }

    connect(ui->comboBox, &QComboBox::currentIndexChanged, this, [this, testButton](int index){
        ui->stackedWidget->setCurrentIndex(index);
        // 只有“等待”没有可测试的识别动作
        testButton->setDisabled(ui->comboBox->itemData(index).toString() == "WAIT");
    });

    // 初始化数据
    QString initType = pages.first().second;
    if (mode == EditMode::Edit && !stepData.isEmpty()) {
        const QString type = stepData["type"].toString();
        if (ui->comboBox->findData(type) >= 0) {
            initType = type;
            if (type == "OPENCV") typeForm->loadFromJson(configId, stepData);
            else if (type == "OCR") ocrForm->loadFromJson(configId, stepData);
            else if (type == "YOLO") yoloForm->loadFromJson(configId, stepData);
            else if (type == "WAIT") waitForm->loadFromJson(configId, stepData);
        }
    }
    const int initIndex = ui->comboBox->findData(initType);
    ui->comboBox->setCurrentIndex(initIndex);
    ui->stackedWidget->setCurrentIndex(initIndex);
    testButton->setDisabled(initType == "WAIT");

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
    const QString type = currentType();
    if (type == "OPENCV") return typeForm->toJson();
    if (type == "OCR") return ocrForm->toJson();
    if (type == "YOLO") return yoloForm->toJson();
    if (type == "WAIT") return waitForm->toJson();
    return QJsonObject();
}

QString EditTaskDialog::currentType() const {
    return ui->comboBox->currentData().toString();
}

QWidget* EditTaskDialog::formForType(const QString& type) const {
    if (type == "OPENCV") return typeForm;
    if (type == "OCR") return ocrForm;
    if (type == "YOLO") return yoloForm;
    if (type == "WAIT") return waitForm;
    return nullptr;
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
    if (!GameWindow::instance().locate())
    {
        Logger::log(QString("窗口未找到"));
        return;
    }

    const QString type = currentType();
    if (type == "OPENCV") {
        QString savePath;
        QJsonObject json = typeForm->toJson();
        QString imagePath = json["image"].toString(); //此时还是image 保存到config文件后是imagePath
        const double score = json["score"].toDouble();
        const bool randomClick = json["randomClick"].toBool();
        if (isBase64(imagePath))
        {
            savePath  = ScriptActions::instance().opencvRecognizesAndClickByBase64(imagePath, score, randomClick);
        }else
        {
            savePath = ScriptActions::instance().opencvRecognizesAndClick(imagePath, score, randomClick);
        }
        if (savePath.isEmpty())
        {
            Logger::log(QString("测试OpenCV识别失败"));
            return;
        }
        emit imagePathRequested(savePath); // 发射信号
    } else if (type == "OCR") {
        QJsonObject json = ocrForm->toJson();
        QString ocrText = json["ocrText"].toString();
        const double score = json["score"].toDouble();
        const bool randomClick = json["randomClick"].toBool();
        const QRectF roiPercent(json["ocrRoiX"].toDouble(), json["ocrRoiY"].toDouble(),
                                json["ocrRoiW"].toDouble(), json["ocrRoiH"].toDouble());
        QString savePath = ScriptActions::instance().ocrRecognizesAndClick(ocrText, score, randomClick, roiPercent);
        if (savePath.isEmpty())
        {
            Logger::log(QString("测试OCR识别失败"));
            return;
        }
        emit imagePathRequested(savePath); // 发射信号
    } else if (type == "YOLO") {
        QJsonObject json = yoloForm->toJson();
        QString yoloLabel = json["yoloLabel"].toString();
        const double score = json["score"].toDouble();
        const bool randomClick = json["randomClick"].toBool();
        QString savePath = ScriptActions::instance().yoloRecognizesAndClick(score, randomClick, yoloLabel, 0.0, 0.0);
        if (savePath.isEmpty())
        {
            Logger::log(QString("测试YOLO识别失败"));
            return;
        }
        emit imagePathRequested(savePath); // 发射信号
    }
    // 等待（WAIT）无可测试动作
}

bool EditTaskDialog::validateWaitFormData(const QJsonObject &data)
{
    QString taskName = data["taskName"].toString().trimmed();
    bool randomWait = data["randomWait"].toBool();
    int time = data["time"].toInt();
    int offsetTime = data["offsetTime"].toInt();

    if (taskName.isEmpty()) {
        QMessageBox::warning(this, "警告", "任务名称不能为空！");
        return false;
    }

    if (time <= 0)
    {
        QMessageBox::warning(this, "警告", "等待时间不能 ＜= 0 ！");
        return false;
    }

    if (offsetTime <= 0)
    {
        QMessageBox::warning(this, "警告", "偏移时间不能 <= 0 ！");
        return false;
    }

    if (randomWait)
    {
        if (offsetTime >= time)
        {
            QMessageBox::warning(this, "警告", "偏移时间不能 >= 等待时间 ！");
            return false;
        }
    }

    return true;
}

bool EditTaskDialog::validateOpenCVFormData(const QJsonObject &data)
{
    QString taskName = data["taskName"].toString().trimmed();
    QString imagePath = data["image"].toString().trimmed(); //此时还是image 不是imagePath

    if (taskName.isEmpty()) {
        QMessageBox::warning(this, "警告", "任务名称不能为空！");
        return false;
    }

    if (imagePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "截图不能为空！");
        return false;
    }

    return true;
}

bool EditTaskDialog::validateOcrFormData(const QJsonObject &data)
{
    QString taskName = data["taskName"].toString().trimmed();
    QString ocrText = data["ocrText"].toString().trimmed();

    if (taskName.isEmpty()) {
        QMessageBox::warning(this, "警告", "任务名称不能为空！");
        return false;
    }

    if (ocrText.isEmpty()) {
        QMessageBox::warning(this, "警告", "OCR文本不能为空！");
        return false;
    }

    return true;
}

bool EditTaskDialog::validateYoloFormData(const QJsonObject &data)
{
    QString taskName = data["taskName"].toString().trimmed();
    QString yoloLabel = data["yoloLabel"].toString().trimmed();

    if (taskName.isEmpty()) {
        QMessageBox::warning(this, "警告", "任务名称不能为空！");
        return false;
    }

    if (yoloLabel.isEmpty()) {
        QMessageBox::warning(this, "警告", "识别标签不能为空！");
        return false;
    }

    return true;
}

// 在 EditTaskDialog 中添加验证方法
bool EditTaskDialog::validateData() {
    QJsonObject data = resultData();
    QString type = data["type"].toString();
    bool isValidate = true;

    if (comparesEqual(type, "OPENCV"))
    {
        isValidate = validateOpenCVFormData(data);
    }else if (comparesEqual(type, "WAIT"))
    {
        isValidate = validateWaitFormData(data);
    }else if (comparesEqual(type, "OCR"))
    {
        isValidate = validateOcrFormData(data);
    }else if (comparesEqual(type, "YOLO"))
    {
        isValidate = validateYoloFormData(data);
    }

    return isValidate;
}

// 重写 accept()
void EditTaskDialog::accept() {
    if (!validateData()) {
        return; // 验证失败，不关闭对话框
    }
    QDialog::accept();
}