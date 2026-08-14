//
// Created by CZY on 2025/9/25.
//

// You may need to build the project (run Qt uic code generator) to get "ui_mainwindow.h" resolved

#include "mainwindow.h"
#include "src/core/Logger.h"
#include <opencv2/opencv.hpp>
#include "ui_mainwindow.h"
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QTimer>
#include <QDebug>
#include <QInputDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
#include "QMessageBox"

#include "src/core/AppPaths.h"
#include "src/core/ConfigTypeEnum.h"
#include "src/core/ProfileStore.h"
#include "src/core/SettingManager.h"
#include "src/engine/ScriptActions.h"
#include "src/engine/TaskRunner.h"
#include "src/platform/DPIHelper.h"
#include "src/ui/editTask/edittaskdialog.h"
#include "src/ui/setting/settingdialog.h"
#include "src/vision/ClassNameCache.h"
#include "src/vision/ImageIo.h"
#include "src/vision/YOLODetector.h"

mainwindow::mainwindow(QWidget *parent) :
    QWidget(parent), ui(new Ui::mainwindow) {
    ui->setupUi(this);

    // 配合全局 QSS 的 alternate-background-color 显示斑马纹
    ui->listWidget->setAlternatingRowColors(true);
    ui->tableWidget->setAlternatingRowColors(true);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setShowGrid(false);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->openCVIdentifyLabel->setStyleSheet(
        "border: 1px dashed #c0c4d0; border-radius: 6px; background-color: #ffffff;");
    ui->settingButton->raise(); // 确保不被同层其他控件盖住

    // 把 Logger 输出重定向到界面日志框
    Logger::setSink([this](const QString& msg) { appendLogToUI(msg); });

    // 绑定选中信号
    connect(ui->listWidget, &QListWidget::itemClicked,
            this, &mainwindow::onItemClicked);

    //绑定开启任务按钮
    connect(ui->startTaskButton, &QToolButton::clicked,
            this, &mainwindow::startTaskButtonClick);

    //绑定关闭任务按钮
    connect(ui->stopTaskButton, &QToolButton::clicked,
            this, &mainwindow::stopTaskButtonClick);

    //绑定添加方案按钮
    connect(ui->programmeAddBtn, &QToolButton::clicked,
            this, &mainwindow::onProgrammeAddBtnClicked);

    //绑定删除方案按钮
    connect(ui->programmeRemoveBtn, &QToolButton::clicked,
            this, &mainwindow::onProgrammeRemoveBtnClicked);

    //绑定添加方案内容按钮
    connect(ui->programmeContentAddBtn, &QToolButton::clicked,
            this, &mainwindow::onProgrammeContentAddBtnClicked);

    //绑定打开设置按钮
    connect(ui->settingButton, &QToolButton::clicked,
            this, &mainwindow::onSettingBtnClicked);

    //绑定方案内容上移按钮
    connect(ui->programmeContentUpButton, &QToolButton::clicked,
            this, &mainwindow::onProgrammeUpBtnClicked);

    //绑定方案内容下移按钮
    connect(ui->programmeContentDownButton, &QToolButton::clicked,
            this, &mainwindow::onProgrammeDownBtnClicked);

    //监听编辑方案名称
    connect(ui->listWidget, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        // 这里可以保存修改到配置文件等
        updateProgrammeContent(AppPaths::instance().configPath(), currentItem.id, item->text());
        loadListWidgetData();
    });

    //读取setting的配置
    if (SETTING_CONFIG.loadConfig())
    {
        Logger::log(QString("Setting配置加载成功！"));
    }

    //读取classes.txt的配置
    QString classesPath = AppPaths::instance().classesNamePath();
    if (ClassNameCache::initialize(classesPath))
    {
        Logger::log(QString("classes.txt配置加载成功！"));
    }

    //读取方案的配置
    loadListWidgetData();

    Logger::log(QString("Config配置加载成功！"));

    // 检查OpenCV版本和编译选项
    Logger::log(QString("OpenCV 版本 %1").arg(CV_VERSION));

    Logger::log(QString("RapidOCR 版本 v1.1.0"));

    QString onnxPath = AppPaths::instance().onmyojiYoloOnnxPath();
    bool initSuccess = YOLODetector::getInstance().initialize(onnxPath.toStdWString());
    if (initSuccess)
    {
        Logger::log(QString("onmyoji-yolo-v5 加载成功！"));
    }

    ui->taskCycleNumber->setValue(1);

    connect(&ScriptActions::instance(), &ScriptActions::requestShowImage,
            this, &mainwindow::showOpenCVIdentifyImage);

    // 任务执行器信号绑定
    TaskRunner& runner = TaskRunner::instance();
    connect(&runner, &TaskRunner::showImage, this, &mainwindow::showOpenCVIdentifyImage);
    connect(&runner, &TaskRunner::started, this, [this]() {
        ui->startTaskButton->setEnabled(false);
        ui->stopTaskButton->setEnabled(true);
    });
    connect(&runner, &TaskRunner::finished, this, [this]() {
        ui->startTaskButton->setEnabled(true);
        ui->stopTaskButton->setEnabled(false);
    });

    //设置DPI感知
    DPIHelper::SetProcessDPIAwareness();
}

