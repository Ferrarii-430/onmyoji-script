//
// Created by CZY on 2025/9/30.
//

#ifndef EDITTASKDIALOG_H
#define EDITTASKDIALOG_H

#include <QDialog>
#include <QJsonObject>

#include "src/ui/typeOpenCVForm/typeopencvform.h"
#include "src/ui/waitForm/waitform.h"
#include "src/ui/ocrForm/OcrForm.h"
#include "src/ui/yoloForm/YoloForm.h"

enum class EditMode { Add, Edit };

QT_BEGIN_NAMESPACE
namespace Ui { class EditTaskDialog; }
QT_END_NAMESPACE

class EditTaskDialog : public QDialog {
Q_OBJECT

signals:
    void imagePathRequested(const QString &path); // 定义信号

public:
    EditMode m_mode;
    QJsonObject m_stepData;
    QJsonObject m_resultData;
    explicit EditTaskDialog(EditMode mode, const QJsonObject &stepData, const QString &configId, QWidget *parent = nullptr);
    QJsonObject collectData() const;
    QJsonObject resultData() const;
    ~EditTaskDialog() override;
void onTestButtonClick();
bool validateWaitFormData(const QJsonObject& data);
bool validateOpenCVFormData(const QJsonObject& data);
bool validateOcrFormData(const QJsonObject& data);
bool validateYoloFormData(const QJsonObject& data);
bool validateData();
void accept();
void setCurrentIndex();

private:
    TypeOpenCVForm* typeForm;
    WaitForm* waitForm;
    OcrForm* ocrForm;
    YoloForm* yoloForm;
    Ui::EditTaskDialog *ui;
};


#endif //EDITTASKDIALOG_H
