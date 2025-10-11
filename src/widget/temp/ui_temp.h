/********************************************************************************
** Form generated from reading UI file 'temp.ui'
**
** Created by: Qt User Interface Compiler version 6.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TEMP_H
#define UI_TEMP_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_temp
{
public:

    void setupUi(QWidget *temp)
    {
        if (temp->objectName().isEmpty())
            temp->setObjectName("temp");
        temp->resize(400, 300);

        retranslateUi(temp);

        QMetaObject::connectSlotsByName(temp);
    } // setupUi

    void retranslateUi(QWidget *temp)
    {
        temp->setWindowTitle(QCoreApplication::translate("temp", "temp", nullptr));
    } // retranslateUi

};

namespace Ui {
    class temp: public Ui_temp {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TEMP_H
