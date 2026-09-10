//
// Created by CZY on 2026/9/10.
//

#ifndef ROI_CLICK_FORM_H
#define ROI_CLICK_FORM_H

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class RoiClickForm; }
QT_END_NAMESPACE

// 范围点击步骤表单：选择一个百分比区域后直接点击（随机取点或点击中心），
// 不做任何识别。区域预览复用 ocrForm 的 RoiPreviewWidget，
// 支持上传图片 / 截图获取作为预览底图，与 OcrForm 的交互保持一致。
class RoiClickForm : public QWidget {
Q_OBJECT

public:
    explicit RoiClickForm(QWidget *parent = nullptr);
    ~RoiClickForm() override;
    void loadFromJson(const QString &configId, const QJsonObject& obj);
    void initStepInputBoxSelect(QString configId, const QString& stepsId);
    QJsonObject toJson() const;

private slots:
    void onUploadImageClicked();
    void onCaptureImageClicked();
    void updateRoiPreview();

private:
    Ui::RoiClickForm *ui;
    QString currentConfigId;
    QJsonObject stepDataCopy;
    QMap<QString,QString> stepSelect;
};
#endif //ROI_CLICK_FORM_H
