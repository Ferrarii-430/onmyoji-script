#include "BossConfigDialog.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QColor>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <functional>

namespace {

constexpr int kColBossName = 0;
constexpr int kColLineup = 1;
constexpr int kColGreenMark = 2;
constexpr int kColMarkBoss = 3;
constexpr int kColStrongPursuit = 4;

// 开关列（红标 BOSS / 强力追击）：单元格里放一个居中的复选框。
// 用单元格控件而非可勾选 item，QSS 的开关样式才能生效，也才能居中。
QWidget *createCheckCell(QTableWidget *table, bool checked)
{
    auto *container = new QWidget(table);
    container->setObjectName("checkCell");

    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *check = new QCheckBox(container);
    // 全局 QSS 给 QCheckBox 设了 spacing:6px，空文本时右侧会多出 6px，
    // 勾选框会偏左；这里清零才能做到真正的水平居中
    check->setStyleSheet("QCheckBox { spacing: 0px; }");
    check->setChecked(checked);

    layout->addStretch(1);
    layout->addWidget(check);
    layout->addStretch(1);
    return container;
}

// 绿标位置列：进入编辑时弹 0-5 的 SpinBox，避免手打越界；
// 用 delegate 而不是 cellWidget，行增删/上下移动时数据才会跟着走
class GreenMarkDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        auto *spin = new QSpinBox(parent);
        spin->setRange(0, 5);
        spin->setAlignment(Qt::AlignCenter);
        return spin;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
            spin->setValue(index.data(Qt::EditRole).toInt());
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
            spin->interpretText();
            model->setData(index, spin->value(), Qt::EditRole);
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }
};

} // namespace

BossConfigDialog::BossConfigDialog(const QString &schemeName, const QJsonArray &list,
                                   QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(schemeName.isEmpty()
                       ? QStringLiteral("超鬼王配置")
                       : QStringLiteral("超鬼王配置 - %1").arg(schemeName));
    resize(760, 460);
    setMinimumSize(620, 300);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 12);
    root->setSpacing(10);

    auto *hint = new QLabel(
        QStringLiteral("每行一条鬼王配置：按识别到的鬼王名称取对应阵容与标记，没有则使用默认阵容名称。"
                       "绿标位置 0-5（0 表示不标记），红标 BOSS 为是否标记敌方 BOSS，"
                       "强力追击为是否开启强力追击。"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #6b7280; font-size: 9pt;");
    root->addWidget(hint);

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("鬼王名称"), QStringLiteral("阵容名称"),
                                        QStringLiteral("绿标位置"), QStringLiteral("红标 BOSS"),
                                        QStringLiteral("强力追击")});
    m_table->horizontalHeader()->setSectionResizeMode(kColBossName, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(kColLineup, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(kColGreenMark, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(kColMarkBoss, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(kColStrongPursuit, QHeaderView::Fixed);
    m_table->horizontalHeader()->resizeSection(kColBossName, 180);
    m_table->horizontalHeader()->resizeSection(kColGreenMark, 85);
    m_table->horizontalHeader()->resizeSection(kColMarkBoss, 90);
    m_table->horizontalHeader()->resizeSection(kColStrongPursuit, 90);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setItemDelegateForColumn(kColGreenMark, new GreenMarkDelegate(this));
    // 开关列用单元格控件承载复选框，容器需透明，才能露出行的斑马纹/选中色
    m_table->setStyleSheet("#checkCell { background: transparent; }");
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked
                             | QAbstractItemView::SelectedClicked
                             | QAbstractItemView::EditKeyPressed);
    root->addWidget(m_table, 1);

    auto *tools = new QHBoxLayout;
    tools->setSpacing(8);
    auto *addBtn = new QPushButton(QStringLiteral("添加"), this);
    auto *delBtn = new QPushButton(QStringLiteral("删除"), this);
    auto *upBtn = new QPushButton(QStringLiteral("上移"), this);
    auto *downBtn = new QPushButton(QStringLiteral("下移"), this);
    tools->addWidget(addBtn);
    tools->addWidget(delBtn);
    tools->addWidget(upBtn);
    tools->addWidget(downBtn);
    tools->addStretch(1);
    root->addLayout(tools);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &BossConfigDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &BossConfigDialog::reject);
    connect(addBtn, &QPushButton::clicked, this, &BossConfigDialog::addRow);
    connect(delBtn, &QPushButton::clicked, this, &BossConfigDialog::removeSelectedRows);
    connect(upBtn, &QPushButton::clicked, this, [this]() { moveRow(-1); });
    connect(downBtn, &QPushButton::clicked, this, [this]() { moveRow(1); });
    connect(m_table, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem *) { refreshHighlight(); });

    for (const QJsonValue &value : list) {
        const QJsonObject entry = value.toObject();
        const QString bossName = entry.value("bossName").toString();
        if (bossName.isEmpty()) {
            continue;
        }
        appendRow(bossName,
                  entry.value("lineupName").toString(),
                  entry.value("greenMark").toInt(0),
                  entry.value("markBoss").toBool(false),
                  entry.value("strongPursuit").toBool(false));
    }
    // 空配置也先给一行空行，进来就能直接敲
    if (m_table->rowCount() == 0) {
        appendRow(QString(), QString(), 0, false, false);
    }

    refreshHighlight();
    m_table->setCurrentCell(0, kColBossName);
    m_table->setFocus();
}

