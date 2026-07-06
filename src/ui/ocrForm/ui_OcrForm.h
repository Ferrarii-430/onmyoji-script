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
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_OcrForm
{
public:
    QWidget *formLayoutWidget;
    QFormLayout *formLayout;
    QLabel *label;
    QLineEdit *lineTaskNameEdit;
    QLabel *label_3;
    QLineEdit *ocrTextEdit;
    QLabel *label_2;
    QLabel *label_4;
    QComboBox *opencvErrorHandle;
    QLabel *stepInputLabel;
    QComboBox *stepInputBox;
    QLabel *label_6;
    QCheckBox *randomClickCheckBox;
    QDoubleSpinBox *spinScoreBox;

    void setupUi(QWidget *OcrForm)
    {
        if (OcrForm->objectName().isEmpty())
            OcrForm->setObjectName("OcrForm");
        OcrForm->resize(291, 307);
        formLayoutWidget = new QWidget(OcrForm);
        formLayoutWidget->setObjectName("formLayoutWidget");
        formLayoutWidget->setGeometry(QRect(0, 0, 291, 301));
        formLayout = new QFormLayout(formLayoutWidget);
        formLayout->setObjectName("formLayout");
        formLayout->setHorizontalSpacing(6);
        formLayout->setVerticalSpacing(20);
        formLayout->setContentsMargins(0, 0, 0, 0);
        label = new QLabel(formLayoutWidget);
        label->setObjectName("label");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, label);

        lineTaskNameEdit = new QLineEdit(formLayoutWidget);
        lineTaskNameEdit->setObjectName("lineTaskNameEdit");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, lineTaskNameEdit);

        label_3 = new QLabel(formLayoutWidget);
        label_3->setObjectName("label_3");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, label_3);

        ocrTextEdit = new QLineEdit(formLayoutWidget);
        ocrTextEdit->setObjectName("ocrTextEdit");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, ocrTextEdit);

        label_2 = new QLabel(formLayoutWidget);
        label_2->setObjectName("label_2");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, label_2);

        label_4 = new QLabel(formLayoutWidget);
        label_4->setObjectName("label_4");

        formLayout->setWidget(4, QFormLayout::ItemRole::LabelRole, label_4);

        opencvErrorHandle = new QComboBox(formLayoutWidget);
        opencvErrorHandle->setObjectName("opencvErrorHandle");

        formLayout->setWidget(4, QFormLayout::ItemRole::FieldRole, opencvErrorHandle);

        stepInputLabel = new QLabel(formLayoutWidget);
        stepInputLabel->setObjectName("stepInputLabel");

        formLayout->setWidget(5, QFormLayout::ItemRole::LabelRole, stepInputLabel);

        stepInputBox = new QComboBox(formLayoutWidget);
        stepInputBox->setObjectName("stepInputBox");

        formLayout->setWidget(5, QFormLayout::ItemRole::FieldRole, stepInputBox);

        label_6 = new QLabel(formLayoutWidget);
        label_6->setObjectName("label_6");

        formLayout->setWidget(3, QFormLayout::ItemRole::LabelRole, label_6);

        randomClickCheckBox = new QCheckBox(formLayoutWidget);
        randomClickCheckBox->setObjectName("randomClickCheckBox");

        formLayout->setWidget(3, QFormLayout::ItemRole::FieldRole, randomClickCheckBox);

        spinScoreBox = new QDoubleSpinBox(formLayoutWidget);
        spinScoreBox->setObjectName("spinScoreBox");
        spinScoreBox->setMaximum(1.000000000000000);
        spinScoreBox->setSingleStep(0.010000000000000);

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, spinScoreBox);


        retranslateUi(OcrForm);

        QMetaObject::connectSlotsByName(OcrForm);
    } // setupUi

    void retranslateUi(QWidget *OcrForm)
    {
        OcrForm->setWindowTitle(QCoreApplication::translate("OcrForm", "OcrForm", nullptr));
        label->setText(QCoreApplication::translate("OcrForm", "\344\273\273\345\212\241\345\220\215\347\247\260", nullptr));
        label_3->setText(QCoreApplication::translate("OcrForm", "\350\257\206\345\210\253\346\226\207\345\255\227", nullptr));
        label_2->setText(QCoreApplication::translate("OcrForm", "\345\210\206\346\225\260\351\230\210\345\200\274", nullptr));
        label_4->setText(QCoreApplication::translate("OcrForm", "\350\257\206\345\210\253\345\244\261\350\264\245\345\244\204\347\220\206", nullptr));
        stepInputLabel->setText(QCoreApplication::translate("OcrForm", "\350\267\263\350\275\254\346\255\245\351\252\244", nullptr));
        label_6->setText(QCoreApplication::translate("OcrForm", "\346\230\257\345\220\246\351\232\217\346\234\272\347\202\271\345\207\273", nullptr));
        randomClickCheckBox->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class OcrForm: public Ui_OcrForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OCRFORM_H
