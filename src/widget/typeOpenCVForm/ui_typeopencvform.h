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
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TypeOpenCVForm
{
public:
    QWidget *formLayoutWidget;
    QFormLayout *formLayout;
    QLabel *label;
    QLineEdit *lineTaskNameEdit;
    QLabel *label_2;
    QDoubleSpinBox *spinScoreBox;
    QLabel *label_3;
    QCheckBox *randomClickCheckBox;
    QLabel *label_5;
    QComboBox *opencvErrorHandle;
    QLabel *label_4;
    QToolButton *btnCapture;
    QLabel *labelPreview;
    QLabel *stepInputLabel;
    QComboBox *stepInputBox;

    void setupUi(QWidget *TypeOpenCVForm)
    {
        if (TypeOpenCVForm->objectName().isEmpty())
            TypeOpenCVForm->setObjectName("TypeOpenCVForm");
        TypeOpenCVForm->resize(291, 331);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(TypeOpenCVForm->sizePolicy().hasHeightForWidth());
        TypeOpenCVForm->setSizePolicy(sizePolicy);
        formLayoutWidget = new QWidget(TypeOpenCVForm);
        formLayoutWidget->setObjectName("formLayoutWidget");
        formLayoutWidget->setGeometry(QRect(0, 0, 291, 331));
        formLayout = new QFormLayout(formLayoutWidget);
        formLayout->setObjectName("formLayout");
        formLayout->setVerticalSpacing(20);
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

        label_5 = new QLabel(formLayoutWidget);
        label_5->setObjectName("label_5");

        formLayout->setWidget(3, QFormLayout::ItemRole::LabelRole, label_5);

        opencvErrorHandle = new QComboBox(formLayoutWidget);
        opencvErrorHandle->setObjectName("opencvErrorHandle");

        formLayout->setWidget(3, QFormLayout::ItemRole::FieldRole, opencvErrorHandle);

        label_4 = new QLabel(formLayoutWidget);
        label_4->setObjectName("label_4");

        formLayout->setWidget(5, QFormLayout::ItemRole::LabelRole, label_4);

        btnCapture = new QToolButton(formLayoutWidget);
        btnCapture->setObjectName("btnCapture");

        formLayout->setWidget(5, QFormLayout::ItemRole::FieldRole, btnCapture);

        labelPreview = new QLabel(formLayoutWidget);
        labelPreview->setObjectName("labelPreview");
        sizePolicy.setHeightForWidth(labelPreview->sizePolicy().hasHeightForWidth());
        labelPreview->setSizePolicy(sizePolicy);
        labelPreview->setScaledContents(false);

        formLayout->setWidget(6, QFormLayout::ItemRole::SpanningRole, labelPreview);

        stepInputLabel = new QLabel(formLayoutWidget);
        stepInputLabel->setObjectName("stepInputLabel");

        formLayout->setWidget(4, QFormLayout::ItemRole::LabelRole, stepInputLabel);

        stepInputBox = new QComboBox(formLayoutWidget);
        stepInputBox->setObjectName("stepInputBox");

        formLayout->setWidget(4, QFormLayout::ItemRole::FieldRole, stepInputBox);


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
        label_5->setText(QCoreApplication::translate("TypeOpenCVForm", "\350\257\206\345\210\253\345\244\261\350\264\245\345\244\204\347\220\206", nullptr));
        label_4->setText(QCoreApplication::translate("TypeOpenCVForm", "\346\210\252\345\233\276", nullptr));
        btnCapture->setText(QCoreApplication::translate("TypeOpenCVForm", "\347\202\271\345\207\273\345\274\200\345\247\213\346\210\252\345\233\276", nullptr));
        labelPreview->setText(QString());
        stepInputLabel->setText(QCoreApplication::translate("TypeOpenCVForm", "\350\267\263\350\275\254\346\255\245\351\252\244", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TypeOpenCVForm: public Ui_TypeOpenCVForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TYPEOPENCVFORM_H
