//
// Created by CZY on 2025/9/30.
//

#ifndef WAITFORM_H
#define WAITFORM_H

#include <QJsonObject>
#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class WaitForm; }
QT_END_NAMESPACE

class WaitForm : public QWidget {
Q_OBJECT

public:
    explicit WaitForm(QWidget *parent = nullptr);
    ~WaitForm() override;
    void loadFromJson(const QString &configId, const QJsonObject &obj);
    QJsonObject toJson() const;
private:
    Ui::WaitForm *ui;
    QJsonObject stepDataCopy;
};


#endif //WAITFORM_H