QString BossConfigDialog::bossNameAt(int row) const
{
    const QTableWidgetItem *item = m_table ? m_table->item(row, kColBossName) : nullptr;
    return item ? item->text().trimmed() : QString();
}

QString BossConfigDialog::lineupNameAt(int row) const
{
    const QTableWidgetItem *item = m_table ? m_table->item(row, kColLineup) : nullptr;
    return item ? item->text().trimmed() : QString();
}

int BossConfigDialog::greenMarkAt(int row) const
{
    const QTableWidgetItem *item = m_table ? m_table->item(row, kColGreenMark) : nullptr;
    if (!item) {
        return 0;
    }
    bool ok = false;
    const int value = item->text().trimmed().toInt(&ok);
    if (!ok || value < 0 || value > 5) {
        return 0;
    }
    return value;
}

QCheckBox *BossConfigDialog::checkBoxAt(int row, int column) const
{
    QWidget *cell = m_table ? m_table->cellWidget(row, column) : nullptr;
    return cell ? cell->findChild<QCheckBox *>() : nullptr;
}

bool BossConfigDialog::boolCellAt(int row, int column) const
{
    const QCheckBox *check = checkBoxAt(row, column);
    return check && check->isChecked();
}

void BossConfigDialog::appendRow(const QString &bossName, const QString &lineupName,
                                 int greenMark, bool markBoss, bool strongPursuit)
{
    // 批量载入时逐行触发 itemChanged 会反复重算高亮，统一屏蔽后由调用方刷新
    QSignalBlocker blocker(m_table);

    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *nameItem = new QTableWidgetItem(bossName);
    m_table->setItem(row, kColBossName, nameItem);

    m_table->setItem(row, kColLineup, new QTableWidgetItem(lineupName));

    auto *greenItem = new QTableWidgetItem(QString::number(qBound(0, greenMark, 5)));
    greenItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, kColGreenMark, greenItem);

    // 两个开关列都用居中的复选框单元格控件，默认均为关闭
    m_table->setCellWidget(row, kColMarkBoss, createCheckCell(m_table, markBoss));
    m_table->setCellWidget(row, kColStrongPursuit, createCheckCell(m_table, strongPursuit));
}

void BossConfigDialog::addRow()
{
    appendRow(QString(), QString(), 0, false, false);
    const int row = m_table->rowCount() - 1;
    m_table->setCurrentCell(row, kColBossName);
    if (QTableWidgetItem *item = m_table->item(row, kColBossName)) {
        m_table->editItem(item);
    }
}

void BossConfigDialog::removeSelectedRows()
{
    QList<int> rows;
    for (const QModelIndex &index : m_table->selectionModel()->selectedRows()) {
        rows.append(index.row());
    }
    if (rows.isEmpty() && m_table->currentRow() >= 0) {
        rows.append(m_table->currentRow());
    }
    if (rows.isEmpty()) {
        return;
    }

    // 从后往前删，避免前面的行被删后下标整体前移
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        m_table->removeRow(row);
    }
    refreshHighlight();
}

