/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_mainwindow
{
public:
    QLabel *label;
    QListWidget *listWidget;
    QTableWidget *tableWidget;
    QLabel *label_3;
    QPlainTextEdit *plainTextEdit;
    QLabel *label_4;
    QToolButton *startTaskButton;
    QToolButton *stopTaskButton;
    QLabel *label_5;
    QSpinBox *taskCycleNumber;
    QLabel *label_6;
    QLabel *currentTaskName;
    QPushButton *programmeAddBtn;
    QPushButton *programmeRemoveBtn;
    QLabel *openCVIdentifyLabel;
    QWidget *horizontalLayoutWidget;
    QHBoxLayout *horizontalLayout;
    QLabel *label_2;
    QToolButton *programmeContentUpButton;
    QToolButton *programmeContentDownButton;
    QPushButton *programmeContentAddBtn;
    QToolButton *settingButton;

    void setupUi(QWidget *mainwindow)
    {
        if (mainwindow->objectName().isEmpty())
            mainwindow->setObjectName("mainwindow");
        mainwindow->setEnabled(true);
        mainwindow->resize(851, 478);
        label = new QLabel(mainwindow);
        label->setObjectName("label");
        label->setGeometry(QRect(10, 10, 31, 21));
        listWidget = new QListWidget(mainwindow);
        listWidget->setObjectName("listWidget");
        listWidget->setGeometry(QRect(10, 40, 181, 131));
        listWidget->setEditTriggers(QAbstractItemView::EditTrigger::DoubleClicked);
        tableWidget = new QTableWidget(mainwindow);
        tableWidget->setObjectName("tableWidget");
        tableWidget->setGeometry(QRect(10, 210, 451, 251));
        tableWidget->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        label_3 = new QLabel(mainwindow);
        label_3->setObjectName("label_3");
        label_3->setGeometry(QRect(200, 10, 81, 21));
        plainTextEdit = new QPlainTextEdit(mainwindow);
        plainTextEdit->setObjectName("plainTextEdit");
        plainTextEdit->setGeometry(QRect(200, 40, 261, 131));
        plainTextEdit->setReadOnly(true);
        label_4 = new QLabel(mainwindow);
        label_4->setObjectName("label_4");
        label_4->setGeometry(QRect(480, 10, 131, 21));
        startTaskButton = new QToolButton(mainwindow);
        startTaskButton->setObjectName("startTaskButton");
        startTaskButton->setGeometry(QRect(540, 420, 111, 31));
        stopTaskButton = new QToolButton(mainwindow);
        stopTaskButton->setObjectName("stopTaskButton");
        stopTaskButton->setGeometry(QRect(690, 420, 111, 31));
        label_5 = new QLabel(mainwindow);
        label_5->setObjectName("label_5");
        label_5->setGeometry(QRect(542, 350, 61, 20));
        taskCycleNumber = new QSpinBox(mainwindow);
        taskCycleNumber->setObjectName("taskCycleNumber");
        taskCycleNumber->setGeometry(QRect(620, 350, 70, 21));
        taskCycleNumber->setMaximum(999);
        label_6 = new QLabel(mainwindow);
        label_6->setObjectName("label_6");
        label_6->setGeometry(QRect(520, 310, 81, 20));
        currentTaskName = new QLabel(mainwindow);
        currentTaskName->setObjectName("currentTaskName");
        currentTaskName->setGeometry(QRect(620, 310, 201, 16));
        programmeAddBtn = new QPushButton(mainwindow);
        programmeAddBtn->setObjectName("programmeAddBtn");
        programmeAddBtn->setGeometry(QRect(70, 10, 61, 23));
        programmeRemoveBtn = new QPushButton(mainwindow);
        programmeRemoveBtn->setObjectName("programmeRemoveBtn");
        programmeRemoveBtn->setGeometry(QRect(130, 10, 61, 23));
        openCVIdentifyLabel = new QLabel(mainwindow);
        openCVIdentifyLabel->setObjectName("openCVIdentifyLabel");
        openCVIdentifyLabel->setGeometry(QRect(470, 40, 371, 191));
        horizontalLayoutWidget = new QWidget(mainwindow);
        horizontalLayoutWidget->setObjectName("horizontalLayoutWidget");
        horizontalLayoutWidget->setGeometry(QRect(10, 170, 451, 41));
        horizontalLayout = new QHBoxLayout(horizontalLayoutWidget);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        label_2 = new QLabel(horizontalLayoutWidget);
        label_2->setObjectName("label_2");

        horizontalLayout->addWidget(label_2);

        programmeContentUpButton = new QToolButton(horizontalLayoutWidget);
        programmeContentUpButton->setObjectName("programmeContentUpButton");

        horizontalLayout->addWidget(programmeContentUpButton);

        programmeContentDownButton = new QToolButton(horizontalLayoutWidget);
        programmeContentDownButton->setObjectName("programmeContentDownButton");

        horizontalLayout->addWidget(programmeContentDownButton);

        programmeContentAddBtn = new QPushButton(horizontalLayoutWidget);
        programmeContentAddBtn->setObjectName("programmeContentAddBtn");

        horizontalLayout->addWidget(programmeContentAddBtn);

        settingButton = new QToolButton(mainwindow);
        settingButton->setObjectName("settingButton");
        settingButton->setGeometry(QRect(440, 10, 24, 21));

        retranslateUi(mainwindow);

        QMetaObject::connectSlotsByName(mainwindow);
    } // setupUi

    void retranslateUi(QWidget *mainwindow)
    {
        mainwindow->setWindowTitle(QCoreApplication::translate("mainwindow", "onmyoji-script v1.3", nullptr));
        label->setText(QCoreApplication::translate("mainwindow", "\346\226\271\346\241\210", nullptr));
        label_3->setText(QCoreApplication::translate("mainwindow", "\346\227\245\345\277\227\350\276\223\345\207\272", nullptr));
        label_4->setText(QCoreApplication::translate("mainwindow", "OpenCV\350\257\206\345\210\253\347\274\251\347\225\245\345\233\276", nullptr));
        startTaskButton->setText(QCoreApplication::translate("mainwindow", "\345\220\257\345\212\250", nullptr));
        stopTaskButton->setText(QCoreApplication::translate("mainwindow", "\345\201\234\346\255\242", nullptr));
        label_5->setText(QCoreApplication::translate("mainwindow", "\345\276\252\347\216\257\346\254\241\346\225\260", nullptr));
        label_6->setText(QCoreApplication::translate("mainwindow", "\345\275\223\345\211\215\344\273\273\345\212\241\346\226\271\346\241\210", nullptr));
        currentTaskName->setText(QCoreApplication::translate("mainwindow", "TextLabel", nullptr));
        programmeAddBtn->setText(QCoreApplication::translate("mainwindow", "\346\267\273\345\212\240\346\226\271\346\241\210", nullptr));
        programmeRemoveBtn->setText(QCoreApplication::translate("mainwindow", "\345\210\240\351\231\244\346\226\271\346\241\210", nullptr));
        openCVIdentifyLabel->setText(QString());
        label_2->setText(QCoreApplication::translate("mainwindow", "\345\275\223\345\211\215\346\226\271\346\241\210\345\206\205\345\256\271                                                ", nullptr));
        programmeContentUpButton->setText(QCoreApplication::translate("mainwindow", "\342\206\221", nullptr));
        programmeContentDownButton->setText(QCoreApplication::translate("mainwindow", "\342\206\223", nullptr));
        programmeContentAddBtn->setText(QCoreApplication::translate("mainwindow", "\346\267\273\345\212\240\346\226\271\346\241\210\345\206\205\345\256\271", nullptr));
        settingButton->setText(QCoreApplication::translate("mainwindow", "\342\232\231\357\270\217", nullptr));
    } // retranslateUi

};

namespace Ui {
    class mainwindow: public Ui_mainwindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