mainwindow::~mainwindow() {
    delete ui;
}

void mainwindow::loadListWidgetData()
{
    refreshConfig();

    ui->listWidget->clear();

    QJsonArray arr = m_configArray;
    for (int i = 0; i < arr.size(); i++) {
        QJsonObject obj = arr[i].toObject();

        QString name = obj["name"].toString();
        QString id   = obj["id"].toString();   // 假设 JSON 里有 id 字段

        // 创建列表项
        QListWidgetItem *item = new QListWidgetItem(name);

        // 绑定额外数据（Qt::UserRole 是给用户自定义数据用的）
        item->setData(Qt::UserRole, id);
        item->setFlags(item->flags() | Qt::ItemIsEditable); // 添加可编辑标志

        ui->listWidget->addItem(item);
    }

    if (arr.size() > 0)
    {
        QJsonObject step = arr[0].toObject();
        QString id = step["id"].toString();
        QString name = step["name"].toString();
        commonSetCurrentItem(id,name);
        ui->currentTaskName->setText(name);
        showStepsInTable(arr[0].toObject()["steps"].toArray()); //默认显示第一配置的方案内容
    }
}

// 点击了方案列表
void mainwindow::onItemClicked(QListWidgetItem *item) {
    if (!item) return;
    // 获取显示文本
    QString name = item->text();
    QString id   = item->data(Qt::UserRole).toString();
    // Logger::log(QString( "onItemClicked->选中项 name:" + name + ", id:" + id));
    commonSetCurrentItem(id,name);
    ui->currentTaskName->setText(name);

    QJsonObject obj = QJsonObject(); //默认为空
    //读取对应的方案步骤数据到表格
    for (const QJsonValue &val : m_configArray) {
        if (!val.isObject()) {
            continue;
        }
        obj = val.toObject();
        if (obj["id"].toString() == id) {
            break;
        }
    }

    QJsonArray steps = obj["steps"].toArray();
    if (obj["type"] == "system")
    {
        ui->tableWidget->clear(); // 清空表格
        ui->tableWidget->setRowCount(0);
        ui->tableWidget->setColumnCount(0);
        ui->programmeContentAddBtn->setDisabled(true);
        // 按该系统方案的 systemConfig 生成可编辑表单(无自定义配置则退回原提示)
        showSystemConfigForm(id);
    }else
    {
        ui->programmeContentAddBtn->setDisabled(false);
        showStepsInTable(steps);
    }
}

