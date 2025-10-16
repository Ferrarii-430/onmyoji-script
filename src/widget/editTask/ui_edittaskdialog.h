/********************************************************************************
** Form generated from reading UI file 'edittaskdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_EDITTASKDIALOG_H
#define UI_EDITTASKDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>

QT_BEGIN_NAMESPACE

class Ui_EditTaskDialog
{
public:
    QDialogButtonBox *buttonBox;
    QLabel *label;
    QComboBox *comboBox;
    QStackedWidget *stackedWidget;

    void setupUi(QDialog *EditTaskDialog)
    {
        if (EditTaskDialog->objectName().isEmpty())
            EditTaskDialog->setObjectName("EditTaskDialog");
        EditTaskDialog->resize(479, 496);
        buttonBox = new QDialogButtonBox(EditTaskDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setGeometry(QRect(150, 450, 161, 23));
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);
        label = new QLabel(EditTaskDialog);
        label->setObjectName("label");
        label->setGeometry(QRect(80, 30, 48, 16));
        comboBox = new QComboBox(EditTaskDialog);
        comboBox->setObjectName("comboBox");
        comboBox->setGeometry(QRect(150, 30, 211, 23));
        stackedWidget = new QStackedWidget(EditTaskDialog);
        stackedWidget->setObjectName("stackedWidget");
        stackedWidget->setGeometry(QRect(80, 80, 291, 311));

        retranslateUi(EditTaskDialog);

        QMetaObject::connectSlotsByName(EditTaskDialog);
    } // setupUi

    void retranslateUi(QDialog *EditTaskDialog)
    {
        EditTaskDialog->setWindowTitle(QCoreApplication::translate("EditTaskDialog", "EditTaskDialog", nullptr));
        label->setText(QCoreApplication::translate("EditTaskDialog", "\344\273\273\345\212\241\347\261\273\345\236\213", nullptr));
    } // retranslateUi

};

namespace Ui {
    class EditTaskDialog: public Ui_EditTaskDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_EDITTASKDIALOG_H
