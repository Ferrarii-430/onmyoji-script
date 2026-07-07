#ifndef YOLO_FORM_H
#define YOLO_FORM_H

#include <QJsonObject>
#include <QMap>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

// YOLO 识别步骤编辑表单：选择目标标签（来自 yolo_label_catalog.json），
// 配置分数阈值/随机点击/识别失败处理，步骤 JSON 中以 yoloLabel 保存标签名。
class YoloForm : public QWidget {
Q_OBJECT

public:
    explicit YoloForm(QWidget *parent = nullptr);
    void loadFromJson(const QString &configId, const QJsonObject& obj);
    void initStepInputBoxSelect(QString configId, const QString& stepsId);
    QJsonObject toJson() const;

private:
    void populateLabelBox();

    QLineEdit* lineTaskNameEdit;
    QComboBox* labelBox;
    QDoubleSpinBox* spinScoreBox;
    QCheckBox* randomClickCheckBox;
    QComboBox* errorHandleBox;
    QLabel* stepInputLabel;
    QComboBox* stepInputBox;

    QString currentConfigId;
    QJsonObject stepDataCopy;
    QMap<QString,QString> stepSelect;
};

#endif //YOLO_FORM_H
