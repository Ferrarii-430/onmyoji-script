//
// Created by CZY on 2025/10/15.
//

// You may need to build the project (run Qt uic code generator) to get "ui_SettingDialog.h" resolved

#include "settingdialog.h"

#include <QDir>
#include <QMessageBox>

#include "Logger.h"
#include "SettingManager.h"
#include "ui_SettingDialog.h"


SettingDialog::SettingDialog(QWidget *parent) :
    QDialog(parent), ui(new Ui::SettingDialog) {
    ui->setupUi(this);

    initSetting();

    // 连接信号槽
    connect(ui->btnSave, &QToolButton::clicked, this, &SettingDialog::onSaveClicked);
    connect(ui->btnCancel, &QToolButton::clicked, this, &SettingDialog::onCancelClicked);

    // 保存原始值
    m_originalMouseMode = SETTING_CONFIG.getMouseControlMode();
    m_originalMouseSpeed = SETTING_CONFIG.getMouseSpeed();
    m_originalScreenshotMode = SETTING_CONFIG.getScreenshotMode();
    m_originalScreenshotMode = SETTING_CONFIG.getMouseClickMode();
}

SettingDialog::~SettingDialog() {
    delete ui;
}

void SettingDialog::initSetting() const
{
    ui->mouseControlMode->addItem("直线移动", "LINEAR");
    ui->mouseControlMode->addItem("贝塞尔曲线", "BEZIER");
    ui->mouseControlMode->addItem("S形曲线", "S_CURVE");
    ui->mouseControlMode->addItem("随机漫步", "RANDOM_WALK");

    ui->screenshotMode->addItem("PrintWindow", "PrintWindow");
    ui->screenshotMode->addItem("DirectX截图", "DirectX截图");

    ui->mouseClickMode->addItem("PostMessage", "PostMessage");
    ui->mouseClickMode->addItem("InputMouse", "InputMouse");

    //初始化值
    ui->mouseControlMode->setCurrentText(SETTING_CONFIG.getMouseControlMode());
    ui->mouseSpeed->setValue(SETTING_CONFIG.getMouseSpeed());
    ui->screenshotMode->setCurrentText(SETTING_CONFIG.getScreenshotMode());
    ui->mouseClickMode->setCurrentText(SETTING_CONFIG.getMouseClickMode());

    // 根据配置值设置当前选项
    QString currentMouseMode = SETTING_CONFIG.getMouseControlMode();
    int mouseModeIndex = ui->mouseControlMode->findData(currentMouseMode);
    if (mouseModeIndex >= 0) {
        ui->mouseControlMode->setCurrentIndex(mouseModeIndex);
    } else {
        // 如果配置值不在选项中，使用默认值
        ui->mouseControlMode->setCurrentIndex(1); // 默认贝塞尔曲线
        qWarning() << "未找到匹配的鼠标控制模式:" << currentMouseMode << "，使用默认值";
    }

    // 设置鼠标速度
    ui->mouseSpeed->setValue(SETTING_CONFIG.getMouseSpeed());

    // 设置截图模式
    QString currentScreenshotMode = SETTING_CONFIG.getScreenshotMode();
    int screenshotIndex = ui->screenshotMode->findData(currentScreenshotMode);
    if (screenshotIndex >= 0) {
        ui->screenshotMode->setCurrentIndex(screenshotIndex);
    } else {
        // 如果配置值不在选项中，使用默认值
        ui->screenshotMode->setCurrentIndex(0); // 默认PrintWindow
        qWarning() << "未找到匹配的截图模式:" << currentScreenshotMode << "，使用默认值";
    }

    // 设置鼠标点击模式
    QString currentMouseClickMode = SETTING_CONFIG.getMouseClickMode();
    int mouseClickModeIndex = ui->mouseClickMode->findData(currentMouseClickMode);
    if (mouseClickModeIndex >= 0) {
        ui->mouseClickMode->setCurrentIndex(mouseClickModeIndex);
    } else {
        // 如果配置值不在选项中，使用默认值
        ui->mouseClickMode->setCurrentIndex(0); // 默认PostMessage
        qWarning() << "未找到匹配的鼠标点击模式:" << currentMouseClickMode << "，使用默认值";
    }
}

void SettingDialog::onSaveClicked()
{
    // 获取当前界面值
    QString mouseMode = ui->mouseControlMode->currentData().toString();
    int mouseSpeed = ui->mouseSpeed->value();
    QString screenshotMode = ui->screenshotMode->currentData().toString();

    // 验证数据
    if (mouseSpeed < 1 || mouseSpeed > 10) {
        QMessageBox::warning(this, "输入错误", "鼠标速度必须在1-10之间");
        ui->mouseSpeed->setFocus();
        return;
    }

    // 创建配置对象
    QJsonObject config;
    config["mouseControlMode"] = mouseMode;
    config["mouseSpeed"] = mouseSpeed;
    config["screenshotMode"] = screenshotMode;

    // 保存到文件
    if (saveConfigToFile(config)) {
        // 重新加载全局配置
        if (SETTING_CONFIG.reloadConfig()) {
            // 应用设置到系统
            applySettings();

            QMessageBox::information(this, "成功", "设置已保存并应用");
            accept(); // 关闭对话框
        } else {
            QMessageBox::critical(this, "错误", "配置重新加载失败，请重启程序");
        }
    } else {
        QMessageBox::critical(this, "保存失败", "无法保存配置文件");
    }
}

bool SettingDialog::saveConfigToFile(const QJsonObject& config)
{
    QString basePath = QCoreApplication::applicationDirPath();
    QString configPath = basePath + "/src/resource/setting.json";

    // 确保目录存在
    QDir configDir = QFileInfo(configPath).absoluteDir();
    if (!configDir.exists()) {
        if (!configDir.mkpath(".")) {
            qWarning() << "无法创建配置目录:" << configDir.absolutePath();
            return false;
        }
    }

    QFile configFile(configPath);
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法打开配置文件进行写入:" << configPath;
        return false;
    }

    QJsonDocument configDoc(config);
    qint64 bytesWritten = configFile.write(configDoc.toJson(QJsonDocument::Indented));
    configFile.close();

    if (bytesWritten <= 0) {
        qWarning() << "配置文件写入失败:" << configPath;
        return false;
    }

    qInfo() << "配置文件保存成功:" << configPath;
    return true;
}

void SettingDialog::applySettings()
{
    // 获取最新配置值
    QString mouseMode = SETTING_CONFIG.getMouseControlMode();
    int mouseSpeed = SETTING_CONFIG.getMouseSpeed();
    QString screenshotMode = SETTING_CONFIG.getScreenshotMode();

    qDebug() << "应用新设置:"
             << "鼠标模式=" << mouseMode
             << "鼠标速度=" << mouseSpeed
             << "截图模式=" << screenshotMode;

    // 这里可以添加应用设置到系统的逻辑
    // 例如：通知其他模块配置已更新

    // 发出全局信号通知配置变更
    // emit settingsChanged();

    // 调用其他模块的配置更新方法
    // MouseSimulator::getInstance().updateConfig();
    // ScreenshotManager::getInstance().updateConfig();
}

void SettingDialog::onCancelClicked()
{
    reject(); // 关闭对话框
}