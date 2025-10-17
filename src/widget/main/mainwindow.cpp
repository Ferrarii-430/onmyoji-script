//
// Created by CZY on 2025/9/25.
//

// You may need to build the project (run Qt uic code generator) to get "ui_mainwindow.h" resolved

#include "mainwindow.h"
#include "Logger.h"
#include <iostream>
#include <windows.h>
#include "ui_mainwindow.h"
#include <QFile>
#include <QJsonArray>
#include <QTimer>
#include <QDebug>
#include <opencv2/opencv.hpp>
#include <ConfigTypeEnum.h>
#include <ExecutionSteps.h>
#include <QInputDialog>

#include "ConfigManager.h"
#include "src/utils/common.h"
#include "QMessageBox"
#include "SettingManager.h"
#include "src/widget/editTask/edittaskdialog.h"
#include "src/widget/setting/settingdialog.h"

//TODO 🐶💩代码 有空我一定重构

mainwindow::mainwindow(QWidget *parent) :
    QWidget(parent), ui(new Ui::mainwindow) {
    ui->setupUi(this);

    // 设置全局 Logger 的 mainwindow 指针
    Logger::setMainWindow(this);

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
        updateProgrammeContent(CONFIG_PATH, currentItem.id, item->text());
        loadListWidgetData();
    });

    //读取setting的配置
    if (SETTING_CONFIG.loadConfig())
    {
        Logger::log(QString("Setting配置加载成功！"));
    }

    //读取方案的配置
    loadListWidgetData();

    Logger::log(QString("Config配置加载成功！"));

    // 检查OpenCV版本和编译选项
    Logger::log(QString("OpenCV版本 %1").arg(CV_VERSION));
}

mainwindow::~mainwindow() {
    delete ui;
}

//加载配置文件
void mainwindow::loadConfig()
{
    QFile file(CONFIG_PATH);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::log(QString("无法打开配置文件：" + file.errorString() + "路径:" + CONFIG_PATH));
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return;

    QJsonArray arr = doc.array();

    // 保存全局数据
    m_configArray = arr;
}

void mainwindow::loadListWidgetData()
{
    loadConfig();

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
        setCurrentItem(id, name);
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
    setCurrentItem(id, name);

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
    showStepsInTable(steps);
}

//加载数据到任务表格
void mainwindow::showStepsInTable(const QJsonArray &steps) {
    ui->tableWidget->clear(); // 清空表格
    ui->tableWidget->setRowCount(steps.size());
    ui->tableWidget->setColumnCount(5); // 新增序号列
    QStringList headers = {"序号", "任务名称", "类型", "操作",""};
    ui->tableWidget->setHorizontalHeaderLabels(headers);

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
        ui->tableWidget->setCellWidget(i, 3, editBtn);
        connect(editBtn, &QPushButton::clicked, this, [this, step]() {
            // 弹窗修改，或进入编辑模式
            EditTaskDialog* dlg = new EditTaskDialog(EditMode::Edit,step,nullptr);
            dlg->show();
            connect(dlg, &EditTaskDialog::accepted,[dlg, this]() {
                QJsonObject data = dlg->resultData();
                // 保存回 JSON 文件 & 刷新表格
                saveBase64ImageToFile(data);
                updateConfigInJsonFile(CONFIG_PATH, currentItem.id, data);
                QTimer::singleShot(0, dlg, &QObject::deleteLater); // 延迟一拍
                showCurrentSelectStepsInTable();
            });
        });

        // 删除按钮
        QPushButton *delBtn = new QPushButton("删除");
        ui->tableWidget->setCellWidget(i, 4, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, step]() {
            // 添加确认弹窗
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "确认删除",
                                        "确定要删除这个步骤吗？",
                                        QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                // qDebug() << "点击了删除: 行=" << i << " id=" << step["stepsId"].toString();
                removeConfigById(CONFIG_PATH, currentItem.id, step["stepsId"].toString());
                showCurrentSelectStepsInTable();
            }
        });

    }

    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->resizeRowsToContents();
}

