/********************************************************************************
** Form generated from reading UI file 'settingdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETTINGDIALOG_H
#define UI_SETTINGDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SettingDialog
{
public:
    QWidget *horizontalLayoutWidget;
    QHBoxLayout *horizontalLayout;
    QToolButton *btnSave;
    QToolButton *btnCancel;
    QWidget *formLayoutWidget;
    QFormLayout *formLayout;
    QLabel *label;
    QComboBox *mouseControlMode;
    QLabel *label_2;
    QComboBox *screenshotMode;
    QLabel *label_3;
    QSpinBox *mouseSpeed;
    QLabel *label_4;
    QComboBox *mouseClickMode;

    void setupUi(QDialog *SettingDialog)
    {
        if (SettingDialog->objectName().isEmpty())
            SettingDialog->setObjectName("SettingDialog");
        SettingDialog->resize(400, 300);
        horizontalLayoutWidget = new QWidget(SettingDialog);
        horizontalLayoutWidget->setObjectName("horizontalLayoutWidget");
        horizontalLayoutWidget->setGeometry(QRect(120, 260, 171, 31));
        horizontalLayout = new QHBoxLayout(horizontalLayoutWidget);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        btnSave = new QToolButton(horizontalLayoutWidget);
        btnSave->setObjectName("btnSave");

        horizontalLayout->addWidget(btnSave);

        btnCancel = new QToolButton(horizontalLayoutWidget);
        btnCancel->setObjectName("btnCancel");

        horizontalLayout->addWidget(btnCancel);

        formLayoutWidget = new QWidget(SettingDialog);
        formLayoutWidget->setObjectName("formLayoutWidget");
        formLayoutWidget->setGeometry(QRect(50, 20, 301, 211));
        formLayout = new QFormLayout(formLayoutWidget);
        formLayout->setObjectName("formLayout");
        formLayout->setHorizontalSpacing(20);
        formLayout->setVerticalSpacing(10);
        formLayout->setContentsMargins(0, 0, 0, 0);
        label = new QLabel(formLayoutWidget);
        label->setObjectName("label");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, label);

        mouseControlMode = new QComboBox(formLayoutWidget);
        mouseControlMode->setObjectName("mouseControlMode");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, mouseControlMode);

        label_2 = new QLabel(formLayoutWidget);
        label_2->setObjectName("label_2");

        formLayout->setWidget(3, QFormLayout::ItemRole::LabelRole, label_2);

        screenshotMode = new QComboBox(formLayoutWidget);
        screenshotMode->setObjectName("screenshotMode");

        formLayout->setWidget(3, QFormLayout::ItemRole::FieldRole, screenshotMode);

        label_3 = new QLabel(formLayoutWidget);
        label_3->setObjectName("label_3");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, label_3);

        mouseSpeed = new QSpinBox(formLayoutWidget);
        mouseSpeed->setObjectName("mouseSpeed");
        mouseSpeed->setMaximum(10);

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, mouseSpeed);

        label_4 = new QLabel(formLayoutWidget);
        label_4->setObjectName("label_4");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, label_4);

        mouseClickMode = new QComboBox(formLayoutWidget);
        mouseClickMode->setObjectName("mouseClickMode");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, mouseClickMode);


        retranslateUi(SettingDialog);

        QMetaObject::connectSlotsByName(SettingDialog);
    } // setupUi

    void retranslateUi(QDialog *SettingDialog)
    {
        SettingDialog->setWindowTitle(QCoreApplication::translate("SettingDialog", "\350\256\276\347\275\256", nullptr));
        btnSave->setText(QCoreApplication::translate("SettingDialog", "\345\272\224\347\224\250/\344\277\235\345\255\230", nullptr));
        btnCancel->setText(QCoreApplication::translate("SettingDialog", "\345\217\226\346\266\210", nullptr));
        label->setText(QCoreApplication::translate("SettingDialog", "\351\274\240\346\240\207\346\216\247\345\210\266\346\250\241\345\274\217", nullptr));
        label_2->setText(QCoreApplication::translate("SettingDialog", "\350\216\267\345\217\226\346\210\252\345\233\276\346\226\271\345\274\217", nullptr));
        label_3->setText(QCoreApplication::translate("SettingDialog", "\351\274\240\346\240\207\351\200\237\345\272\246", nullptr));
        label_4->setText(QCoreApplication::translate("SettingDialog", "\351\274\240\346\240\207\347\202\271\345\207\273\346\250\241\345\274\217", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SettingDialog: public Ui_SettingDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGDIALOG_H
