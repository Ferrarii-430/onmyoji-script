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
#include <QRandomGenerator>
#include <src/utils/common.h>

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
            EditTaskDialog* dlg = new EditTaskDialog(EditMode::Edit,step,currentItem.id,nullptr);
            dlg->show();
            connect(dlg, &EditTaskDialog::accepted,[dlg, this]() {
                QJsonObject data = dlg->resultData();
                if (data.isEmpty()) {
                    return;
                }

                // 保存回 JSON 文件 & 刷新表格
                saveBase64ImageToFile(data);
                updateConfigInJsonFile(CONFIG_PATH, currentItem.id, data);
                QTimer::singleShot(0, dlg, &QObject::deleteLater); // 延迟一拍
                showCurrentSelectStepsInTable();
            });
            connect(dlg, &EditTaskDialog::imagePathRequested, this, &mainwindow::showOpenCVIdentifyImage);
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
    // refreshConfig();

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
    int total = number;
    const bool infiniteLoop = (number <= 0);
    Logger::log(QString("任务循环次数: %1").arg(infiniteLoop ? "无限" : QString::number(number)));

    do {
        // 用于跟踪每个步骤的错误重试次数
        QMap<int, int> errorRetryMap;
        bool stopDoLoop = false; // 控制是否停止外部循环

        for (int i = 0; i < steps.size() && m_isRunning && !stopDoLoop; ++i)
        {
            QJsonObject step = steps[i].toObject();
            QString typeStr = step["type"].toString();
            ConfigTypeEnum type = stringToConfigType(typeStr);
            QString savePath;
            switch (type) {
                case ConfigTypeEnum::OPENCV: {
                        Logger::log(QString("开始进行OpenCV识图"));
                        QString imagePath = step["imagePath"].toString();
                        const double score = step["score"].toDouble();
                        const bool randomClick = step["randomClick"].toBool();
                        int retryCount = 0;

                        while (retryCount < 3) {
                            savePath = ExecutionSteps::getInstance().opencvRecognizesAndClick(imagePath, score, randomClick);
                            if (!savePath.isNull()) {
                                showOpenCVIdentifyImage(savePath);
                                break; // 成功
                            }

                            retryCount++;
                            if (retryCount < 3) {
                                Logger::log(QString("截图失败，第%1次重试").arg(retryCount));
                                Sleep(1000); // 等待1秒后重试
                            }
                        }
                        break;
                }

                case ConfigTypeEnum::OCR:{
                        Logger::log(QString("开始进行OCR识图"));
                        QString ocrText = step["ocrText"].toString();
                        const double score = step["score"].toDouble();
                        const bool randomClick = step["randomClick"].toBool();
                        int retryCount = 0;

                        while (retryCount < 3) {
                            savePath = ExecutionSteps::getInstance().ocrRecognizesAndClick(ocrText, score, randomClick);
                            if (!savePath.isNull()) {
                                showOpenCVIdentifyImage(savePath);
                                break; // 成功
                            }

                            retryCount++;
                            if (retryCount < 3) {
                                Logger::log(QString("截图失败，第%1次重试").arg(retryCount));
                                Sleep(1000); // 等待1秒后重试
                            }
                        }
                        break;
                }

                case ConfigTypeEnum::WAIT: {
                            int waitTime = step["time"].toInt();
                            bool randomWait = step["randomWait"].toBool();
                            int offsetTime = step["offsetTime"].toInt();

                            int actualWaitTime = waitTime;
                            if (randomWait && offsetTime > 0) {
                                // 生成在 [-offsetTime, offsetTime] 范围内的随机偏移量
                                int randomOffset = QRandomGenerator::global()->bounded(-offsetTime, offsetTime + 1);
                                actualWaitTime = qMax(0, waitTime + randomOffset); // 确保等待时间非负
                            }

                            // 记录实际等待时间，如果启用了随机等待则标注
                            if (randomWait) {
                                Logger::log(QString("等待%1毫秒（随机偏移，基础时间%2毫秒）...").arg(actualWaitTime).arg(waitTime));
                            } else {
                                Logger::log(QString("等待%1毫秒...").arg(actualWaitTime));
                            }

                            int interval = 100; // 每100ms检查一次
                            for (int t = 0; t < actualWaitTime && m_isRunning; t += interval) {
                                Sleep(qMin(interval, actualWaitTime - t));
                                QCoreApplication::processEvents();
                            }
                            break;
                }

                default: {
                        Logger::log(QString("未知的命令：%1").arg(typeStr));
                        break;
                }
            }

            //识别错误处理
            if (savePath.isEmpty() && type != ConfigTypeEnum::WAIT)
            {
                QString identifyErrorHandle = step["identifyErrorHandle"].toString();
                if (identifyErrorHandle == "next") {
                    //什么都不用做继续执行
                    Logger::log(QString("识别失败，继续执行下一个步骤"));
                } else if (identifyErrorHandle == "jump") {
                    //跳转到指定stepsId对应的步骤
                    if (step.contains("jumpStepsId") && !step["jumpStepsId"].toString().isEmpty()) {
                        QString jumpStepsId = step["jumpStepsId"].toString();
                        int targetIndex = -1;

                        // 遍历所有步骤，查找匹配的stepsId
                        for (int j = 0; j < steps.size(); ++j) {
                            QJsonObject currentStep = steps[j].toObject();
                            if (currentStep["stepsId"].toString() == jumpStepsId) {
                                targetIndex = j;
                                break;
                            }
                        }

                        if (targetIndex != -1) {
                            i = targetIndex - 1; // 设置i为targetIndex-1，因为循环会i++
                            Logger::log(QString("识别失败，跳转到步骤ID '%1' (索引 %2)").arg(jumpStepsId).arg(targetIndex));
                        } else {
                            Logger::log(QString("未找到步骤ID '%1'，使用默认next处理").arg(jumpStepsId));
                            // 默认为next
                        }
                    } else {
                        Logger::log(QString("跳转步骤ID未设置，使用默认next处理"));
                        // 默认为next
                    }
                } else if (identifyErrorHandle == "continue") {
                    //跳过for循环
                    Logger::log(QString("识别失败，跳过当前任务迭代的剩余步骤"));
                    break; // 跳出内部for循环，继续外部循环的下一个迭代
                } else if (identifyErrorHandle == "break") {
                    //直接停止do循环
                    Logger::log(QString("识别失败，停止整个任务循环"));
                    stopDoLoop = true;
                    break; // 跳出内部for循环
                } else if (identifyErrorHandle == "retry") {
                    //重新执行一次当前步骤
                    int &retryCount = errorRetryMap[i]; // 获取当前步骤的错误重试次数
                    if (retryCount < 3) { // 最大重试3次
                        retryCount++;
                        i = i - 1; // 重试当前步骤
                        Logger::log(QString("识别失败，第%1次重试当前步骤").arg(retryCount));
                        continue; // 跳过剩余代码，直接下一次迭代（重试）
                    } else {
                        Logger::log(QString("识别失败，重试次数用尽，继续下一个步骤"));
                        // 默认为next
                    }
                } else {
                    Logger::log(QString("未知的错误处理选项: %1，使用默认next处理").arg(identifyErrorHandle));
                    // 默认为next
                }
            }

            // 如果设置了stopDoLoop，跳出内部循环
            if (stopDoLoop) {
                break;
            }
        }

        // 如果外部循环需要停止，跳出
        if (stopDoLoop) {
            break;
        }

        if (!infiniteLoop) {
            number--;
            Logger::log(QString("当前任务执行次数:(%1/%2)").arg(QString::number(total-number), QString::number(total)));
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
        if (removeConfigById(CONFIG_PATH, currentItem.id))
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

        saveBase64ImageToFile(data);
        addConfigToJsonFile(CONFIG_PATH,currentItem.id,data);
        QTimer::singleShot(0, dlg, &QObject::deleteLater);  // 延迟一拍
        showCurrentSelectStepsInTable();
    });
    connect(dlg, &EditTaskDialog::imagePathRequested, this, &mainwindow::showOpenCVIdentifyImage);
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

    // 用 OpenCV 读取图像
    cv::Mat img = cv::imread(savePath.toStdString());
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