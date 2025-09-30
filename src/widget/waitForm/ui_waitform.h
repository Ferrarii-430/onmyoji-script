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
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_WaitForm
{
public:
    QLabel *label;

    void setupUi(QWidget *WaitForm)
    {
        if (WaitForm->objectName().isEmpty())
            WaitForm->setObjectName("WaitForm");
        WaitForm->resize(400, 300);
        label = new QLabel(WaitForm);
        label->setObjectName("label");
        label->setGeometry(QRect(140, 110, 131, 21));

        retranslateUi(WaitForm);

        QMetaObject::connectSlotsByName(WaitForm);
    } // setupUi

    void retranslateUi(QWidget *WaitForm)
    {
        WaitForm->setWindowTitle(QCoreApplication::translate("WaitForm", "waitForm", nullptr));
        label->setText(QCoreApplication::translate("WaitForm", "\346\210\221\346\230\257\347\255\211\345\276\205\347\261\273\345\236\213", nullptr));
    } // retranslateUi

};

namespace Ui {
    class WaitForm: public Ui_WaitForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WAITFORM_H
