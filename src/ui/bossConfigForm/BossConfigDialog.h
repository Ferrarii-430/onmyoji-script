//
// 超鬼王专属配置页：鬼王 → 阵容名称 / 绿标位置 / 红标 BOSS / 强力追击 的列表。
//

#ifndef BOSSCONFIGDIALOG_H
#define BOSSCONFIGDIALOG_H

#include <QDialog>
#include <QJsonArray>
#include <QString>

class QCheckBox;
class QTableWidget;

// 只负责编辑与校验，不落盘：点「确定」后由调用方写回方案对象的
// bossConfig 数组（随 config.json 持久化）。
class BossConfigDialog : public QDialog {
Q_OBJECT

public:
    // schemeName 仅用于窗口标题；list 为进入编辑时的初始内容
    explicit BossConfigDialog(const QString &schemeName, const QJsonArray &list,
                              QWidget *parent = nullptr);

    // 当前表格内容：跳过鬼王名称为空的行，绿标位置已收敛到 0-5
    QJsonArray list() const;

public slots:
    // 鬼王名称必填且唯一，校验不通过时停在弹窗内
    void accept() override;

private:
    void appendRow(const QString &bossName, const QString &lineupName,
                   int greenMark, bool markBoss, bool strongPursuit);
    void addRow();
    void removeSelectedRows();
    void moveRow(int delta);
    // 鬼王名称重复标红，并清掉上一次的高亮
    void refreshHighlight();

    QString bossNameAt(int row) const;
    QString lineupNameAt(int row) const;
    int greenMarkAt(int row) const;
    // 开关列(红标 BOSS / 强力追击)都是居中的 QCheckBox 单元格控件
    QCheckBox *checkBoxAt(int row, int column) const;
    bool boolCellAt(int row, int column) const;

    QTableWidget *m_table = nullptr;
};

#endif //BOSSCONFIGDIALOG_H
