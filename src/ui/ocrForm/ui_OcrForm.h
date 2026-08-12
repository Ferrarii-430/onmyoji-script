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
#include <QtWidgets/QGridLayout>
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
    QLabel *enhanceLabel;
    QGridLayout *enhanceLayout;
    QCheckBox *enhanceUpscaleBox;
    QCheckBox *enhanceGrayscaleBox;
    QCheckBox *enhanceContrastBox;
    QCheckBox *enhanceSharpenBox;
    QCheckBox *enhanceInvertBox;
    QLabel *label_4;
    QComboBox *opencvErrorHandle;
    QLabel *stepInputLabel;
    QComboBox *stepInputBox;
    QLabel *label_6;
    QCheckBox *randomClickCheckBox;
    QDoubleSpinBox *spinScoreBox;
    QLabel *label_5;
    QHBoxLayout *previewBtnLayout;
    QToolButton *btnUploadImage;
    QToolButton *btnCaptureImage;
    RoiPreviewWidget *roiPreview;

    void setupUi(QWidget *OcrForm)
    {
        if (OcrForm->objectName().isEmpty())
            OcrForm->setObjectName("OcrForm");
        OcrForm->resize(291, 620);
        formLayoutWidget = new QWidget(OcrForm);
        formLayoutWidget->setObjectName("formLayoutWidget");
        formLayoutWidget->setGeometry(QRect(0, 0, 291, 614));
        formLayout = new QFormLayout(formLayoutWidget);
        formLayout->setObjectName("formLayout");
        formLayout->setHorizontalSpacing(6);
        formLayout->setVerticalSpacing(12);
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
        roiXBox->setDecimals(1);
        roiXBox->setMaximum(100.000000000000000);

        roiPosLayout->addWidget(roiXBox);

        roiYBox = new QDoubleSpinBox(formLayoutWidget);
        roiYBox->setObjectName("roiYBox");
        roiYBox->setDecimals(1);
        roiYBox->setMaximum(100.000000000000000);

        roiPosLayout->addWidget(roiYBox);


        formLayout->setLayout(4, QFormLayout::ItemRole::FieldRole, roiPosLayout);

        roiSizeLabel = new QLabel(formLayoutWidget);
        roiSizeLabel->setObjectName("roiSizeLabel");

        formLayout->setWidget(5, QFormLayout::ItemRole::LabelRole, roiSizeLabel);

        roiSizeLayout = new QHBoxLayout();
        roiSizeLayout->setObjectName("roiSizeLayout");
        roiWBox = new QDoubleSpinBox(formLayoutWidget);
        roiWBox->setObjectName("roiWBox");
        roiWBox->setDecimals(1);
        roiWBox->setMaximum(100.000000000000000);
        roiWBox->setValue(100.000000000000000);

        roiSizeLayout->addWidget(roiWBox);

        roiHBox = new QDoubleSpinBox(formLayoutWidget);
        roiHBox->setObjectName("roiHBox");
        roiHBox->setDecimals(1);
        roiHBox->setMaximum(100.000000000000000);
        roiHBox->setValue(100.000000000000000);

        roiSizeLayout->addWidget(roiHBox);


        formLayout->setLayout(5, QFormLayout::ItemRole::FieldRole, roiSizeLayout);

        enhanceLabel = new QLabel(formLayoutWidget);
        enhanceLabel->setObjectName("enhanceLabel");

        formLayout->setWidget(6, QFormLayout::ItemRole::LabelRole, enhanceLabel);

        enhanceLayout = new QGridLayout();
        enhanceLayout->setObjectName("enhanceLayout");
        enhanceLayout->setHorizontalSpacing(16);
        enhanceLayout->setVerticalSpacing(8);
        enhanceLayout->setContentsMargins(-1, 2, -1, 2);
        enhanceUpscaleBox = new QCheckBox(formLayoutWidget);
        enhanceUpscaleBox->setObjectName("enhanceUpscaleBox");
        enhanceUpscaleBox->setChecked(true);

        enhanceLayout->addWidget(enhanceUpscaleBox, 0, 0, 1, 1);

        enhanceGrayscaleBox = new QCheckBox(formLayoutWidget);
        enhanceGrayscaleBox->setObjectName("enhanceGrayscaleBox");

        enhanceLayout->addWidget(enhanceGrayscaleBox, 0, 1, 1, 1);

        enhanceContrastBox = new QCheckBox(formLayoutWidget);
        enhanceContrastBox->setObjectName("enhanceContrastBox");

        enhanceLayout->addWidget(enhanceContrastBox, 1, 0, 1, 1);

        enhanceSharpenBox = new QCheckBox(formLayoutWidget);
        enhanceSharpenBox->setObjectName("enhanceSharpenBox");

        enhanceLayout->addWidget(enhanceSharpenBox, 1, 1, 1, 1);

        enhanceInvertBox = new QCheckBox(formLayoutWidget);
        enhanceInvertBox->setObjectName("enhanceInvertBox");

        enhanceLayout->addWidget(enhanceInvertBox, 2, 0, 1, 2);


        formLayout->setLayout(6, QFormLayout::ItemRole::FieldRole, enhanceLayout);

        label_4 = new QLabel(formLayoutWidget);
        label_4->setObjectName("label_4");

        formLayout->setWidget(7, QFormLayout::ItemRole::LabelRole, label_4);

        opencvErrorHandle = new QComboBox(formLayoutWidget);
        opencvErrorHandle->setObjectName("opencvErrorHandle");

        formLayout->setWidget(7, QFormLayout::ItemRole::FieldRole, opencvErrorHandle);

        stepInputLabel = new QLabel(formLayoutWidget);
        stepInputLabel->setObjectName("stepInputLabel");

        formLayout->setWidget(8, QFormLayout::ItemRole::LabelRole, stepInputLabel);

        stepInputBox = new QComboBox(formLayoutWidget);
        stepInputBox->setObjectName("stepInputBox");

        formLayout->setWidget(8, QFormLayout::ItemRole::FieldRole, stepInputBox);

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

        formLayout->setWidget(9, QFormLayout::ItemRole::LabelRole, label_5);

        previewBtnLayout = new QHBoxLayout();
        previewBtnLayout->setSpacing(8);
        previewBtnLayout->setObjectName("previewBtnLayout");
        btnUploadImage = new QToolButton(formLayoutWidget);
        btnUploadImage->setObjectName("btnUploadImage");

        previewBtnLayout->addWidget(btnUploadImage);

        btnCaptureImage = new QToolButton(formLayoutWidget);
        btnCaptureImage->setObjectName("btnCaptureImage");

        previewBtnLayout->addWidget(btnCaptureImage);


        formLayout->setLayout(9, QFormLayout::ItemRole::FieldRole, previewBtnLayout);

        roiPreview = new RoiPreviewWidget(formLayoutWidget);
        roiPreview->setObjectName("roiPreview");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(roiPreview->sizePolicy().hasHeightForWidth());
        roiPreview->setSizePolicy(sizePolicy);
        roiPreview->setMinimumSize(QSize(260, 150));

        formLayout->setWidget(10, QFormLayout::ItemRole::SpanningRole, roiPreview);


        retranslateUi(OcrForm);

        QMetaObject::connectSlotsByName(OcrForm);
    } // setupUi

    void retranslateUi(QWidget *OcrForm)
    {
        OcrForm->setWindowTitle(QCoreApplication::translate("OcrForm", "OcrForm", nullptr));
        label->setText(QCoreApplication::translate("OcrForm", "\344\273\273\345\212\241\345\220\215\347\247\260", nullptr));
        label_3->setText(QCoreApplication::translate("OcrForm", "\350\257\206\345\210\253\346\226\207\345\255\227", nullptr));
        label_2->setText(QCoreApplication::translate("OcrForm", "\345\210\206\346\225\260\351\230\210\345\200\274", nullptr));
#if QT_CONFIG(tooltip)
        roiPosLabel->setToolTip(QCoreApplication::translate("OcrForm", "\350\257\206\345\210\253\345\214\272\345\237\237\345\267\246\344\270\212\350\247\222\345\235\220\346\240\207\357\274\214\345\215\240\345\233\276\347\211\207\345\256\275/\351\253\230\347\232\204\347\231\276\345\210\206\346\257\224", nullptr));
#endif // QT_CONFIG(tooltip)
        roiPosLabel->setText(QCoreApplication::translate("OcrForm", "\345\214\272\345\237\237\345\267\246/\344\270\212(%)", nullptr));
        roiXBox->setSuffix(QCoreApplication::translate("OcrForm", "%", nullptr));
        roiYBox->setSuffix(QCoreApplication::translate("OcrForm", "%", nullptr));
#if QT_CONFIG(tooltip)
        roiSizeLabel->setToolTip(QCoreApplication::translate("OcrForm", "\350\257\206\345\210\253\345\214\272\345\237\237\345\256\275\351\253\230\357\274\214\345\215\240\345\233\276\347\211\207\345\256\275/\351\253\230\347\232\204\347\231\276\345\210\206\346\257\224\357\274\233100%x100% \350\241\250\347\244\272\350\257\206\345\210\253\346\225\264\345\274\240\345\233\276\347\211\207", nullptr));
#endif // QT_CONFIG(tooltip)
        roiSizeLabel->setText(QCoreApplication::translate("OcrForm", "\345\214\272\345\237\237\345\256\275/\351\253\230(%)", nullptr));
        roiWBox->setSuffix(QCoreApplication::translate("OcrForm", "%", nullptr));
        roiHBox->setSuffix(QCoreApplication::translate("OcrForm", "%", nullptr));
#if QT_CONFIG(tooltip)
        enhanceLabel->setToolTip(QCoreApplication::translate("OcrForm", "\344\273\205\345\234\250\350\257\206\345\210\253\345\214\272\345\237\237\345\260\217\344\272\216\346\225\264\345\274\240\345\233\276\347\211\207\346\227\266\347\224\237\346\225\210\357\274\214\345\217\257\345\244\232\351\200\211\357\274\233\345\260\217\345\255\227\345\217\267/\346\232\227\345\272\225\346\226\207\345\255\227\345\273\272\350\256\256\345\205\250\351\203\250\345\274\200\345\220\257", nullptr));
#endif // QT_CONFIG(tooltip)
        enhanceLabel->setText(QCoreApplication::translate("OcrForm", "\350\257\206\345\210\253\345\242\236\345\274\272", nullptr));
#if QT_CONFIG(tooltip)
        enhanceUpscaleBox->setToolTip(QCoreApplication::translate("OcrForm", "\350\243\201\345\211\252\345\233\276\350\277\207\345\260\217\346\227\266\347\255\211\346\257\224\346\224\276\345\244\247\357\274\210\345\217\214\344\270\211\346\254\241\346\217\222\345\200\274\357\274\211\357\274\214\346\234\200\345\244\232 3 \345\200\215\357\274\233\350\257\206\345\210\253\346\225\264\345\274\240\345\233\276\347\211\207\346\227\266\344\270\215\347\224\237\346\225\210", nullptr));
#endif // QT_CONFIG(tooltip)
        enhanceUpscaleBox->setText(QCoreApplication::translate("OcrForm", "\346\224\276\345\244\247", nullptr));
#if QT_CONFIG(tooltip)
        enhanceGrayscaleBox->setToolTip(QCoreApplication::translate("OcrForm", "\350\275\254\344\270\272\347\201\260\345\272\246\345\233\276\357\274\214\345\216\273\346\216\211\345\275\251\350\211\262\350\203\214\346\231\257\345\271\262\346\211\260", nullptr));
#endif // QT_CONFIG(tooltip)
        enhanceGrayscaleBox->setText(QCoreApplication::translate("OcrForm", "\347\201\260\345\272\246", nullptr));
#if QT_CONFIG(tooltip)
        enhanceContrastBox->setToolTip(QCoreApplication::translate("OcrForm", "CLAHE \345\261\200\351\203\250\345\257\271\346\257\224\345\272\246\345\242\236\345\274\272\357\274\214\346\217\220\344\272\256\346\232\227\345\244\204\346\226\207\345\255\227\357\274\210\344\274\232\350\207\252\345\212\250\350\275\254\347\201\260\345\272\246\357\274\211", nullptr));
#endif // QT_CONFIG(tooltip)
        enhanceContrastBox->setText(QCoreApplication::translate("OcrForm", "\345\257\271\346\257\224\345\272\246", nullptr));
#if QT_CONFIG(tooltip)
        enhanceSharpenBox->setToolTip(QCoreApplication::translate("OcrForm", "USM \351\224\220\345\214\226\357\274\214\346\201\242\345\244\215\346\224\276\345\244\247\345\220\216\350\242\253\346\212\271\345\271\263\347\232\204\347\254\224\347\224\273\350\276\271\347\274\230", nullptr));
#endif // QT_CONFIG(tooltip)
        enhanceSharpenBox->setText(QCoreApplication::translate("OcrForm", "\351\224\220\345\214\226", nullptr));
#if QT_CONFIG(tooltip)
        enhanceInvertBox->setToolTip(QCoreApplication::translate("OcrForm", "\346\243\200\346\265\213\345\210\260\346\232\227\345\272\225\344\272\256\345\255\227\346\227\266\345\217\215\350\211\262\344\270\272\347\231\275\345\272\225\351\273\221\345\255\227\357\274\210\344\274\232\350\207\252\345\212\250\350\275\254\347\201\260\345\272\246\357\274\211", nullptr));
#endif // QT_CONFIG(tooltip)
        enhanceInvertBox->setText(QCoreApplication::translate("OcrForm", "\346\232\227\345\272\225\350\207\252\345\212\250\345\217\215\350\211\262", nullptr));
        label_4->setText(QCoreApplication::translate("OcrForm", "\350\257\206\345\210\253\345\244\261\350\264\245\345\244\204\347\220\206", nullptr));
        stepInputLabel->setText(QCoreApplication::translate("OcrForm", "\350\267\263\350\275\254\346\255\245\351\252\244", nullptr));
        label_6->setText(QCoreApplication::translate("OcrForm", "\346\230\257\345\220\246\351\232\217\346\234\272\347\202\271\345\207\273", nullptr));
        randomClickCheckBox->setText(QString());
        label_5->setText(QCoreApplication::translate("OcrForm", "\351\242\204\350\247\210", nullptr));
        btnUploadImage->setText(QCoreApplication::translate("OcrForm", "\344\270\212\344\274\240\345\233\276\347\211\207\346\237\245\347\234\213\345\214\272\345\237\237", nullptr));
#if QT_CONFIG(tooltip)
        btnCaptureImage->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        btnCaptureImage->setText(QCoreApplication::translate("OcrForm", "\346\210\252\345\233\276\350\216\267\345\217\226", nullptr));
    } // retranslateUi

};

namespace Ui {
    class OcrForm: public Ui_OcrForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OCRFORM_H
