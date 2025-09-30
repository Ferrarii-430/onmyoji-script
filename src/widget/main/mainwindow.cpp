//
// Created by CZY on 2025/9/25.
//

// You may need to build the project (run Qt uic code generator) to get "ui_mainwindow.h" resolved

#include "mainwindow.h"
#include "Logger.h"
#include <iostream>

#include<windows.h>
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <opencv2/opencv.hpp>
#include <ConfigTypeEnum.h>
#include <ExecutionSteps.h>
#include <QInputDialog>
#include "src/utils/common.h"
#include "QMessageBox"
#include "src/widget/editTask/edittaskdialog.h"


mainwindow::mainwindow(QWidget *parent) :
    QWidget(parent), ui(new Ui::mainwindow) {
    ui->setupUi(this);

    // 设置全局 Logger 的 mainwindow 指针
    Logger::setMainWindow(this);

    // 测试
    // Logger::log(QString("程序启动"));

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

    loadConfig();

    loadListWidgetData();
}

mainwindow::~mainwindow() {
    delete ui;
}

//加载配置文件
void mainwindow::loadConfig()
{
    // 检查OpenCV版本和编译选项
    Logger::log(QString("OpenCV版本 %1").arg(CV_VERSION));
    Logger::log(QString("是否有AVX支持：%1").arg(CV_CPU_AVX));
    Logger::log(QString("是否有AVX2支持：%1").arg(CV_CPU_AVX2));

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

    appendLog("脚本配置加载成功！");
}

void mainwindow::loadListWidgetData()
{
    QJsonArray arr = m_configArray;
    for (int i = 0; i < arr.size(); i++) {
        QJsonObject obj = arr[i].toObject();

        QString name = obj["name"].toString();
        QString id   = obj["id"].toString();   // 假设 JSON 里有 id 字段

        // 创建列表项
        QListWidgetItem *item = new QListWidgetItem(name);

        // 绑定额外数据（Qt::UserRole 是给用户自定义数据用的）
        item->setData(Qt::UserRole, id);

        ui->listWidget->addItem(item);
    }

    if (arr.size() > 0)
    {
        QJsonObject step = arr[0].toObject();
        QString id = step["id"].toString();
        QString name = step["name"].toString();
        setCurrentItem(id, name);
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

//添加日志显示
void mainwindow::appendLog(const QString &log) const
{
    if (!ui->plainTextEdit) return;

    // 在末尾添加文本，并换行
    ui->plainTextEdit->appendPlainText(log);

    // 自动滚动到底部
    QTextCursor cursor = ui->plainTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->plainTextEdit->setTextCursor(cursor);
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
        connect(editBtn, &QPushButton::clicked, this, [this, i, step, taskName]() {
            qDebug() << "点击了编辑: 行=" << i << " taskName=" << taskName;
            // TODO: 弹窗修改，或进入编辑模式
            EditTaskDialog dlg(EditMode::Edit,step,this);
            if (dlg.exec() == QDialog::Accepted)
            {
                // 根据 typeCombo 的当前索引或者值读取表单数据
                QJsonObject newData = dlg.resultData();
                qDebug() << "编辑后数据:" << newData;
                // TODO: 保存回 JSON 文件 & 刷新表格
            }
        });

        // 删除按钮
        QPushButton *delBtn = new QPushButton("删除");
        ui->tableWidget->setCellWidget(i, 4, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, i, step, taskName]() {
            qDebug() << "点击了删除: 行=" << i << " id=" << step["id"].toString();
            // TODO: 删除 JSON 并刷新表格
        });

    }

    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->resizeRowsToContents();
}

//开启当前选中的任务
void mainwindow::startTaskButtonClick()
{
    std::cout << currentItem.id.toStdString() << "<-id" << std::endl;

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

    for (const QJsonValue &val : steps)
    {
        QJsonObject step = val.toObject();
        QString typeStr = step["type"].toString();

        ConfigTypeEnum type = stringToConfigType(typeStr);
        switch (type)
        {
            case ConfigTypeEnum::OPENCV:
                // TODO: 处理 OPENCV 类型
                Logger::log(QString("开始进行OpenCV识图"));
                ExecutionSteps::getInstance().opencvRecognizesAndClick(step["imagePath"].toString().toStdString());
                break;

            case ConfigTypeEnum::WAIT:
                // TODO: 处理 WAIT 类型
                Logger::log(QString("等待" + step["time"].toString() + "毫秒..."));
                Sleep(step["time"].toInt());
                break;

            default: appendLog(QString("未知的命令：" + typeStr));
        }
    }
}

//关闭当前选中的任务
void mainwindow::stopTaskButtonClick()
{

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
            loadConfig();  // 刷新配置
            setCurrentItem("", ""); //清空当前选择
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

void mainwindow::onProgrammeContentAddBtnClicked()
{
    QJsonObject empty;
    EditTaskDialog* dlg = new EditTaskDialog(EditMode::Add,empty,nullptr); // 非模态
    dlg->show();
    connect(dlg, &EditTaskDialog::accepted, [dlg]() {
        QJsonObject data = dlg->resultData();
        // 保存 JSON
        dlg->deleteLater(); // 这里直接用 dlg
        qDebug() << data.toVariantMap();
    });
}