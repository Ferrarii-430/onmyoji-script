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

    QString configPath = QCoreApplication::applicationDirPath() + "/src/resource/config.json";
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::log(QString("无法打开配置文件：" + file.errorString() + "路径:" + configPath));
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
    Logger::log(QString( "onItemClicked->选中项 name:" + name + ", id:" + id));
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
    ui->tableWidget->setColumnCount(3); // 新增序号列
    QStringList headers = {"序号", "任务名称", "类型"};
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