// 按系统方案的 systemConfig 描述动态生成可编辑表单，控件变更即写回 config.json
void mainwindow::showSystemConfigForm(const QString &configId)
{
    // 每次销毁并重建容器：避免复用旧 QFormLayout 清理时的 takeAt 告警
    if (m_systemConfigForm) {
        m_systemConfigForm->deleteLater();
        m_systemConfigForm = nullptr;
    }

    const QJsonArray systemConfig = getSystemConfig(configId);
    if (systemConfig.isEmpty()) {
        // 该系统方案没有自定义配置，退回原来的静态提示
        ui->systemConfigTips->show();
        return;
    }
    ui->systemConfigTips->hide();

    // 表单容器，位置对齐任务表格区域
    m_systemConfigForm = new QWidget(ui->tableWidget->parentWidget());
    m_systemConfigForm->setGeometry(ui->tableWidget->geometry());
    // 边框 + 背景色，营造独立配置区域的分隔感（用 #objectName 限定，避免影响内部控件）
    m_systemConfigForm->setObjectName("systemConfigForm");
    m_systemConfigForm->setStyleSheet(
        "#systemConfigForm {"
        "  background-color: #f7f9fc;"
        "  border: 1px solid #d0d7e2;"
        "  border-radius: 8px;"
        "}");

    auto *form = new QFormLayout(m_systemConfigForm);
    form->setContentsMargins(16, 16, 16, 16);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 区域标题
    auto *title = new QLabel(QStringLiteral("方案配置"), m_systemConfigForm);
    title->setStyleSheet("font-weight: bold; font-size: 12pt; color: #333; padding-bottom: 4px;");
    form->addRow(title);

    // 方案级 tip：整个方案的功能使用说明，显示在标题下方
    const QString systemTip = getSystemTip(configId);
    if (!systemTip.isEmpty()) {
        auto *tipLabel = new QLabel(systemTip, m_systemConfigForm);
        tipLabel->setWordWrap(true);
        tipLabel->setStyleSheet(
            "color: #6b7280; font-size: 9pt; background-color: #eef2f7;"
            "border: 1px solid #d0d7e2; border-radius: 6px; padding: 8px;");
        form->addRow(tipLabel);
    }

    const QString configPath = AppPaths::instance().configPath();

    for (const QJsonValue &val : systemConfig) {
        const QJsonObject field = val.toObject();
        const QString key = field["key"].toString();
        const QString label = field["label"].toString();
        const QString control = field["control"].toString();
        const QString itemTip = field.value("tip").toString();

        // 每个配置项用一个垂直容器承载「控件 + 可选 tip 提示」，便于统一加入表单
        auto *fieldWidget = new QWidget(m_systemConfigForm);
        auto *fieldLayout = new QVBoxLayout(fieldWidget);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setSpacing(4);

        if (control == "number") {
            if (field.value("decimals").toInt(0) <= 0) {
                // 整数：QSpinBox
                auto *spin = new QSpinBox(fieldWidget);
                spin->setRange(field.value("min").toInt(0), field.value("max").toInt(100));
                spin->setSingleStep(field.value("step").toInt(1));
                spin->setValue(field.value("value").toInt());
                connect(spin, qOverload<int>(&QSpinBox::valueChanged), this,
                        [=](int v) { updateSystemConfigValue(configPath, configId, key, v); });
                fieldLayout->addWidget(spin);
            } else {
                // 小数：QDoubleSpinBox
                auto *spin = new QDoubleSpinBox(fieldWidget);
                spin->setDecimals(field.value("decimals").toInt(2));
                spin->setRange(field.value("min").toDouble(0.0), field.value("max").toDouble(1.0));
                spin->setSingleStep(field.value("step").toDouble(0.1));
                spin->setValue(field.value("value").toDouble());
                connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                        [=](double v) { updateSystemConfigValue(configPath, configId, key, v); });
                fieldLayout->addWidget(spin);
            }
        } else if (control == "switch") {
            // 开关：QCheckBox
            auto *check = new QCheckBox(fieldWidget);
            check->setChecked(field.value("value").toBool());
            connect(check, &QCheckBox::toggled, this,
                    [=](bool v) { updateSystemConfigValue(configPath, configId, key, v); });
            fieldLayout->addWidget(check);
        } else if (control == "select") {
            // 下拉：QComboBox
            auto *combo = new QComboBox(fieldWidget);
            for (const QJsonValue &opt : field.value("options").toArray()) {
                combo->addItem(opt.toString());
            }
            combo->setCurrentText(field.value("value").toString());
            connect(combo, &QComboBox::currentTextChanged, this,
                    [=](const QString &v) { updateSystemConfigValue(configPath, configId, key, v); });
            fieldLayout->addWidget(combo);
        } else if (control == "input") {
            // 文本输入：QLineEdit
            auto *edit = new QLineEdit(fieldWidget);
            edit->setText(field.value("value").toString());
            edit->setPlaceholderText(field.value("placeholder").toString());
            if (field.contains("maxLength")) {
                edit->setMaxLength(field.value("maxLength").toInt());
            }
            connect(edit, &QLineEdit::textChanged, this,
                    [=](const QString &v) { updateSystemConfigValue(configPath, configId, key, v); });
            fieldLayout->addWidget(edit);
        }

        // 配置项级 tip：单个选项的解释说明，显示在控件下方
        if (!itemTip.isEmpty()) {
            auto *itemTipLabel = new QLabel(itemTip, fieldWidget);
            itemTipLabel->setWordWrap(true);
            itemTipLabel->setStyleSheet("color: #9ca3af; font-size: 8pt;");
            fieldLayout->addWidget(itemTipLabel);
        }

        form->addRow(label, fieldWidget);
    }

    m_systemConfigForm->show();
    m_systemConfigForm->raise();
}