void BossConfigDialog::moveRow(int delta)
{
    const int row = m_table->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= m_table->rowCount()) {
        return;
    }

    QSignalBlocker blocker(m_table);
    // 文本列整体交换
    for (int col = kColBossName; col <= kColGreenMark; ++col) {
        QTableWidgetItem *current = m_table->item(row, col);
        QTableWidgetItem *other = m_table->item(target, col);
        if (!current || !other) {
            continue;
        }
        const QString text = current->text();
        current->setText(other->text());
        other->setText(text);
    }
    // 开关列是单元格控件（不随 item 文本交换），逐列换两行的勾选状态
    for (int col : {kColMarkBoss, kColStrongPursuit}) {
        QCheckBox *current = checkBoxAt(row, col);
        QCheckBox *other = checkBoxAt(target, col);
        if (!current || !other) {
            continue;
        }
        const bool checked = current->isChecked();
        current->setChecked(other->isChecked());
        other->setChecked(checked);
    }

    const int column = m_table->currentColumn() < 0 ? kColBossName : m_table->currentColumn();
    m_table->setCurrentCell(target, column);
}

void BossConfigDialog::refreshHighlight()
{
    if (!m_table) {
        return;
    }
    QSignalBlocker blocker(m_table);

    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (QTableWidgetItem *item = m_table->item(row, kColBossName)) {
            item->setData(Qt::BackgroundRole, QVariant());
            item->setToolTip(QString());
        }
    }

    QSet<QString> seen;
    QSet<QString> duplicated;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QString name = bossNameAt(row);
        if (name.isEmpty()) {
            continue;
        }
        if (seen.contains(name)) {
            duplicated.insert(name);
        } else {
            seen.insert(name);
        }
    }

    if (duplicated.isEmpty()) {
        return;
    }
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (!duplicated.contains(bossNameAt(row))) {
            continue;
        }
        if (QTableWidgetItem *item = m_table->item(row, kColBossName)) {
            item->setBackground(QColor(0xff, 0xe3, 0xe3));
            item->setToolTip(QStringLiteral("鬼王名称重复"));
        }
    }
}

void BossConfigDialog::accept()
{
    refreshHighlight();

    QStringList emptyNameRows;
    QStringList duplicateNames;
    QSet<QString> seen;
    int firstBadRow = -1;

    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QString name = bossNameAt(row);
        const QString lineup = lineupNameAt(row);

        if (name.isEmpty()) {
            // 整行都空视为未使用的空行；只填了阵容/勾了开关才提示缺鬼王名称
            const bool touched = !lineup.isEmpty() || greenMarkAt(row) != 0
                                 || boolCellAt(row, kColMarkBoss)
                                 || boolCellAt(row, kColStrongPursuit);
            if (touched) {
                emptyNameRows.append(QString::number(row + 1));
                if (firstBadRow < 0) {
                    firstBadRow = row;
                }
            }
            continue;
        }
        if (seen.contains(name)) {
            if (!duplicateNames.contains(name)) {
                duplicateNames.append(name);
            }
            if (firstBadRow < 0) {
                firstBadRow = row;
            }
        } else {
            seen.insert(name);
        }
    }

    if (!emptyNameRows.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法保存"),
                             QStringLiteral("第 %1 行未填鬼王名称。")
                                 .arg(emptyNameRows.join(QStringLiteral("、"))));
        m_table->setCurrentCell(firstBadRow, kColBossName);
        return;
    }
    if (!duplicateNames.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法保存"),
                             QStringLiteral("鬼王名称重复：%1")
                                 .arg(duplicateNames.join(QStringLiteral("、"))));
        m_table->setCurrentCell(firstBadRow, kColBossName);
        return;
    }

    // 绿标位置若是手打的空值/越界值，这里统一收敛一次，避免写进配置后取不到
    {
        QSignalBlocker blocker(m_table);
        for (int row = 0; row < m_table->rowCount(); ++row) {
            if (QTableWidgetItem *item = m_table->item(row, kColGreenMark)) {
                item->setText(QString::number(greenMarkAt(row)));
            }
        }
    }

    QDialog::accept();
}

QJsonArray BossConfigDialog::list() const
{
    QJsonArray result;
    if (!m_table) {
        return result;
    }
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QString bossName = bossNameAt(row);
        if (bossName.isEmpty()) {
            continue;
        }
        QJsonObject entry;
        entry["bossName"] = bossName;
        entry["lineupName"] = lineupNameAt(row);
        entry["greenMark"] = greenMarkAt(row);
        entry["markBoss"] = boolCellAt(row, kColMarkBoss);
        entry["strongPursuit"] = boolCellAt(row, kColStrongPursuit);
        result.append(entry);
    }
    return result;
}
