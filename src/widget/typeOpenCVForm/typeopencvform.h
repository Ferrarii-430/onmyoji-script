//
// Created by CZY on 2025/9/30.
//

#ifndef TYPEOPENCVFORM_H
#define TYPEOPENCVFORM_H

#include <QJsonObject>
#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class TypeOpenCVForm; }
QT_END_NAMESPACE

class TypeOpenCVForm : public QWidget {
Q_OBJECT

public:
    explicit TypeOpenCVForm(QWidget *parent = nullptr);
    ~TypeOpenCVForm() override;
    void loadFromJson(const QJsonObject &obj);
    QJsonObject toJson() const;



private slots:
    void onCaptureButtonClicked();
    void updatePreview() const;

private:
    Ui::TypeOpenCVForm *ui;
    QPixmap capturedImage;  // 保存截图
    QPixmap originalPixmap_;
    QJsonObject stepDataCopy;
};


#endif //TYPEOPENCVFORM_H
