//
// Created by CZY on 2025/10/11.
//

#ifndef OCR_FORM_H
#define OCR_FORM_H

#include <QJsonObject>
#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class OcrForm; }
QT_END_NAMESPACE

class OcrForm : public QWidget {
Q_OBJECT

public:
    explicit OcrForm(QWidget *parent = nullptr);
    ~OcrForm() override;
    void loadFromJson(const QJsonObject& obj);
    QJsonObject toJson() const;

private:
    Ui::OcrForm *ui;
    QJsonObject stepDataCopy;
};


#endif //OCR_FORM_H
