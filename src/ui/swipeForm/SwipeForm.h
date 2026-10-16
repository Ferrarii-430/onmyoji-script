//
// Created by CZY on 2026/10/15.
//

#ifndef SWIPE_FORM_H
#define SWIPE_FORM_H

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class SwipeForm; }
QT_END_NAMESPACE

// 滑动步骤表单：设置起点（按下）/终点（抬起）两个百分比坐标，
// 从点1按下滑动到点2后抬起，不做任何识别。
// 预览复用截图获取/上传图片作为底图（与 RoiClickForm 交互一致），
// 点击预览可快速把坐标取到当前编辑点。
class SwipeForm : public QWidget {
Q_OBJECT

public:
    explicit SwipeForm(QWidget *parent = nullptr);
    ~SwipeForm() override;
    void loadFromJson(const QString &configId, const QJsonObject& obj);
    void initStepInputBoxSelect(QString configId, const QString& stepsId);
    QJsonObject toJson() const;

private slots:
    void onUploadImageClicked();
    void onCaptureImageClicked();
    void updateSwipePreview();

private:
    Ui::SwipeForm *ui;
    QString currentConfigId;
    QJsonObject stepDataCopy;
    QMap<QString,QString> stepSelect;
};
#endif //SWIPE_FORM_H
