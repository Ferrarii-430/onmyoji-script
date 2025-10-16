//
// Created by CZY on 2025/10/15.
//

#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>


QT_BEGIN_NAMESPACE
namespace Ui { class SettingDialog; }
QT_END_NAMESPACE

class SettingDialog : public QDialog {
Q_OBJECT

public:
    explicit SettingDialog(QWidget *parent = nullptr);
    ~SettingDialog() override;

private slots:
    void initSetting() const;  // 初始化设置显示
    void onSaveClicked();      // 保存配置
    void onCancelClicked();    // 取消

private:
    Ui::SettingDialog *ui;

    // 保存原始值用于取消操作
    QString m_originalMouseMode;
    int m_originalMouseSpeed;
    QString m_originalScreenshotMode;

    // 保存配置到文件
    bool saveConfigToFile(const QJsonObject& config);
    // 应用配置到系统
    void applySettings();
};


#endif //SETTINGDIALOG_H
