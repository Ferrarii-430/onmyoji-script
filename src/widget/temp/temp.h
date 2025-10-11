//
// Created by CZY on 2025/10/11.
//

#ifndef TEMP_H
#define TEMP_H

#include <QJsonObject>
#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class temp; }
QT_END_NAMESPACE

class temp : public QWidget {
Q_OBJECT

public:
    explicit temp(QWidget *parent = nullptr);
    ~temp() override;
void loadFromJson(const QJsonObject& obj);
QJsonObject toJson() const;

private:
    Ui::temp *ui;
    QJsonObject stepDataCopy;
};


#endif //TEMP_H
