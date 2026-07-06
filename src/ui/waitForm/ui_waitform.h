/********************************************************************************
** Form generated from reading UI file 'waitform.ui'
**
** Created by: Qt User Interface Compiler version 6.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WAITFORM_H
#define UI_WAITFORM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_WaitForm
{
public:
    QWidget *formLayoutWidget;
    QFormLayout *formLayout;
    QLabel *label;
    QSpinBox *spinBox;
    QLabel *label_2;
    QLineEdit *lineTaskNameEdit;
    QLabel *label_3;
    QCheckBox *randomWaitCheckBox;
    QLabel *randomSleepTimeLabel;
    QSpinBox *randomSleepTimeSpinBox;

    void setupUi(QWidget *WaitForm)
    {
        if (WaitForm->objectName().isEmpty())
            WaitForm->setObjectName("WaitForm");
        WaitForm->resize(291, 311);
        formLayoutWidget = new QWidget(WaitForm);
        formLayoutWidget->setObjectName("formLayoutWidget");
        formLayoutWidget->setGeometry(QRect(0, 0, 291, 311));
        formLayout = new QFormLayout(formLayoutWidget);
        formLayout->setObjectName("formLayout");
        formLayout->setHorizontalSpacing(20);
        formLayout->setVerticalSpacing(6);
        formLayout->setContentsMargins(0, 0, 0, 0);
        label = new QLabel(formLayoutWidget);
        label->setObjectName("label");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, label);

        spinBox = new QSpinBox(formLayoutWidget);
        spinBox->setObjectName("spinBox");
        spinBox->setMaximum(60000);
        spinBox->setSingleStep(100);

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, spinBox);

        label_2 = new QLabel(formLayoutWidget);
        label_2->setObjectName("label_2");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, label_2);

        lineTaskNameEdit = new QLineEdit(formLayoutWidget);
        lineTaskNameEdit->setObjectName("lineTaskNameEdit");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, lineTaskNameEdit);

        label_3 = new QLabel(formLayoutWidget);
        label_3->setObjectName("label_3");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, label_3);

        randomWaitCheckBox = new QCheckBox(formLayoutWidget);
        randomWaitCheckBox->setObjectName("randomWaitCheckBox");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, randomWaitCheckBox);

        randomSleepTimeLabel = new QLabel(formLayoutWidget);
        randomSleepTimeLabel->setObjectName("randomSleepTimeLabel");

        formLayout->setWidget(3, QFormLayout::ItemRole::LabelRole, randomSleepTimeLabel);

        randomSleepTimeSpinBox = new QSpinBox(formLayoutWidget);
        randomSleepTimeSpinBox->setObjectName("randomSleepTimeSpinBox");
        randomSleepTimeSpinBox->setMaximum(60000);
        randomSleepTimeSpinBox->setSingleStep(100);

        formLayout->setWidget(3, QFormLayout::ItemRole::FieldRole, randomSleepTimeSpinBox);


        retranslateUi(WaitForm);

        QMetaObject::connectSlotsByName(WaitForm);
    } // setupUi

    void retranslateUi(QWidget *WaitForm)
    {
        WaitForm->setWindowTitle(QCoreApplication::translate("WaitForm", "waitForm", nullptr));
        label->setText(QCoreApplication::translate("WaitForm", "\347\255\211\345\276\205\346\227\266\351\227\264\357\274\210\346\257\253\347\247\222\357\274\211", nullptr));
        label_2->setText(QCoreApplication::translate("WaitForm", "\344\273\273\345\212\241\345\220\215\347\247\260", nullptr));
        label_3->setText(QCoreApplication::translate("WaitForm", "\346\230\257\345\220\246\351\232\217\346\234\272\347\255\211\345\276\205", nullptr));
        randomWaitCheckBox->setText(QString());
        randomSleepTimeLabel->setText(QCoreApplication::translate("WaitForm", "\345\201\217\347\247\273\351\232\217\346\234\272\346\227\266\351\227\264", nullptr));
    } // retranslateUi

};

namespace Ui {
    class WaitForm: public Ui_WaitForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WAITFORM_H
