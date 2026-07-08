/********************************************************************************
** Form generated from reading UI file 'OcrForm.ui'
**
** Created by: Qt User Interface Compiler version 6.2.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OCRFORM_H
#define UI_OCRFORM_H

#include <QtCore/QVariant>
#include <QtCore/QCoreApplication>
#include <QtCore/QRect>
#include <QtCore/QSize>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

#include "src/ui/ocrForm/RoiPreviewWidget.h"

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
    QLabel *roiPosLabel;
    QHBoxLayout *roiPosLayout;
    QDoubleSpinBox *roiXBox;
    QDoubleSpinBox *roiYBox;
    QLabel *roiSizeLabel;
    QHBoxLayout *roiSizeLayout;
    QDoubleSpinBox *roiWBox;
    QDoubleSpinBox *roiHBox;
    QLabel *label_4;
    QComboBox *opencvErrorHandle;
    QLabel *stepInputLabel;
    QComboBox *stepInputBox;
    QLabel *label_6;
    QCheckBox *randomClickCheckBox;
    QDoubleSpinBox *spinScoreBox;
    QLabel *label_5;
    QToolButton *btnUploadImage;
    RoiPreviewWidget *roiPreview;

    void setupUi(QWidget *OcrForm)
    {
        if (OcrForm->objectName().isEmpty())
            OcrForm->setObjectName(QString::fromUtf8("OcrForm"));
        OcrForm->resize(291, 560);
        formLayoutWidget = new QWidget(OcrForm);
        formLayoutWidget->setObjectName(QString::fromUtf8("formLayoutWidget"));
        formLayoutWidget->setGeometry(QRect(0, 0, 291, 554));
        formLayout = new QFormLayout(formLayoutWidget);
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        formLayout->setHorizontalSpacing(6);
        formLayout->setVerticalSpacing(20);
        formLayout->setContentsMargins(0, 0, 0, 0);
        label = new QLabel(formLayoutWidget);
        label->setObjectName(QString::fromUtf8("label"));

        formLayout->setWidget(0, QFormLayout::LabelRole, label);

        lineTaskNameEdit = new QLineEdit(formLayoutWidget);
        lineTaskNameEdit->setObjectName(QString::fromUtf8("lineTaskNameEdit"));

        formLayout->setWidget(0, QFormLayout::FieldRole, lineTaskNameEdit);

        label_3 = new QLabel(formLayoutWidget);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        formLayout->setWidget(1, QFormLayout::LabelRole, label_3);

        ocrTextEdit = new QLineEdit(formLayoutWidget);
        ocrTextEdit->setObjectName(QString::fromUtf8("ocrTextEdit"));

        formLayout->setWidget(1, QFormLayout::FieldRole, ocrTextEdit);

        label_2 = new QLabel(formLayoutWidget);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        formLayout->setWidget(2, QFormLayout::LabelRole, label_2);

        roiPosLabel = new QLabel(formLayoutWidget);
        roiPosLabel->setObjectName(QString::fromUtf8("roiPosLabel"));

        formLayout->setWidget(4, QFormLayout::LabelRole, roiPosLabel);

        roiPosLayout = new QHBoxLayout();
        roiPosLayout->setObjectName(QString::fromUtf8("roiPosLayout"));
        roiXBox = new QDoubleSpinBox(formLayoutWidget);
        roiXBox->setObjectName(QString::fromUtf8("roiXBox"));
        roiXBox->setMaximum(100.000000000000000);
        roiXBox->setDecimals(1);

        roiPosLayout->addWidget(roiXBox);

        roiYBox = new QDoubleSpinBox(formLayoutWidget);
        roiYBox->setObjectName(QString::fromUtf8("roiYBox"));
        roiYBox->setMaximum(100.000000000000000);
        roiYBox->setDecimals(1);

        roiPosLayout->addWidget(roiYBox);


        formLayout->setLayout(4, QFormLayout::FieldRole, roiPosLayout);

        roiSizeLabel = new QLabel(formLayoutWidget);
        roiSizeLabel->setObjectName(QString::fromUtf8("roiSizeLabel"));

        formLayout->setWidget(5, QFormLayout::LabelRole, roiSizeLabel);

        roiSizeLayout = new QHBoxLayout();
        roiSizeLayout->setObjectName(QString::fromUtf8("roiSizeLayout"));
        roiWBox = new QDoubleSpinBox(formLayoutWidget);
        roiWBox->setObjectName(QString::fromUtf8("roiWBox"));
        roiWBox->setMaximum(100.000000000000000);
        roiWBox->setDecimals(1);
        roiWBox->setValue(100.000000000000000);

        roiSizeLayout->addWidget(roiWBox);

        roiHBox = new QDoubleSpinBox(formLayoutWidget);
        roiHBox->setObjectName(QString::fromUtf8("roiHBox"));
        roiHBox->setMaximum(100.000000000000000);
        roiHBox->setDecimals(1);
        roiHBox->setValue(100.000000000000000);

        roiSizeLayout->addWidget(roiHBox);


        formLayout->setLayout(5, QFormLayout::FieldRole, roiSizeLayout);

        label_4 = new QLabel(formLayoutWidget);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        formLayout->setWidget(6, QFormLayout::LabelRole, label_4);

        opencvErrorHandle = new QComboBox(formLayoutWidget);
        opencvErrorHandle->setObjectName(QString::fromUtf8("opencvErrorHandle"));

        formLayout->setWidget(6, QFormLayout::FieldRole, opencvErrorHandle);

        stepInputLabel = new QLabel(formLayoutWidget);
        stepInputLabel->setObjectName(QString::fromUtf8("stepInputLabel"));

        formLayout->setWidget(7, QFormLayout::LabelRole, stepInputLabel);

        stepInputBox = new QComboBox(formLayoutWidget);
        stepInputBox->setObjectName(QString::fromUtf8("stepInputBox"));

        formLayout->setWidget(7, QFormLayout::FieldRole, stepInputBox);

        label_6 = new QLabel(formLayoutWidget);
        label_6->setObjectName(QString::fromUtf8("label_6"));

        formLayout->setWidget(3, QFormLayout::LabelRole, label_6);

        randomClickCheckBox = new QCheckBox(formLayoutWidget);
        randomClickCheckBox->setObjectName(QString::fromUtf8("randomClickCheckBox"));

        formLayout->setWidget(3, QFormLayout::FieldRole, randomClickCheckBox);

        spinScoreBox = new QDoubleSpinBox(formLayoutWidget);
        spinScoreBox->setObjectName(QString::fromUtf8("spinScoreBox"));
        spinScoreBox->setMaximum(1.000000000000000);
        spinScoreBox->setSingleStep(0.010000000000000);

        formLayout->setWidget(2, QFormLayout::FieldRole, spinScoreBox);

        label_5 = new QLabel(formLayoutWidget);
        label_5->setObjectName(QString::fromUtf8("label_5"));

        formLayout->setWidget(8, QFormLayout::LabelRole, label_5);

        btnUploadImage = new QToolButton(formLayoutWidget);
        btnUploadImage->setObjectName(QString::fromUtf8("btnUploadImage"));

        formLayout->setWidget(8, QFormLayout::FieldRole, btnUploadImage);

        roiPreview = new RoiPreviewWidget(formLayoutWidget);
        roiPreview->setObjectName(QString::fromUtf8("roiPreview"));
        roiPreview->setMinimumSize(QSize(260, 150));
        roiPreview->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

        formLayout->setWidget(9, QFormLayout::SpanningRole, roiPreview);


        retranslateUi(OcrForm);

        QMetaObject::connectSlotsByName(OcrForm);
    } // setupUi

    void retranslateUi(QWidget *OcrForm)
    {
        OcrForm->setWindowTitle(QCoreApplication::translate("OcrForm", "OcrForm", nullptr));
        label->setText(QCoreApplication::translate("OcrForm", "\344\273\273\345\212\241\345\220\215\347\247\260", nullptr));
        label_3->setText(QCoreApplication::translate("OcrForm", "\350\257\206\345\210\253\346\226\207\345\255\227", nullptr));
        label_2->setText(QCoreApplication::translate("OcrForm", "\345\210\206\346\225\260\351\230\210\345\200\274", nullptr));
        roiPosLabel->setText(QCoreApplication::translate("OcrForm", "\345\214\272\345\237\237\345\267\246/\344\270\212(%)", nullptr));
#if QT_CONFIG(tooltip)
        roiPosLabel->setToolTip(QCoreApplication::translate("OcrForm", "\350\257\206\345\210\253\345\214\272\345\237\237\345\267\246\344\270\212\350\247\222\345\235\220\346\240\207\357\274\214\345\215\240\345\233\276\347\211\207\345\256\275/\351\253\230\347\232\204\347\231\276\345\210\206\346\257\224", nullptr));
#endif // QT_CONFIG(tooltip)
        roiXBox->setSuffix(QCoreApplication::translate("OcrForm", "%", nullptr));
        roiYBox->setSuffix(QCoreApplication::translate("OcrForm", "%", nullptr));
        roiSizeLabel->setText(QCoreApplication::translate("OcrForm", "\345\214\272\345\237\237\345\256\275/\351\253\230(%)", nullptr));
#if QT_CONFIG(tooltip)
        roiSizeLabel->setToolTip(QCoreApplication::translate("OcrForm", "\350\257\206\345\210\253\345\214\272\345\237\237\345\256\275\351\253\230\357\274\214\345\215\240\345\233\276\347\211\207\345\256\275/\351\253\230\347\232\204\347\231\276\345\210\206\346\257\224\357\274\233100%x100% \350\241\250\347\244\272\350\257\206\345\210\253\346\225\264\345\274\240\345\233\276\347\211\207", nullptr));
#endif // QT_CONFIG(tooltip)
        roiWBox->setSuffix(QCoreApplication::translate("OcrForm", "%", nullptr));
        roiHBox->setSuffix(QCoreApplication::translate("OcrForm", "%", nullptr));
        label_4->setText(QCoreApplication::translate("OcrForm", "\350\257\206\345\210\253\345\244\261\350\264\245\345\244\204\347\220\206", nullptr));
        label_5->setText(QCoreApplication::translate("OcrForm", "\351\242\204\350\247\206", nullptr));
        btnUploadImage->setText(QCoreApplication::translate("OcrForm", "\344\270\212\344\274\240\345\233\276\347\211\207\346\237\245\347\234\213\345\214\272\345\237\237", nullptr));
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