//加载数据到任务表格
void mainwindow::showStepsInTable(const QJsonArray &steps) {
    ui->systemConfigTips->hide();
    if (m_systemConfigForm) {
        m_systemConfigForm->hide();
    }
    ui->tableWidget->raise();
    ui->tableWidget->clear(); // 清空表格
    ui->tableWidget->setRowCount(steps.size());
    ui->tableWidget->setColumnCount(5); // 新增序号列
    QStringList headers = {"序号", "任务名称", "类型", "操作",""};
    ui->tableWidget->setHorizontalHeaderLabels(headers);

    // 单元格按钮外包一层容器留出边距，避免按钮铺满整格粘在一起
    auto wrapCellWidget = [](QWidget* inner) {
        auto* container = new QWidget();
        auto* layout = new QHBoxLayout(container);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->addWidget(inner);
        return container;
    };

    for (int i = 0; i < steps.size(); ++i) {
        QJsonObject step = steps[i].toObject();

        // 序号
        ui->tableWidget->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1))); // 从1开始

        // 任务名称
        QString taskName = step["taskName"].toString();
        ui->tableWidget->setItem(i, 1, new QTableWidgetItem(taskName));

        // 类型
        QString type = step["type"].toString();
        ui->tableWidget->setItem(i, 2, new QTableWidgetItem(type));

        // 编辑按钮
        QPushButton *editBtn = new QPushButton("编辑");
        ui->tableWidget->setCellWidget(i, 3, wrapCellWidget(editBtn));
        connect(editBtn, &QPushButton::clicked, this, [this, step]() {
            // 弹窗修改，或进入编辑模式
            EditTaskDialog* dlg = new EditTaskDialog(EditMode::Edit,step,currentItem.id,nullptr);
            dlg->show();
            connect(dlg, &EditTaskDialog::accepted,[dlg, this]() {
                QJsonObject data = dlg->resultData();
                if (data.isEmpty()) {
                    return;
                }

                // 保存回 JSON 文件 & 刷新表格
                if (data["type"].toString() == "OPENCV") {
                    saveBase64ImageToFile(data);
                }
                updateConfigInJsonFile(AppPaths::instance().configPath(), currentItem.id, data);
                QTimer::singleShot(0, dlg, &QObject::deleteLater); // 延迟一拍
                showCurrentSelectStepsInTable();
            });
            connect(dlg, &EditTaskDialog::imagePathRequested, this, &mainwindow::showOpenCVIdentifyImage);
        });

        // 删除按钮
        QPushButton *delBtn = new QPushButton("删除");
        ui->tableWidget->setCellWidget(i, 4, wrapCellWidget(delBtn));
        connect(delBtn, &QPushButton::clicked, this, [this, step]() {
            // 添加确认弹窗
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "确认删除",
                                        "确定要删除这个步骤吗？",
                                        QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                // qDebug() << "点击了删除: 行=" << i << " id=" << step["stepsId"].toString();
                removeConfigById(AppPaths::instance().configPath(), currentItem.id, step["stepsId"].toString());
                showCurrentSelectStepsInTable();
            }
        });

    }

    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->resizeRowsToContents();
}

