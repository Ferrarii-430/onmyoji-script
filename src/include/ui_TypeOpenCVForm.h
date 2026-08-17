/********************************************************************************
** Form generated from reading UI file 'typeopencvform.ui'
**
** Created by: Qt User Interface Compiler version 6.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TYPEOPENCVFORM_H
#define UI_TYPEOPENCVFORM_H

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
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TypeOpenCVForm
{
public:
    QVBoxLayout *rootLayout;
    QWidget *formLayoutWidget;
    QFormLayout *formLayout;
    QLabel *label;
    QLineEdit *lineTaskNameEdit;
    QLabel *label_2;
    QDoubleSpinBox *spinScoreBox;
    QLabel *label_3;
    QCheckBox *randomClickCheckBox;
    QLabel *label_6;
    QCheckBox *colorCheckCheckBox;
    QLabel *excludeLabel;
    QHBoxLayout *excludeLrLayout;
    QLabel *excludeLeftLabel;
    QDoubleSpinBox *excludeLeftBox;
    QLabel *excludeRightLabel;
    QDoubleSpinBox *excludeRightBox;
    QLabel *excludeTbLabel;
    QHBoxLayout *excludeTbLayout;
    QLabel *excludeTopLabel;
    QDoubleSpinBox *excludeTopBox;
    QLabel *excludeBottomLabel;
    QDoubleSpinBox *excludeBottomBox;
    QLabel *label_5;
    QComboBox *opencvErrorHandle;
    QLabel *stepInputLabel;
    QComboBox *stepInputBox;
    QLabel *label_4;
    QToolButton *btnCapture;
    QLabel *labelPreview;

    void setupUi(QWidget *TypeOpenCVForm)
    {
        if (TypeOpenCVForm->objectName().isEmpty())
            TypeOpenCVForm->setObjectName("TypeOpenCVForm");
        TypeOpenCVForm->resize(291, 350);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(TypeOpenCVForm->sizePolicy().hasHeightForWidth());
        TypeOpenCVForm->setSizePolicy(sizePolicy);
        rootLayout = new QVBoxLayout(TypeOpenCVForm);
        rootLayout->setObjectName("rootLayout");
        rootLayout->setContentsMargins(0, 0, 0, 0);
        formLayoutWidget = new QWidget(TypeOpenCVForm);
        formLayoutWidget->setObjectName("formLayoutWidget");
        sizePolicy.setHeightForWidth(formLayoutWidget->sizePolicy().hasHeightForWidth());
        formLayoutWidget->setSizePolicy(sizePolicy);
        formLayout = new QFormLayout(formLayoutWidget);
        formLayout->setObjectName("formLayout");
        formLayout->setVerticalSpacing(12);
        formLayout->setContentsMargins(0, 0, 0, 0);
        label = new QLabel(formLayoutWidget);
        label->setObjectName("label");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, label);

        lineTaskNameEdit = new QLineEdit(formLayoutWidget);
        lineTaskNameEdit->setObjectName("lineTaskNameEdit");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, lineTaskNameEdit);

        label_2 = new QLabel(formLayoutWidget);
        label_2->setObjectName("label_2");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, label_2);

        spinScoreBox = new QDoubleSpinBox(formLayoutWidget);
        spinScoreBox->setObjectName("spinScoreBox");
        spinScoreBox->setMaximum(1.000000000000000);
        spinScoreBox->setSingleStep(0.010000000000000);

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, spinScoreBox);

        label_3 = new QLabel(formLayoutWidget);
        label_3->setObjectName("label_3");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, label_3);

        randomClickCheckBox = new QCheckBox(formLayoutWidget);
        randomClickCheckBox->setObjectName("randomClickCheckBox");

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, randomClickCheckBox);

        label_6 = new QLabel(formLayoutWidget);
        label_6->setObjectName("label_6");

        formLayout->setWidget(3, QFormLayout::ItemRole::LabelRole, label_6);

        colorCheckCheckBox = new QCheckBox(formLayoutWidget);
        colorCheckCheckBox->setObjectName("colorCheckCheckBox");

        formLayout->setWidget(3, QFormLayout::ItemRole::FieldRole, colorCheckCheckBox);

        excludeLabel = new QLabel(formLayoutWidget);
        excludeLabel->setObjectName("excludeLabel");

        formLayout->setWidget(4, QFormLayout::ItemRole::LabelRole, excludeLabel);

        excludeLrLayout = new QHBoxLayout();
        excludeLrLayout->setSpacing(4);
        excludeLrLayout->setObjectName("excludeLrLayout");
        excludeLeftLabel = new QLabel(formLayoutWidget);
        excludeLeftLabel->setObjectName("excludeLeftLabel");

        excludeLrLayout->addWidget(excludeLeftLabel);

        excludeLeftBox = new QDoubleSpinBox(formLayoutWidget);
        excludeLeftBox->setObjectName("excludeLeftBox");
        excludeLeftBox->setSuffix(QString::fromUtf8("%"));
        excludeLeftBox->setDecimals(0);
        excludeLeftBox->setMaximum(90.000000000000000);
        excludeLeftBox->setSingleStep(5.000000000000000);

        excludeLrLayout->addWidget(excludeLeftBox);

        excludeRightLabel = new QLabel(formLayoutWidget);
        excludeRightLabel->setObjectName("excludeRightLabel");

        excludeLrLayout->addWidget(excludeRightLabel);

        excludeRightBox = new QDoubleSpinBox(formLayoutWidget);
        excludeRightBox->setObjectName("excludeRightBox");
        excludeRightBox->setSuffix(QString::fromUtf8("%"));
        excludeRightBox->setDecimals(0);
        excludeRightBox->setMaximum(90.000000000000000);
        excludeRightBox->setSingleStep(5.000000000000000);

        excludeLrLayout->addWidget(excludeRightBox);


        formLayout->setLayout(4, QFormLayout::ItemRole::FieldRole, excludeLrLayout);

        excludeTbLabel = new QLabel(formLayoutWidget);
        excludeTbLabel->setObjectName("excludeTbLabel");

        formLayout->setWidget(5, QFormLayout::ItemRole::LabelRole, excludeTbLabel);

        excludeTbLayout = new QHBoxLayout();
        excludeTbLayout->setSpacing(4);
        excludeTbLayout->setObjectName("excludeTbLayout");
        excludeTopLabel = new QLabel(formLayoutWidget);
        excludeTopLabel->setObjectName("excludeTopLabel");

        excludeTbLayout->addWidget(excludeTopLabel);

        excludeTopBox = new QDoubleSpinBox(formLayoutWidget);
        excludeTopBox->setObjectName("excludeTopBox");
        excludeTopBox->setSuffix(QString::fromUtf8("%"));
        excludeTopBox->setDecimals(0);
        excludeTopBox->setMaximum(90.000000000000000);
        excludeTopBox->setSingleStep(5.000000000000000);

        excludeTbLayout->addWidget(excludeTopBox);

        excludeBottomLabel = new QLabel(formLayoutWidget);
        excludeBottomLabel->setObjectName("excludeBottomLabel");

        excludeTbLayout->addWidget(excludeBottomLabel);

        excludeBottomBox = new QDoubleSpinBox(formLayoutWidget);
        excludeBottomBox->setObjectName("excludeBottomBox");
        excludeBottomBox->setSuffix(QString::fromUtf8("%"));
        excludeBottomBox->setDecimals(0);
        excludeBottomBox->setMaximum(90.000000000000000);
        excludeBottomBox->setSingleStep(5.000000000000000);

        excludeTbLayout->addWidget(excludeBottomBox);


        formLayout->setLayout(5, QFormLayout::ItemRole::FieldRole, excludeTbLayout);

        label_5 = new QLabel(formLayoutWidget);
        label_5->setObjectName("label_5");

        formLayout->setWidget(6, QFormLayout::ItemRole::LabelRole, label_5);

        opencvErrorHandle = new QComboBox(formLayoutWidget);
        opencvErrorHandle->setObjectName("opencvErrorHandle");

        formLayout->setWidget(6, QFormLayout::ItemRole::FieldRole, opencvErrorHandle);

        stepInputLabel = new QLabel(formLayoutWidget);
        stepInputLabel->setObjectName("stepInputLabel");

        formLayout->setWidget(7, QFormLayout::ItemRole::LabelRole, stepInputLabel);

        stepInputBox = new QComboBox(formLayoutWidget);
        stepInputBox->setObjectName("stepInputBox");

        formLayout->setWidget(7, QFormLayout::ItemRole::FieldRole, stepInputBox);

        label_4 = new QLabel(formLayoutWidget);
        label_4->setObjectName("label_4");

        formLayout->setWidget(8, QFormLayout::ItemRole::LabelRole, label_4);

        btnCapture = new QToolButton(formLayoutWidget);
        btnCapture->setObjectName("btnCapture");

        formLayout->setWidget(8, QFormLayout::ItemRole::FieldRole, btnCapture);

        labelPreview = new QLabel(formLayoutWidget);
        labelPreview->setObjectName("labelPreview");
        sizePolicy.setHeightForWidth(labelPreview->sizePolicy().hasHeightForWidth());
        labelPreview->setSizePolicy(sizePolicy);
        labelPreview->setScaledContents(false);

        formLayout->setWidget(9, QFormLayout::ItemRole::SpanningRole, labelPreview);


        rootLayout->addWidget(formLayoutWidget);


        retranslateUi(TypeOpenCVForm);

        QMetaObject::connectSlotsByName(TypeOpenCVForm);
    } // setupUi

    void retranslateUi(QWidget *TypeOpenCVForm)
    {
        TypeOpenCVForm->setWindowTitle(QCoreApplication::translate("TypeOpenCVForm", "TypeOpenCVForm", nullptr));
        label->setText(QCoreApplication::translate("TypeOpenCVForm", "\344\273\273\345\212\241\345\220\215\347\247\260", nullptr));
        label_2->setText(QCoreApplication::translate("TypeOpenCVForm", "\345\210\206\346\225\260\351\230\210\345\200\274", nullptr));
        label_3->setText(QCoreApplication::translate("TypeOpenCVForm", "\346\230\257\345\220\246\351\232\217\346\234\272\347\202\271\345\207\273", nullptr));
        randomClickCheckBox->setText(QString());
        label_6->setText(QCoreApplication::translate("TypeOpenCVForm", "\345\274\200\345\220\257\351\242\234\350\211\262\345\210\244\346\226\255", nullptr));
#if QT_CONFIG(tooltip)
        colorCheckCheckBox->setToolTip(QCoreApplication::translate("TypeOpenCVForm", "\345\274\200\345\220\257\345\220\216\345\257\271\345\214\271\351\205\215\345\214\272\345\237\237\345\201\232 HSV \351\242\234\350\211\262\346\240\241\351\252\214\357\274\214\345\217\257\345\214\272\345\210\206\345\220\214\345\275\242\347\212\266\344\270\215\345\220\214\350\211\262\347\232\204\346\250\241\346\235\277\357\274\210\345\246\202\344\270\215\345\220\214\347\212\266\346\200\201\347\232\204\346\214\211\351\222\256\357\274\211", nullptr));
#endif // QT_CONFIG(tooltip)
        colorCheckCheckBox->setText(QString());
#if QT_CONFIG(tooltip)
        excludeLabel->setToolTip(QCoreApplication::translate("TypeOpenCVForm", "\351\232\217\346\234\272\347\202\271\345\207\273\346\227\266\346\216\222\351\231\244\345\214\271\351\205\215\346\241\206\345\267\246/\345\217\263\344\270\244\344\276\247\347\232\204\346\257\224\344\276\213\345\214\272\345\237\237\357\274\2100~90%\357\274\211\357\274\214\345\205\250 0 \344\270\215\346\216\222\351\231\244", nullptr));
#endif // QT_CONFIG(tooltip)
        excludeLabel->setText(QCoreApplication::translate("TypeOpenCVForm", "\345\267\246\345\217\263\350\276\271\346\241\206\346\216\222\351\231\244", nullptr));
        excludeLeftLabel->setText(QCoreApplication::translate("TypeOpenCVForm", "\345\267\246", nullptr));
        excludeRightLabel->setText(QCoreApplication::translate("TypeOpenCVForm", "\345\217\263", nullptr));
#if QT_CONFIG(tooltip)
        excludeTbLabel->setToolTip(QCoreApplication::translate("TypeOpenCVForm", "\351\232\217\346\234\272\347\202\271\345\207\273\346\227\266\346\216\222\351\231\244\345\214\271\351\205\215\346\241\206\344\270\212/\344\270\213\344\270\244\344\276\247\347\232\204\346\257\224\344\276\213\345\214\272\345\237\237\357\274\2100~90%\357\274\211\357\274\214\345\205\250 0 \344\270\215\346\216\222\351\231\244", nullptr));
#endif // QT_CONFIG(tooltip)
        excludeTbLabel->setText(QCoreApplication::translate("TypeOpenCVForm", "\344\270\212\344\270\213\350\276\271\346\241\206\346\216\222\351\231\244", nullptr));
        excludeTopLabel->setText(QCoreApplication::translate("TypeOpenCVForm", "\344\270\212", nullptr));
        excludeBottomLabel->setText(QCoreApplication::translate("TypeOpenCVForm", "\344\270\213", nullptr));
        label_5->setText(QCoreApplication::translate("TypeOpenCVForm", "\350\257\206\345\210\253\345\244\261\350\264\245\345\244\204\347\220\206", nullptr));
        stepInputLabel->setText(QCoreApplication::translate("TypeOpenCVForm", "\350\267\263\350\275\254\346\255\245\351\252\244", nullptr));
        label_4->setText(QCoreApplication::translate("TypeOpenCVForm", "\346\210\252\345\233\276", nullptr));
        btnCapture->setText(QCoreApplication::translate("TypeOpenCVForm", "\347\202\271\345\207\273\345\274\200\345\247\213\346\210\252\345\233\276", nullptr));
        labelPreview->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class TypeOpenCVForm: public Ui_TypeOpenCVForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TYPEOPENCVFORM_H
