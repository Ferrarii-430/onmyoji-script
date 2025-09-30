//
// Created by CZY on 2025/9/30.
//

#ifndef EDITTASKDIALOG_H
#define EDITTASKDIALOG_H

#include <QDialog>
#include <QJsonObject>

#include "src/widget/typeOpenCVForm/typeopencvform.h"
#include "src/widget/waitForm/waitform.h"

enum class EditMode { Add, Edit };

QT_BEGIN_NAMESPACE
namespace Ui { class EditTaskDialog; }
QT_END_NAMESPACE

class EditTaskDialog : public QDialog {
Q_OBJECT

public:
    EditMode m_mode;
    QJsonObject m_stepData;
    QJsonObject m_resultData;
    explicit EditTaskDialog(EditMode mode, const QJsonObject &stepData, QWidget *parent = nullptr);
    QJsonObject collectData() const;
    QJsonObject resultData() const;
    ~EditTaskDialog() override;
    void setCurrentIndex();

private:
    TypeOpenCVForm* typeForm;
    WaitForm* waitForm;
    Ui::EditTaskDialog *ui;
};


#endif //EDITTASKDIALOG_H