void mainwindow::showCurrentSelectStepsInTable()
{
    refreshConfig();
    if (currentItem.id.isEmpty())
    {
        Logger::log(QString("当前选择方案ID为空"));
        return;
    }


    bool hasConfigId = false;
    for (int i = 0; i < m_configArray.size(); ++i)
    {
        QJsonObject obj = m_configArray[i].toObject();
        if (obj["id"].toString() == currentItem.id)
        {
            showStepsInTable(obj["steps"].toArray());
            hasConfigId = true;
            break;
        }
    }

    if (!hasConfigId)
    {
        Logger::log("没有ConfigId: " + currentItem.id);
    }
}

//开启当前选中的任务
void mainwindow::startTaskButtonClick()
{
    if (currentItem.id.isEmpty())
    {
        Logger::log(QString("没有选中脚本方案"));
        return;
    }

    QJsonObject obj = QJsonObject(); //默认为空
    //读取对应的方案步骤数据
    for (const QJsonValue &val : m_configArray) {
        if (!val.isObject()) {
            continue;
        }
        obj = val.toObject();
        if (obj["id"].toString() == currentItem.id) {
            break;
        }
    }
    QJsonArray steps = obj["steps"].toArray();

    int cycleCount = ui->taskCycleNumber->text().toInt();
    TaskRunner::instance().run(steps, cycleCount);
}

//关闭当前选中的任务
void mainwindow::stopTaskButtonClick()
{
    TaskRunner::instance().stop();
}

void mainwindow::appendLogToUI(const QString &msg)
{
    if (!ui || !ui->plainTextEdit) return;

    ui->plainTextEdit->appendPlainText(msg);
    QTextCursor cursor = ui->plainTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->plainTextEdit->setTextCursor(cursor);
}

// 按钮点击槽函数
void mainwindow::onProgrammeAddBtnClicked()
{
    bool ok;
    QString configName = QInputDialog::getText(
        this,
        tr("添加方案"),
        tr("请输入方案名称:"),
        QLineEdit::Normal,
        "",
        &ok
    );

    if (ok && !configName.isEmpty()) {
        addConfigToJsonFile(AppPaths::instance().configPath(), configName);
        // 输入了有效内容
        Logger::log("已添加新的配置: " + configName);
        loadListWidgetData();
    } else {
        // 用户取消或输入为空
        Logger::log(QString("用户未输入或无效的配置名称"));
    }
}

// 按钮点击槽函数
void mainwindow::onProgrammeRemoveBtnClicked()
{
    if (currentItem.id.isEmpty())
    {
        Logger::log(QString("未选中配置"));
        return;
    }
    // 弹出确认对话框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        this,
        tr("确认删除"),
        tr("确定要删除配置 “%1” 吗？").arg(currentItem.taskName),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes)
    {
        if (removeConfigById(AppPaths::instance().configPath(), currentItem.id))
        {
            Logger::log("已删除配置: " + currentItem.taskName);
            loadListWidgetData();  // 刷新配置
            commonSetCurrentItem("","");
            ui->currentTaskName->setText("");
            if (m_configArray.size() > 0)
            {
                const QString id = m_configArray[0].toObject()["id"].toString();
                const QString name = m_configArray[0].toObject()["name"].toString();
                commonSetCurrentItem(id,name);
                ui->currentTaskName->setText(name);
            }
        }
        else
        {
            Logger::log("删除配置失败: " + currentItem.taskName);
        }
    }
    else
    {
        Logger::log(QString("已取消删除"));
    }
}

