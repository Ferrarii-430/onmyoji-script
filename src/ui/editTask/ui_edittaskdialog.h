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
    QLabel *label;
    QComboBox *comboBox;
    QStackedWidget *stackedWidget;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *EditTaskDialog)
    {
        if (EditTaskDialog->objectName().isEmpty())
            EditTaskDialog->setObjectName("EditTaskDialog");
        EditTaskDialog->resize(479, 496);
        label = new QLabel(EditTaskDialog);
        label->setObjectName("label");
        label->setGeometry(QRect(80, 30, 48, 16));
        comboBox = new QComboBox(EditTaskDialog);
        comboBox->setObjectName("comboBox");
        comboBox->setGeometry(QRect(165, 30, 211, 23));
        stackedWidget = new QStackedWidget(EditTaskDialog);
        stackedWidget->setObjectName("stackedWidget");
        stackedWidget->setGeometry(QRect(80, 80, 291, 350));
        buttonBox = new QDialogButtonBox(EditTaskDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setGeometry(QRect(130, 450, 221, 23));
        QSizePolicy sizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(buttonBox->sizePolicy().hasHeightForWidth());
        buttonBox->setSizePolicy(sizePolicy);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        retranslateUi(EditTaskDialog);

        QMetaObject::connectSlotsByName(EditTaskDialog);
    } // setupUi

    void retranslateUi(QDialog *EditTaskDialog)
    {
        EditTaskDialog->setWindowTitle(QCoreApplication::translate("EditTaskDialog", "\346\226\271\346\241\210\345\206\205\345\256\271\350\256\276\347\275\256", nullptr));
        label->setText(QCoreApplication::translate("EditTaskDialog", "\344\273\273\345\212\241\347\261\273\345\236\213", nullptr));
    } // retranslateUi

};

namespace Ui {
    class EditTaskDialog: public Ui_EditTaskDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_EDITTASKDIALOG_H
