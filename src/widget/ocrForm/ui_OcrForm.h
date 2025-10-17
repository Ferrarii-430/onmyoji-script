/********************************************************************************
** Form generated from reading UI file 'OcrForm.ui'
**
** Created by: Qt User Interface Compiler version 6.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OCRFORM_H
#define UI_OCRFORM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_OcrForm
{
public:

    void setupUi(QWidget *OcrForm)
    {
        if (OcrForm->objectName().isEmpty())
            OcrForm->setObjectName("OcrForm");
        OcrForm->resize(400, 300);

        retranslateUi(OcrForm);

        QMetaObject::connectSlotsByName(OcrForm);
    } // setupUi

    void retranslateUi(QWidget *OcrForm)
    {
        OcrForm->setWindowTitle(QCoreApplication::translate("OcrForm", "OcrForm", nullptr));
    } // retranslateUi

};

namespace Ui {
    class OcrForm: public Ui_OcrForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OCRFORM_H