void mainwindow::onSettingBtnClicked()
{
    SettingDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // 设置已保存和应用
        Logger::log(QString("Setting配置已修改"));
    }
}


void mainwindow::onProgrammeContentAddBtnClicked()
{
    QJsonObject empty;
    EditTaskDialog* dlg = new EditTaskDialog(EditMode::Add,empty,currentItem.id,nullptr); // 非模态
    dlg->show();
    connect(dlg, &EditTaskDialog::accepted, [dlg, this]() {
        QJsonObject data = dlg->resultData();
        // 保存 JSON
        if (data.isEmpty()) {
            return;
        }

        if (data["type"].toString() == "OPENCV") {
            saveBase64ImageToFile(data);
        }
        addConfigToJsonFile(AppPaths::instance().configPath(),currentItem.id,data);
        QTimer::singleShot(0, dlg, &QObject::deleteLater);  // 延迟一拍
        showCurrentSelectStepsInTable();
    });
    connect(dlg, &EditTaskDialog::imagePathRequested, this, &mainwindow::showOpenCVIdentifyImage);
}

void mainwindow::onProgrammeUpBtnClicked()
{
    int currentRow = ui->tableWidget->currentRow();
    if (currentRow >= 0) {
        moveProgramme(AppPaths::instance().configPath(), currentItem.id, currentRow, true);
        showCurrentSelectStepsInTable();
        ui->tableWidget->selectRow(currentRow-1);
    } else {
        Logger::log(QString("没有选择方案内容的任何行"));
    }
}

void mainwindow::onProgrammeDownBtnClicked()
{
    int currentRow = ui->tableWidget->currentRow();
    if (currentRow >= 0) {
        moveProgramme(AppPaths::instance().configPath(), currentItem.id, currentRow, false);
        showCurrentSelectStepsInTable();
        ui->tableWidget->selectRow(currentRow+1);
    } else {
        Logger::log(QString("没有选择方案内容的任何行"));
    }
}

void mainwindow::showOpenCVIdentifyImage(const QString& savePath) const
{
    if (savePath.isEmpty()) {
        qWarning() << "[WARN] showOpenCVIdentifyImage: 路径为空";
        ui->openCVIdentifyLabel->clear();
        ui->openCVIdentifyLabel->setText("无图像");
        return;
    }

    // 用 OpenCV 读取图像
    cv::Mat img = vision::imreadQt(savePath);
    if (img.empty()) {
        qWarning() << "[ERROR] 无法加载图片:" << savePath;
        ui->openCVIdentifyLabel->clear();
        ui->openCVIdentifyLabel->setText("加载失败");
        return;
    }

    // 转换为 Qt 可识别格式（BGR → RGB）
    cv::cvtColor(img, img, cv::COLOR_BGR2RGB);

    // 封装成 QImage（不拷贝数据）
    QImage qimg(
        img.data,
        img.cols,
        img.rows,
        static_cast<int>(img.step),
        QImage::Format_RGB888
    );

    // 缩放显示：保持比例完整显示在 QLabel 内
    QSize labelSize = ui->openCVIdentifyLabel->size();
    QPixmap pixmap = QPixmap::fromImage(qimg).scaled(
        labelSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    // 设置显示
    ui->openCVIdentifyLabel->setPixmap(pixmap);
    ui->openCVIdentifyLabel->setAlignment(Qt::AlignCenter);
    ui->openCVIdentifyLabel->setScaledContents(false);  // 不拉伸变形
}
