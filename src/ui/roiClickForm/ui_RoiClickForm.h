/********************************************************************************
** Form generated from reading UI file 'RoiClickForm.ui'
**
** Created by: Qt User Interface Compiler version 6.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ROICLICKFORM_H
#define UI_ROICLICKFORM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>
#include "src/ui/ocrForm/RoiPreviewWidget.h"

QT_BEGIN_NAMESPACE

class Ui_RoiClickForm
{
public:
    QWidget *formLayoutWidget;
    QFormLayout *formLayout;
    QLabel *label;
    QLineEdit *lineTaskNameEdit;
    QLabel *randomClickLabel;
    QCheckBox *randomClickCheckBox;
    QLabel *roiPosLabel;
    QHBoxLayout *roiPosLayout;
    QDoubleSpinBox *roiXBox;
    QDoubleSpinBox *roiYBox;
    QLabel *roiSizeLabel;
    QHBoxLayout *roiSizeLayout;
    QDoubleSpinBox *roiWBox;
    QDoubleSpinBox *roiHBox;
    QLabel *errorHandleLabel;
    QComboBox *errorHandleBox;
    QLabel *stepInputLabel;
    QComboBox *stepInputBox;
    QLabel *previewLabel;
    QHBoxLayout *previewBtnLayout;
    QToolButton *btnUploadImage;
    QToolButton *btnCaptureImage;
    RoiPreviewWidget *roiPreview;
    QSpacerItem *bottomSpacer;

    void setupUi(QWidget *RoiClickForm)
    {
        if (RoiClickForm->objectName().isEmpty())
            RoiClickForm->setObjectName("RoiClickForm");
        RoiClickForm->resize(291, 620);
        formLayoutWidget = new QWidget(RoiClickForm);
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

        randomClickLabel = new QLabel(formLayoutWidget);
        randomClickLabel->setObjectName("randomClickLabel");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, randomClickLabel);

        randomClickCheckBox = new QCheckBox(formLayoutWidget);
        randomClickCheckBox->setObjectName("randomClickCheckBox");
        randomClickCheckBox->setChecked(true);

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, randomClickCheckBox);

        roiPosLabel = new QLabel(formLayoutWidget);
        roiPosLabel->setObjectName("roiPosLabel");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, roiPosLabel);

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


        formLayout->setLayout(2, QFormLayout::ItemRole::FieldRole, roiPosLayout);

        roiSizeLabel = new QLabel(formLayoutWidget);
        roiSizeLabel->setObjectName("roiSizeLabel");

        formLayout->setWidget(3, QFormLayout::ItemRole::LabelRole, roiSizeLabel);

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


        formLayout->setLayout(3, QFormLayout::ItemRole::FieldRole, roiSizeLayout);

        errorHandleLabel = new QLabel(formLayoutWidget);
        errorHandleLabel->setObjectName("errorHandleLabel");

        formLayout->setWidget(4, QFormLayout::ItemRole::LabelRole, errorHandleLabel);

        errorHandleBox = new QComboBox(formLayoutWidget);
        errorHandleBox->setObjectName("errorHandleBox");

        formLayout->setWidget(4, QFormLayout::ItemRole::FieldRole, errorHandleBox);

        stepInputLabel = new QLabel(formLayoutWidget);
        stepInputLabel->setObjectName("stepInputLabel");

        formLayout->setWidget(5, QFormLayout::ItemRole::LabelRole, stepInputLabel);

        stepInputBox = new QComboBox(formLayoutWidget);
        stepInputBox->setObjectName("stepInputBox");

        formLayout->setWidget(5, QFormLayout::ItemRole::FieldRole, stepInputBox);

        previewLabel = new QLabel(formLayoutWidget);
        previewLabel->setObjectName("previewLabel");

        formLayout->setWidget(6, QFormLayout::ItemRole::LabelRole, previewLabel);

        previewBtnLayout = new QHBoxLayout();
        previewBtnLayout->setSpacing(8);
        previewBtnLayout->setObjectName("previewBtnLayout");
        btnUploadImage = new QToolButton(formLayoutWidget);
        btnUploadImage->setObjectName("btnUploadImage");

        previewBtnLayout->addWidget(btnUploadImage);

        btnCaptureImage = new QToolButton(formLayoutWidget);
        btnCaptureImage->setObjectName("btnCaptureImage");

        previewBtnLayout->addWidget(btnCaptureImage);


        formLayout->setLayout(6, QFormLayout::ItemRole::FieldRole, previewBtnLayout);

        roiPreview = new RoiPreviewWidget(formLayoutWidget);
        roiPreview->setObjectName("roiPreview");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(roiPreview->sizePolicy().hasHeightForWidth());
        roiPreview->setSizePolicy(sizePolicy);
        roiPreview->setMinimumSize(QSize(260, 150));
        roiPreview->setMaximumSize(QSize(16777215, 200));

        formLayout->setWidget(7, QFormLayout::ItemRole::SpanningRole, roiPreview);

        bottomSpacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        formLayout->setItem(8, QFormLayout::ItemRole::SpanningRole, bottomSpacer);


        retranslateUi(RoiClickForm);

        QMetaObject::connectSlotsByName(RoiClickForm);
    } // setupUi

    void retranslateUi(QWidget *RoiClickForm)
    {
        RoiClickForm->setWindowTitle(QCoreApplication::translate("RoiClickForm", "RoiClickForm", nullptr));
        label->setText(QCoreApplication::translate("RoiClickForm", "\344\273\273\345\212\241\345\220\215\347\247\260", nullptr));
        randomClickLabel->setText(QCoreApplication::translate("RoiClickForm", "\346\230\257\345\220\246\351\232\217\346\234\272\347\202\271\345\207\273", nullptr));
#if QT_CONFIG(tooltip)
        randomClickCheckBox->setToolTip(QCoreApplication::translate("RoiClickForm", "\345\274\200\345\220\257\345\220\216\345\234\250\345\214\272\345\237\237\345\206\205\351\232\217\346\234\272\345\217\226\347\202\271\347\202\271\345\207\273\357\274\214\351\201\277\345\205\215\345\233\272\345\256\232\345\235\220\346\240\207\357\274\233\345\205\263\351\227\255\345\210\231\347\202\271\345\207\273\345\214\272\345\237\237\344\270\255\345\277\203", nullptr));
#endif // QT_CONFIG(tooltip)
        randomClickCheckBox->setText(QString());
#if QT_CONFIG(tooltip)
        roiPosLabel->setToolTip(QCoreApplication::translate("RoiClickForm", "\347\202\271\345\207\273\345\214\272\345\237\237\345\267\246\344\270\212\350\247\222\345\235\220\346\240\207\357\274\214\345\215\240\345\233\276\347\211\207\345\256\275/\351\253\230\347\232\204\347\231\276\345\210\206\346\257\224", nullptr));
#endif // QT_CONFIG(tooltip)
        roiPosLabel->setText(QCoreApplication::translate("RoiClickForm", "\345\214\272\345\237\237\345\267\246/\344\270\212(%)", nullptr));
        roiXBox->setSuffix(QCoreApplication::translate("RoiClickForm", "%", nullptr));
        roiYBox->setSuffix(QCoreApplication::translate("RoiClickForm", "%", nullptr));
#if QT_CONFIG(tooltip)
        roiSizeLabel->setToolTip(QCoreApplication::translate("RoiClickForm", "\347\202\271\345\207\273\345\214\272\345\237\237\345\256\275\351\253\230\357\274\214\345\215\240\345\233\276\347\211\207\345\256\275/\351\253\230\347\232\204\347\231\276\345\210\206\346\257\224\357\274\233100%x100% \350\241\250\347\244\272\350\246\206\347\233\226\346\225\264\345\274\240\345\233\276\347\211\207", nullptr));
#endif // QT_CONFIG(tooltip)
        roiSizeLabel->setText(QCoreApplication::translate("RoiClickForm", "\345\214\272\345\237\237\345\256\275/\351\253\230(%)", nullptr));
        roiWBox->setSuffix(QCoreApplication::translate("RoiClickForm", "%", nullptr));
        roiHBox->setSuffix(QCoreApplication::translate("RoiClickForm", "%", nullptr));
#if QT_CONFIG(tooltip)
        errorHandleLabel->setToolTip(QCoreApplication::translate("RoiClickForm", "\347\202\271\345\207\273\345\244\261\350\264\245\357\274\210\346\210\252\345\233\276\345\244\261\350\264\245\346\210\226\345\214\272\345\237\237\346\227\240\346\225\210\357\274\211\346\227\266\347\232\204\345\244\204\347\220\206\346\226\271\345\274\217", nullptr));
#endif // QT_CONFIG(tooltip)
        errorHandleLabel->setText(QCoreApplication::translate("RoiClickForm", "\345\244\261\350\264\245\345\244\204\347\220\206", nullptr));
        stepInputLabel->setText(QCoreApplication::translate("RoiClickForm", "\350\267\263\350\275\254\346\255\245\351\252\244", nullptr));
        previewLabel->setText(QCoreApplication::translate("RoiClickForm", "\351\242\204\350\247\210", nullptr));
        btnUploadImage->setText(QCoreApplication::translate("RoiClickForm", "\344\270\212\344\274\240\345\233\276\347\211\207\346\237\245\347\234\213\345\214\272\345\237\237", nullptr));
#if QT_CONFIG(tooltip)
        btnCaptureImage->setToolTip(QCoreApplication::translate("RoiClickForm", "\346\210\252\345\217\226\345\275\223\345\211\215\346\270\270\346\210\217\347\224\273\351\235\242\344\275\234\344\270\272\351\242\204\350\247\210\345\272\225\345\233\276", nullptr));
#endif // QT_CONFIG(tooltip)
        btnCaptureImage->setText(QCoreApplication::translate("RoiClickForm", "\346\210\252\345\233\276\350\216\267\345\217\226", nullptr));
    } // retranslateUi

};

namespace Ui {
    class RoiClickForm: public Ui_RoiClickForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ROICLICKFORM_H
