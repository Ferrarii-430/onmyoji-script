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
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
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
            OcrForm->setObjectName("OcrForm");
        OcrForm->resize(291, 560);
        formLayoutWidget = new QWidget(OcrForm);
        formLayoutWidget->setObjectName("formLayoutWidget");
        formLayoutWidget->setGeometry(QRect(0, 0, 291, 554));
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

        roiPosLabel = new QLabel(formLayoutWidget);
        roiPosLabel->setObjectName("roiPosLabel");

        formLayout->setWidget(4, QFormLayout::ItemRole::LabelRole, roiPosLabel);

        roiPosLayout = new QHBoxLayout();
        roiPosLayout->setObjectName("roiPosLayout");
        roiXBox = new QDoubleSpinBox(formLayoutWidget);
        roiXBox->setObjectName("roiXBox");
        roiXBox->setMaximum(100.000000000000000);
        roiXBox->setDecimals(1);

        roiPosLayout->addWidget(roiXBox);

        roiYBox = new QDoubleSpinBox(formLayoutWidget);
        roiYBox->setObjectName("roiYBox");
        roiYBox->setMaximum(100.000000000000000);
        roiYBox->setDecimals(1);

        roiPosLayout->addWidget(roiYBox);


        formLayout->setLayout(4, QFormLayout::ItemRole::FieldRole, roiPosLayout);

        roiSizeLabel = new QLabel(formLayoutWidget);
        roiSizeLabel->setObjectName("roiSizeLabel");

        formLayout->setWidget(5, QFormLayout::ItemRole::LabelRole, roiSizeLabel);

        roiSizeLayout = new QHBoxLayout();
        roiSizeLayout->setObjectName("roiSizeLayout");
        roiWBox = new QDoubleSpinBox(formLayoutWidget);
        roiWBox->setObjectName("roiWBox");
        roiWBox->setMaximum(100.000000000000000);
        roiWBox->setDecimals(1);
        roiWBox->setValue(100.000000000000000);

        roiSizeLayout->addWidget(roiWBox);

        roiHBox = new QDoubleSpinBox(formLayoutWidget);
        roiHBox->setObjectName("roiHBox");
        roiHBox->setMaximum(100.000000000000000);
        roiHBox->setDecimals(1);
        roiHBox->setValue(100.000000000000000);

        roiSizeLayout->addWidget(roiHBox);


        formLayout->setLayout(5, QFormLayout::ItemRole::FieldRole, roiSizeLayout);

        label_4 = new QLabel(formLayoutWidget);
        label_4->setObjectName("label_4");

        formLayout->setWidget(6, QFormLayout::ItemRole::LabelRole, label_4);

        opencvErrorHandle = new QComboBox(formLayoutWidget);
        opencvErrorHandle->setObjectName("opencvErrorHandle");

        formLayout->setWidget(6, QFormLayout::ItemRole::FieldRole, opencvErrorHandle);

        stepInputLabel = new QLabel(formLayoutWidget);
        stepInputLabel->setObjectName("stepInputLabel");

        formLayout->setWidget(7, QFormLayout::ItemRole::LabelRole, stepInputLabel);

        stepInputBox = new QComboBox(formLayoutWidget);
        stepInputBox->setObjectName("stepInputBox");

        formLayout->setWidget(7, QFormLayout::ItemRole::FieldRole, stepInputBox);

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

        label_5 = new QLabel(formLayoutWidget);
        label_5->setObjectName("label_5");

        formLayout->setWidget(8, QFormLayout::ItemRole::LabelRole, label_5);

        btnUploadImage = new QToolButton(formLayoutWidget);
        btnUploadImage->setObjectName("btnUploadImage");

        formLayout->setWidget(8, QFormLayout::ItemRole::FieldRole, btnUploadImage);

        roiPreview = new RoiPreviewWidget(formLayoutWidget);
        roiPreview->setObjectName("roiPreview");
        roiPreview->setMinimumSize(QSize(260, 150));
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(roiPreview->sizePolicy().hasHeightForWidth());
        roiPreview->setSizePolicy(sizePolicy);

        formLayout->setWidget(9, QFormLayout::ItemRole::SpanningRole, roiPreview);


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
        stepInputLabel->setText(QCoreApplication::translate("OcrForm", "\350\267\263\350\275\254\346\255\245\351\252\244", nullptr));
        label_6->setText(QCoreApplication::translate("OcrForm", "\346\230\257\345\220\246\351\232\217\346\234\272\347\202\271\345\207\273", nullptr));
        randomClickCheckBox->setText(QString());
        label_5->setText(QCoreApplication::translate("OcrForm", "\351\242\204\350\247\210", nullptr));
        btnUploadImage->setText(QCoreApplication::translate("OcrForm", "\344\270\212\344\274\240\345\233\276\347\211\207\346\237\245\347\234\213\345\214\272\345\237\237", nullptr));
    } // retranslateUi

};

namespace Ui {
    class OcrForm: public Ui_OcrForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OCRFORM_H