void mainwindow::showCurrentSelectStepsInTable()
{
    loadConfig();
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

    bool hasHWND = ExecutionSteps::getInstance().checkHWNDHandle();
    if (!hasHWND)
    {
        Logger::log(QString("窗口未找到"));
        return;
    }

    //获取最新配置
    loadConfig();

    QJsonObject obj = QJsonObject(); //默认为空
    //读取对应的方案步骤数据到表格
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

    if (steps.size() == 0)
    {
        Logger::log(QString("当前方案的内容为空，任务停止"));
        return;
    }

    if (m_isRunning) {
        Logger::log(QString("任务已在运行中"));
        return;
    }

    if (!isInitLogPath)
    {
        if (ExecutionSteps::getInstance().dllSetLogPath())
        {
            Logger::log(QString("已修改dll日志路径: ") + ConfigManager::instance().dx11LogPath());
        }else
        {
            Logger::log(QString("dll日志路径修改失败: ") + ConfigManager::instance().dx11LogPath());
        }
        isInitLogPath = true;
    }

    m_isRunning = true;
    ui->startTaskButton->setEnabled(false);
    ui->stopTaskButton->setEnabled(true);

    int number = ui->taskCycleNumber->text().toInt();
    bool infiniteLoop = (number <= 0);
    Logger::log(QString("循环次数: %1").arg(infiniteLoop ? "无限" : QString::number(number)));

    do {
        for (const QJsonValue &val : steps)
        {
            QJsonObject step = val.toObject();
            QString typeStr = step["type"].toString();
            ConfigTypeEnum type = stringToConfigType(typeStr);

            switch (type) {
                case ConfigTypeEnum::OPENCV: {
                        Logger::log(QString("开始进行OpenCV识图"));
                        QString imagePath = step["imagePath"].toString();
                        const double score = step["score"].toDouble();
                        const bool randomClick = step["randomClick"].toBool();
                        int retryCount = 0;

                        QString savePath;
                        while (retryCount < 3) {
                            savePath = ExecutionSteps::getInstance().opencvRecognizesAndClick(imagePath, score, randomClick);
                            if (!savePath.isNull()) {
                                break; // 成功
                            }

                            retryCount++;
                            if (retryCount < 3) {
                                Logger::log(QString("截图失败，第%1次重试").arg(retryCount));
                                Sleep(1000); // 等待1秒后重试
                            }
                        }


                        if (savePath.isEmpty())
                        {
                            Logger::log(QString("识别失败，跳过当次循环！"));
                            continue;
                        }
                        showOpenCVIdentifyImage(savePath);
                        break;
                }

                case ConfigTypeEnum::WAIT: {
                        Logger::log(QString("等待%1毫秒...").arg(step["time"].toInt()));
                        int waitTime = step["time"].toInt();
                        int interval = 100; // 每100ms检查一次
                        for (int t = 0; t < waitTime && m_isRunning; t += interval) {
                            Sleep(qMin(interval, waitTime - t));
                            QCoreApplication::processEvents();
                        }
                        break;
                }

                default: {
                        Logger::log(QString("未知的命令：%1").arg(typeStr));
                        break;
                }
            }
        }

        if (!infiniteLoop) {
            number--;
        }

        //删除掉截图
        // ExecutionSteps::getInstance().deleteCaptureFile();

        //每次任务结束都固定休眠1秒，防止无限循环一直执行
        Sleep(1000);

    } while (m_isRunning && (infiniteLoop || number > 0));

    // 执行结束
    m_isRunning = false;
    ui->startTaskButton->setEnabled(true);
    ui->stopTaskButton->setEnabled(false);

    Logger::log(QString("任务循环结束"));

    //卸载dll异常 暂不使用
    // ExecutionSteps::getInstance().dllStopHook();
}

//关闭当前选中的任务
void mainwindow::stopTaskButtonClick()
{
    m_isRunning = false;
    Logger::log(QString("正在停止任务..."));
}

//修改当前选择的方案
void mainwindow::setCurrentItem(const QString &id, const QString &taskName)
{
    currentItem.id = id;
    currentItem.taskName = taskName;
    ui->currentTaskName->setText(currentItem.taskName);
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
        addConfigToJsonFile(CONFIG_PATH, configName);
        // 输入了有效内容
        Logger::log("已添加新的配置: " + configName);
        loadConfig();
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
        if (removeConfigById(CONFIG_PATH, currentItem.id))
        {
            Logger::log("已删除配置: " + currentItem.taskName);
            loadListWidgetData();  // 刷新配置
            setCurrentItem("", ""); //清空当前选择
            if (m_configArray.size() > 0)
            {
                const QString id = m_configArray[0].toObject()["id"].toString();
                const QString name = m_configArray[0].toObject()["name"].toString();
                setCurrentItem(id, name);
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
    EditTaskDialog* dlg = new EditTaskDialog(EditMode::Add,empty,nullptr); // 非模态
    dlg->show();
    connect(dlg, &EditTaskDialog::accepted, [dlg, this]() {
        QJsonObject data = dlg->resultData();
        // 保存 JSON
        saveBase64ImageToFile(data);
        addConfigToJsonFile(CONFIG_PATH,currentItem.id,data);
        QTimer::singleShot(0, dlg, &QObject::deleteLater);  // 延迟一拍
        showCurrentSelectStepsInTable();
    });
}

void mainwindow::onProgrammeUpBtnClicked()
{
    int currentRow = ui->tableWidget->currentRow();
    if (currentRow >= 0) {
        moveProgramme(CONFIG_PATH, currentItem.id, currentRow, true);
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
        moveProgramme(CONFIG_PATH, currentItem.id, currentRow, false);
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

    // 1️⃣ 用 OpenCV 读取图像
    cv::Mat img = cv::imread(savePath.toStdString());
    if (img.empty()) {
        qWarning() << "[ERROR] 无法加载图片:" << savePath;
        ui->openCVIdentifyLabel->clear();
        ui->openCVIdentifyLabel->setText("加载失败");
        return;
    }

    // 2️⃣ 转换为 Qt 可识别格式（BGR → RGB）
    cv::cvtColor(img, img, cv::COLOR_BGR2RGB);

    // 3️⃣ 封装成 QImage（不拷贝数据）
    QImage qimg(
        img.data,
        img.cols,
        img.rows,
        static_cast<int>(img.step),
        QImage::Format_RGB888
    );

    // 4️⃣ 缩放显示：保持比例完整显示在 QLabel 内
    QSize labelSize = ui->openCVIdentifyLabel->size();
    QPixmap pixmap = QPixmap::fromImage(qimg).scaled(
        labelSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    // 5️⃣ 设置显示
    ui->openCVIdentifyLabel->setPixmap(pixmap);
    ui->openCVIdentifyLabel->setAlignment(Qt::AlignCenter);
    ui->openCVIdentifyLabel->setScaledContents(false);  // 不拉伸变形
}