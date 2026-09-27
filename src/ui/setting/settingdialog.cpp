//
// Created by CZY on 2025/10/15.
//

// You may need to build the project (run Qt uic code generator) to get "ui_SettingDialog.h" resolved

#include "settingdialog.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QSaveFile>

#include "src/core/AppPaths.h"
#include "src/core/Logger.h"
#include "src/core/ProfileStore.h"
#include "src/core/SettingManager.h"
#include "src/core/UpdateChecker.h"
#include "src/engine/TaskRunner.h"
#include "ui_SettingDialog.h"


SettingDialog::SettingDialog(QWidget *parent) :
    QDialog(parent), ui(new Ui::SettingDialog) {
    ui->setupUi(this);

    initSetting();

    // GitHub 项目指引链接，点击用默认浏览器打开
    auto *githubLink = new QLabel(this);
    githubLink->setText("<a href=\"https://github.com/Ferrarii-430/onmyoji-script\" "
                        "style=\"color:#5b6cf0; text-decoration:none;\">GitHub 项目地址</a>");
    githubLink->setOpenExternalLinks(true);
    githubLink->setCursor(Qt::PointingHandCursor);
    githubLink->setToolTip("点击在浏览器中打开项目主页");
    githubLink->setGeometry(120, 320, 171, 20);
    githubLink->setAlignment(Qt::AlignCenter);

    // 手动检查更新链接：调用 UpdateChecker 立即向 Gitee 查询最新发行版。
    // setOpenExternalLinks(false) 让 linkActivated 信号在本进程内触发，而非打开浏览器。
    auto *checkUpdateLink = new QLabel(this);
    checkUpdateLink->setText("<a href=\"#\" "
                             "style=\"color:#5b6cf0; text-decoration:none;\">检查更新</a>");
    checkUpdateLink->setCursor(Qt::PointingHandCursor);
    checkUpdateLink->setToolTip("立即向 Gitee 查询最新发行版");
    checkUpdateLink->setGeometry(120, 352, 171, 20);
    checkUpdateLink->setAlignment(Qt::AlignCenter);
    connect(checkUpdateLink, &QLabel::linkActivated, this, [this]() {
        // notifyOnNoUpdate=true：无论结果如何都弹窗提示用户
        UpdateChecker::instance().checkAsync(true);
    });

    // 连接信号槽
    connect(ui->btnSave, &QToolButton::clicked, this, &SettingDialog::onSaveClicked);
    connect(ui->btnCancel, &QToolButton::clicked, this, &SettingDialog::onCancelClicked);
    connect(ui->configExportBtn, &QToolButton::clicked, this, &SettingDialog::onConfigExportClicked);
    connect(ui->configImportBtn, &QToolButton::clicked, this, &SettingDialog::onConfigImportClicked);

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
    ui->mouseClickMode->addItem("Hook点击", "Hook");

    //初始化值
    ui->mouseControlMode->setCurrentText(SETTING_CONFIG.getMouseControlMode());
    ui->mouseSpeed->setValue(SETTING_CONFIG.getMouseSpeed());
    ui->screenshotMode->setCurrentText(SETTING_CONFIG.getScreenshotMode());
    ui->mouseClickMode->setCurrentText(SETTING_CONFIG.getMouseClickMode());
    ui->persistScreenshot->setChecked(SETTING_CONFIG.getPersistScreenshot());
    ui->globalAutoCancelCollab->setChecked(SETTING_CONFIG.getGlobalAutoCancelCollab());

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
    QString mouseClickMode = ui->mouseClickMode->currentData().toString();
    bool persistScreenshot = ui->persistScreenshot->isChecked();
    bool globalAutoCancelCollab = ui->globalAutoCancelCollab->isChecked();

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
    config["mouseClickMode"] = mouseClickMode;
    config["persistScreenshot"] = persistScreenshot;
    config["globalAutoCancelCollab"] = globalAutoCancelCollab;

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
    // 统一走 AppPaths：带 8.3 短路径转换，中文安装路径下与其它资源路径行为一致
    QString configPath = AppPaths::instance().settingPath();

    // 确保目录存在
    QDir configDir = QFileInfo(configPath).absoluteDir();
    if (!configDir.exists()) {
        if (!configDir.mkpath(".")) {
            qWarning() << "无法创建配置目录:" << configDir.absolutePath();
            return false;
        }
    }

    // QSaveFile 原子写入：先写临时文件，commit() 时刷盘并原子替换，
    // 避免写一半进程被杀/断电导致 setting.json 损坏。
    QSaveFile configFile(configPath);
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法打开配置文件进行写入:" << configPath;
        return false;
    }

    QJsonDocument configDoc(config);
    qint64 bytesWritten = configFile.write(configDoc.toJson(QJsonDocument::Indented));

    if (bytesWritten <= 0 || !configFile.commit()) {
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

void SettingDialog::onConfigExportClicked()
{
    // 任务运行中方案配置可能被脚本写入，禁止导出避免导出中间状态
    if (TaskRunner::instance().isRunning()) {
        QMessageBox::information(this, QStringLiteral("导出配置"),
                                 QStringLiteral("任务运行中，请先停止任务再导出配置"));
        return;
    }

    const QString defaultName = QStringLiteral("onmyoji-config-%1.json")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    const QString target = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出配置"),
        QDir::homePath() + QStringLiteral("/") + defaultName,
        QStringLiteral("JSON 配置 (*.json)"));
    if (target.isEmpty()) {
        return; // 用户取消
    }

    const QString error = exportConfigToFile(target);
    if (error.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出配置"),
                                 QStringLiteral("配置已导出到：\n%1").arg(target));
    } else {
        QMessageBox::warning(this, QStringLiteral("导出配置"), error);
        Logger::log(QStringLiteral("配置导出失败: %1").arg(error));
    }
}

void SettingDialog::onConfigImportClicked()
{
    if (TaskRunner::instance().isRunning()) {
        QMessageBox::information(this, QStringLiteral("导入配置"),
                                 QStringLiteral("任务运行中，请先停止任务再导入配置"));
        return;
    }

    const QString source = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入配置"), QDir::homePath(),
        QStringLiteral("JSON 配置 (*.json);;所有文件 (*)"));
    if (source.isEmpty()) {
        return; // 用户取消
    }

    // 导入会整体替换现有方案，二次确认防止误操作
    const QMessageBox::StandardButton btn = QMessageBox::question(
        this, QStringLiteral("导入配置"),
        QStringLiteral("导入将用文件中的方案【整体替换】当前全部方案，\n"
                       "原配置会自动备份为 config.json.bak。\n确定继续吗？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (btn != QMessageBox::Yes) {
        return;
    }

    const QString error = importConfigFromFile(source);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("导入配置"), error);
        Logger::log(QStringLiteral("配置导入失败: %1").arg(error));
        return;
    }

    // 通知主窗口刷新方案列表与表单
    emit configImported();
    QMessageBox::information(this, QStringLiteral("导入配置"),
                             QStringLiteral("配置导入成功，方案列表已刷新。"));
}