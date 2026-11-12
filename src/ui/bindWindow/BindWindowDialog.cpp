#include "src/ui/bindWindow/BindWindowDialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "src/core/WindowBindingStore.h"
#include "src/platform/ProcessUtils.h"

namespace {

constexpr int kColumnPid = 0;
constexpr int kColumnTitle = 1;
constexpr int kColumnSize = 2;
constexpr int kColumnState = 3;

} // namespace

const QString& BindWindowDialog::targetProcessName()
{
    // 集中一处，后续若要支持别的游戏只改这里
    static const QString name = QStringLiteral("onmyoji.exe");
    return name;
}

BindWindowDialog::BindWindowDialog(QWidget* parent)
    : QDialog(parent)
    , m_processName(targetProcessName())
{
    buildUi();
    refreshList();
}

BindWindowDialog::~BindWindowDialog() = default;

void BindWindowDialog::buildUi()
{
    setWindowTitle(QStringLiteral("绑定游戏窗口"));
    setMinimumSize(760, 420);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(10);

    auto* headerLabel = new QLabel(
        QStringLiteral("选择要绑定的游戏窗口。多开时每个窗口对应一个独立实例。"), this);
    rootLayout->addWidget(headerLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("PID"),
        QStringLiteral("窗口标题"),
        QStringLiteral("客户区尺寸"),
        QStringLiteral("绑定状态")
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(kColumnPid, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColumnTitle, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(kColumnSize, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColumnState, QHeaderView::ResizeToContents);
    rootLayout->addWidget(m_table, 1);

    m_tipLabel = new QLabel(this);
    m_tipLabel->setWordWrap(true);
    rootLayout->addWidget(m_tipLabel);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    m_refreshButton = new QPushButton(QStringLiteral("刷新列表"), this);
    m_pickButton = new QPushButton(QStringLiteral("点选窗口"), this);
    m_unbindButton = new QPushButton(QStringLiteral("解除绑定"), this);
    m_okButton = new QPushButton(QStringLiteral("确定绑定"), this);
    m_cancelButton = new QPushButton(QStringLiteral("取消"), this);

    m_okButton->setDefault(true);

    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addWidget(m_pickButton);
    buttonLayout->addWidget(m_unbindButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    rootLayout->addLayout(buttonLayout);

    connect(m_refreshButton, &QPushButton::clicked, this, &BindWindowDialog::refreshList);
    connect(m_pickButton, &QPushButton::clicked, this, &BindWindowDialog::pickFromScreen);
    connect(m_unbindButton, &QPushButton::clicked, this, &BindWindowDialog::requestUnbind);
    connect(m_okButton, &QPushButton::clicked, this, &BindWindowDialog::confirmSelection);
    connect(m_cancelButton, &QPushButton::clicked, this, &BindWindowDialog::reject);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        confirmSelection();
    });
}

void BindWindowDialog::refreshList()
{
    if (!m_table) {
        return;
    }

    const quint32 keepPid = currentPid();
    m_table->setRowCount(0);

    const std::vector<DWORD> pids =
        platform::findAllPidsByProcessName(m_processName.toStdString());

    m_table->setRowCount(static_cast<int>(pids.size()));

    for (int row = 0; row < static_cast<int>(pids.size()); ++row) {
        const DWORD pid = pids[static_cast<size_t>(row)];
        HWND hwnd = platform::findWindowByPid(pid);
        const platform::PickedWindow info = platform::describeWindow(hwnd);

        auto* pidItem = new QTableWidgetItem(QString::number(pid));
        pidItem->setData(Qt::UserRole, static_cast<uint>(pid));
        pidItem->setTextAlignment(Qt::AlignCenter);

        const QString title = !hwnd
            ? QStringLiteral("(窗口未找到)")
            : (info.title.isEmpty() ? QStringLiteral("(无标题)") : info.title);
        auto* titleItem = new QTableWidgetItem(title);

        auto* sizeItem = new QTableWidgetItem(
            hwnd ? QStringLiteral("%1 × %2").arg(info.clientWidth).arg(info.clientHeight)
                 : QStringLiteral("-"));
        sizeItem->setTextAlignment(Qt::AlignCenter);

        const int boundIndex = WINDOW_BINDINGS.indexOfPid(static_cast<quint32>(pid));
        auto* stateItem = new QTableWidgetItem(
            boundIndex >= 0 ? QStringLiteral("已绑定") : QStringLiteral("未绑定"));
        stateItem->setTextAlignment(Qt::AlignCenter);

        m_table->setItem(row, kColumnPid, pidItem);
        m_table->setItem(row, kColumnTitle, titleItem);
        m_table->setItem(row, kColumnSize, sizeItem);
        m_table->setItem(row, kColumnState, stateItem);
    }

    if (keepPid != 0) {
        selectRowByPid(keepPid);
    }

    if (pids.empty()) {
        m_tipLabel->setText(
            QStringLiteral("未检测到 %1 进程。请先启动游戏，或点击「点选窗口」直接选取。")
                .arg(m_processName));
        m_okButton->setEnabled(false);
    } else {
        m_tipLabel->setText(
            QStringLiteral("检测到 %1 个 %2 实例。选中一行后点击「确定绑定」，双击行可直接绑定。")
                .arg(pids.size())
                .arg(m_processName));
        m_okButton->setEnabled(true);
    }
}

void BindWindowDialog::pickFromScreen()
{
    // 只关闭自己并带上返回码，遮罩由 selectWindow 在对话框销毁后启动
    done(PickFromScreen);
}

void BindWindowDialog::requestUnbind()
{
    done(Unbind);
}

void BindWindowDialog::confirmSelection()
{
    const quint32 pid = currentPid();
    if (pid == 0) {
        QMessageBox::information(this, QStringLiteral("绑定窗口"),
                                 QStringLiteral("请先选择一个游戏窗口。"));
        return;
    }

    HWND hwnd = platform::findWindowByPid(static_cast<DWORD>(pid));
    if (!hwnd) {
        QMessageBox::warning(this, QStringLiteral("绑定窗口"),
                             QStringLiteral("该进程的窗口已失效，请点击「刷新列表」后重试。"));
        return;
    }

    m_selected = platform::describeWindow(hwnd);
    accept();
}

quint32 BindWindowDialog::currentPid() const
{
    if (!m_table) {
        return 0;
    }

    const int row = m_table->currentRow();
    if (row < 0) {
        return 0;
    }

    const QTableWidgetItem* item = m_table->item(row, kColumnPid);
    if (!item) {
        return 0;
    }
    return static_cast<quint32>(item->data(Qt::UserRole).toUInt());
}

void BindWindowDialog::selectRowByPid(quint32 pid)
{
    if (!m_table || pid == 0) {
        return;
    }

    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem* item = m_table->item(row, kColumnPid);
        if (item && static_cast<quint32>(item->data(Qt::UserRole).toUInt()) == pid) {
            m_table->selectRow(row);
            return;
        }
    }
}

bool BindWindowDialog::selectWindow(QWidget* parent, platform::PickedWindow* out)
{
    if (!out) {
        return false;
    }

    while (true) {
        BindWindowDialog dialog(parent);
        const int code = dialog.exec();

        if (code == QDialog::Accepted) {
            *out = dialog.selected();
            return out->valid();
        }

        if (code == Unbind) {
            // 用空实例表示「解除绑定」，调用方据此清空记录并恢复自动查找
            *out = platform::PickedWindow{};
            return true;
        }

        if (code != PickFromScreen) {
            return false;
        }

        // 对话框已关闭（但仍在作用域内），此时启动遮罩不会形成嵌套模态
        platform::PickedWindow picked;
        if (platform::pickWindow(dialog.processName(), &picked)) {
            *out = picked;
            return true;
        }

        // 点选被取消：重新打开列表让用户继续操作，而不是让整个流程退出
    }
}
