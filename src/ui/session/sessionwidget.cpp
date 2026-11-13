//
// Created by CZY on 2025/9/25.
//

// You may need to build the project (run Qt uic code generator) to get "ui_sessionwidget.h" resolved

#include "sessionwidget.h"
#include "src/core/Logger.h"
#include <opencv2/opencv.hpp>
#include "ui_sessionwidget.h"
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QScopedValueRollback>
#include <QSet>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QDebug>
#include <QApplication>
#include <QEventLoop>
#include <QInputDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include "QMessageBox"

#include "src/core/AppPaths.h"
#include "src/core/AppVersion.h"
#include "src/core/ConfigTypeEnum.h"
#include "src/core/ProfileStore.h"
#include "src/core/SettingManager.h"
#include "src/engine/ScriptActions.h"
#include "src/engine/TaskRunner.h"
#include "src/game/GameWindow.h"
#include "src/game/capture/CaptureService.h"
#include "src/platform/DPIHelper.h"
#include "src/platform/ProcessUtils.h"
#include "src/platform/WindowPicker.h"
#include "src/ui/bindWindow/BindWindowDialog.h"
#include "src/ui/bossConfigForm/BossConfigDialog.h"
#include "src/ui/editTask/edittaskdialog.h"
#include "src/ui/setting/settingdialog.h"
#include "src/vision/ClassNameCache.h"
#include "src/vision/ImageIo.h"
#include "src/vision/YOLODetector.h"

namespace {

// 「共用实例」提醒：只告知、不拦截。
// 按钮只有一个「我知道了」，不改变用户已经发起的操作结果。
void warnSharedInstance(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::NoButton, parent);
    auto* ok = box.addButton(QStringLiteral("我知道了"), QMessageBox::AcceptRole);
    box.setDefaultButton(ok);
    box.exec();
}

// 鬼王列表配置是「系统方案-超鬼王」系列独有的功能，方案 id 直接写死在程序里：
// 只有下列方案显示「阵容配置」入口，其余系统方案一律不显示
constexpr const char* kSuperGhostKingConfigIds[] = {
    "627CE2C8-01C9-4828-B4AE-4F5A360E7946", // 系统方案-超鬼王
    "7C0CE007-30B5-4560-92B9-1892E13A8C26"  // 系统方案-超鬼王(协同)
};

// 是否显示该方案的「阵容配置」按钮：先按 id 命中任一超鬼王方案，
// 再看方案对象的 showBossConfig 字段是否有意关掉（缺省视为显示）
bool canShowBossConfigButton(const QString &configId)
{
    bool matched = false;
    for (const char *id : kSuperGhostKingConfigIds) {
        if (configId.compare(QString::fromLatin1(id), Qt::CaseInsensitive) == 0) {
            matched = true;
            break;
        }
    }
    if (!matched) {
        return false;
    }
    for (const QJsonValue &value : m_configArray) {
        const QJsonObject obj = value.toObject();
        if (obj.value("id").toString().compare(configId, Qt::CaseInsensitive) == 0) {
            return obj.value("showBossConfig").toBool(true);
        }
    }
    return true;
}

// 系统方案表单里的数字输入框：滚轮不再改数值。
// QSpinBox 默认在获得焦点后响应滚轮，鼠标划过或滚动页面时很容易误改，
// 这里直接吞掉滚轮事件（ignore 后由父级滚动区接管，页面照常滚动）
class NoWheelSpinBox : public QSpinBox
{
public:
    explicit NoWheelSpinBox(QWidget *parent = nullptr) : QSpinBox(parent) {}

protected:
    void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};

class NoWheelDoubleSpinBox : public QDoubleSpinBox
{
public:
    explicit NoWheelDoubleSpinBox(QWidget *parent = nullptr) : QDoubleSpinBox(parent) {}

protected:
    void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};

// 下拉框同理：鼠标划过滚一下就换选项，且没有任何提示，这里一并吞掉滚轮。
// 弹出列表打开时的滚动由列表自己处理，不受影响
class NoWheelComboBox : public QComboBox
{
public:
    explicit NoWheelComboBox(QWidget *parent = nullptr) : QComboBox(parent) {}

protected:
    void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};

} // namespace

sessionwidget::sessionwidget(QWidget *parent) :
    QWidget(parent), ui(new Ui::sessionwidget) {
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("onmyoji-script v") + APP_VERSION);

    // 配合全局 QSS 的 alternate-background-color 显示斑马纹
    ui->listWidget->setAlternatingRowColors(true);
    ui->tableWidget->setAlternatingRowColors(true);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setShowGrid(false);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->openCVIdentifyLabel->setStyleSheet(
        "border: 1px dashed #c0c4d0; border-radius: 6px; background-color: #ffffff;");
    ui->settingButton->raise(); // 确保不被同层其他控件盖住

    // 日志回调由 shell 统一注册并分发到对应标签页：
    // Logger 只支持一个 sink，多个 Session 各自 setSink 会互相覆盖。

    // 日志框上限：超出自动丢弃最旧内容，防止长时间挂机内存无限增长
    ui->plainTextEdit->setMaximumBlockCount(2000);

    // 绑定选中信号
    connect(ui->listWidget, &QListWidget::itemClicked,
            this, &sessionwidget::onItemClicked);

    //绑定开启任务按钮
    connect(ui->startTaskButton, &QToolButton::clicked,
            this, &sessionwidget::startTaskButtonClick);

    //绑定关闭任务按钮
    connect(ui->stopTaskButton, &QToolButton::clicked,
            this, &sessionwidget::stopTaskButtonClick);

    //绑定添加方案按钮
    connect(ui->programmeAddBtn, &QToolButton::clicked,
            this, &sessionwidget::onProgrammeAddBtnClicked);

    //绑定删除方案按钮
    connect(ui->programmeRemoveBtn, &QToolButton::clicked,
            this, &sessionwidget::onProgrammeRemoveBtnClicked);

    //绑定添加方案内容按钮
    connect(ui->programmeContentAddBtn, &QToolButton::clicked,
            this, &sessionwidget::onProgrammeContentAddBtnClicked);

    //绑定打开设置按钮
    connect(ui->settingButton, &QToolButton::clicked,
            this, &sessionwidget::onSettingBtnClicked);

    //绑定选择游戏窗口按钮
    connect(ui->bindWindowButton, &QToolButton::clicked,
            this, &sessionwidget::onBindWindowBtnClicked);

    //绑定缩略图右上角的「获取截图」按钮
    connect(ui->captureShotButton, &QToolButton::clicked,
            this, &sessionwidget::onCaptureShotBtnClicked);

    //绑定方案内容上移按钮
    connect(ui->programmeContentUpButton, &QToolButton::clicked,
            this, &sessionwidget::onProgrammeUpBtnClicked);

    //绑定方案内容下移按钮
    connect(ui->programmeContentDownButton, &QToolButton::clicked,
            this, &sessionwidget::onProgrammeDownBtnClicked);

    //监听编辑方案名称
    connect(ui->listWidget, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (m_reloadingProfiles || !item) {
            return;
        }

        // 用条目自己携带的 id，而不是全局 currentItem：
        // currentItem 是「最后一次点击」，多标签页下会把别的标签页选中的方案改名
        const QString configId = item->data(Qt::UserRole).toString();
        const QString newName = item->text().trimmed();
        if (configId.isEmpty() || newName.isEmpty()) {
            return;
        }

        // 不能在 itemChanged 的调用栈里重建列表：clear() 会析构正在发这条信号的
        // 条目，后续访问即悬空。延后一拍写文件并重建，顺带避免重入
        QTimer::singleShot(0, this, [this, configId, newName]() {
            refreshConfig();
            updateProgrammeContent(AppPaths::instance().configPath(), configId, newName);

            refreshConfig();
            rebuildProgrammeList();
            focusProgrammeInList(configId);

            // 改名是全局改动，其他标签页的列表也要跟着变
            emit profilesChanged(configId);
        });
    });

    //读取setting的配置
    if (SETTING_CONFIG.loadConfig())
    {
        Logger::log(QString("Setting配置加载成功！"));
    }

    //读取classes.txt的配置
    QString classesPath = AppPaths::instance().classesNamePath();
    if (ClassNameCache::initialize(classesPath))
    {
        Logger::log(QString("classes.txt配置加载成功！"));
    }

    updateBindStateLabel();

    //读取方案的配置
    loadListWidgetData();

    Logger::log(QString("Config配置加载成功！"));

    // 检查OpenCV版本和编译选项
    Logger::log(QString("OpenCV 版本 %1").arg(CV_VERSION));

    Logger::log(QString("OCR 引擎: PP-OCRv6（OcrLite 推理）"));

    // 模型加载是进程级一次性工作，移到 shell 执行：
    // 每个标签页都 initialize 一次会重复占用几十 MB 内存。

    ui->taskCycleNumber->setValue(1);

    // 每个会话持有独立的执行器：任务跑在自己的 worker 线程，
    // 信号只连到本标签页，天然隔离，不再需要「运行归属者」这种全局过滤。
    // 日志标识用对象地址，保证同一进程内唯一且稳定
    m_logTag = QStringLiteral("session-%1")
                   .arg(reinterpret_cast<quintptr>(this), 0, 16);
    m_runner = new TaskRunner();

    connect(m_runner, &TaskRunner::showImage, this,
            [this](const QString& path) { showOpenCVIdentifyImage(path); });

    connect(m_runner, &TaskRunner::showImageData, this,
            [this](const QString& key, const cv::Mat& image) {
                showOpenCVIdentifyImage(key, image);
            });

    connect(m_runner, &TaskRunner::started, this, [this]() {
        setTaskRunningState(true);
        emit taskRunningChanged(true);
        emit titleChanged();
    });

    connect(m_runner, &TaskRunner::finished, this, [this]() {
        setTaskRunningState(false);
        emit taskRunningChanged(false);
        emit titleChanged();
        emit taskFinished();
    });

    connect(m_runner, &TaskRunner::cycleProgress, this,
            [this](int completed, int total) { showCycleProgress(completed, total); });

    // UI 线程内的动作层信号：编辑器里手动测试步骤时结果图直接回显到本标签页。
    // 任务线程有各自独立的 ScriptActions 实例，不会触发这里。
    connect(&ScriptActions::instance(), &ScriptActions::requestShowImage,
            this, [this](const QString& path) { showOpenCVIdentifyImage(path); });

    //设置DPI感知
    DPIHelper::SetProcessDPIAwareness();
}

sessionwidget::~sessionwidget() {
    // 先停止任务并等待线程真正退出，再销毁界面对象：
    // 执行线程会通过信号回写界面，ui 先析构会让它在下一刻访问悬空指针。
    if (m_runner) {
        m_runner->stop();
        delete m_runner;   // 析构内部会 wait 线程结束
        m_runner = nullptr;
    }
    delete ui;
}

QString sessionwidget::tabTitle() const
{
    const QString base = (m_boundPid != 0)
                             ? QStringLiteral("实例 %1").arg(m_boundPid)
                             : QStringLiteral("未绑定");

    // 与其他标签页共用同一实例时给出角标：允许共用，但必须一眼可见
    //
    // 「运行中」不再拼进标题文字：QTabBar 的标签文字颜色由 QSS 统一控制，
    // 想让某个标签的「▶」单独变绿只能上自定义 delegate。改为绿色 tab icon，
    // 由 mainwindow::refreshTabTitle 按 isTaskRunning() 设置/清除
    return m_sharedPeers.isEmpty() ? base
                                   : base + QStringLiteral(" ⚠共用");
}

QString sessionwidget::displayName() const
{
    if (!m_displayName.isEmpty()) {
        return m_displayName;
    }
    // 不用 tabTitle()：标题里带运行/共用角标，写进日志或提示框会重复且易误导
    return (m_boundPid != 0) ? QStringLiteral("实例 %1").arg(m_boundPid)
                             : QStringLiteral("未绑定");
}

void sessionwidget::bindInstance(DWORD pid, HWND hwnd, const QString& displayName)
{
    m_boundPid = pid;
    m_boundHwnd = hwnd;
    m_displayName = displayName;

    // 绑定动作本身就是一次显式操作：重置提醒标记，
    // 下次启动任务时若仍与他人共用会再提醒一次
    m_warnedSharedPid = 0;

    updateBindStateLabel();
    emit titleChanged();
    emit bindingChanged();
}

void sessionwidget::setPeerLookup(std::function<QStringList(DWORD)> lookup)
{
    m_peerLookup = std::move(lookup);
}

void sessionwidget::setSingleSessionProbe(std::function<bool()> probe)
{
    m_singleSessionProbe = std::move(probe);
}

bool sessionwidget::isOnlySession() const
{
    // 没注入就当作「不是唯一」：多标签页下擅自选实例比让用户点一下更糟
    return m_singleSessionProbe && m_singleSessionProbe();
}

bool sessionwidget::autoBindFirstInstance()
{
    const std::vector<DWORD> pids = platform::findAllPidsByProcessName("onmyoji.exe");
    if (pids.empty()) {
        Logger::log(QStringLiteral("未找到运行中的阴阳师进程（onmyoji.exe），请先启动游戏"));
        return false;
    }

    for (DWORD pid : pids) {
        HWND hwnd = platform::findWindowByPid(pid);
        if (!hwnd) {
            continue;
        }

        const std::wstring title = platform::getWindowTitle(hwnd);
        bindInstance(pid, hwnd,
                     title.empty() ? QStringLiteral("onmyoji.exe")
                                   : QString::fromWCharArray(title.c_str()));
        activateBinding();

        Logger::log(QStringLiteral("未手动绑定窗口，已自动使用第一个游戏实例（PID %1）").arg(pid));
        return true;
    }

    Logger::log(QStringLiteral("找到阴阳师进程但没有可用游戏窗口，请点击「绑定窗口」手动选择"));
    return false;
}

void sessionwidget::setSharedPeers(const QStringList& peers)
{
    // 冲突解除后清掉已提醒标记：下次再被别人绑同一实例时重新提醒
    if (peers.isEmpty()) {
        m_warnedSharedPid = 0;
    }

    if (m_sharedPeers == peers) {
        return;
    }

    m_sharedPeers = peers;
    updateBindStateLabel();
    emit titleChanged();
}

void sessionwidget::activateBinding()
{
    // 只影响 UI 线程的预览上下文（编辑器测试步骤走这里）。
    // 任务执行线程在 start() 时固定自己的上下文，不受本调用影响。
    GameWindow::instance().bindTo(m_boundPid, m_boundHwnd);

    // 恢复本标签页的方案选择到 UI 线程的全局上下文：
    // 编辑器里的增删改步骤默认作用于 currentItem，切换标签页后
    // 必须指回本页选中的方案，否则会改到上一个标签页的方案上
    if (!m_selectedItem.id.isEmpty()) {
        commonSetCurrentItem(m_selectedItem.id, m_selectedItem.taskName);
    }
}

void sessionwidget::setSystemConfigOverrides(const QJsonObject& overrides)
{
    m_systemConfigOverrides = overrides;

    // 恢复持久化值时，当前选中的系统方案表单还是按默认值建的，需要重建一次
    if (m_selectedItem.id.isEmpty()) {
        return;
    }
    for (const QJsonValue& val : m_configArray) {
        const QJsonObject obj = val.toObject();
        if (obj.value("id").toString() != m_selectedItem.id) {
            continue;
        }
        if (obj.value("type").toString() == QStringLiteral("system")) {
            showSystemConfigForm(m_selectedItem.id);
        }
        break;
    }
}

QJsonValue sessionwidget::effectiveSystemConfigValue(const QString& configId, const QString& key,
                                                     const QJsonValue& fallback) const
{
    const QJsonObject perConfig = m_systemConfigOverrides.value(configId).toObject();
    return perConfig.contains(key) ? perConfig.value(key) : fallback;
}

void sessionwidget::saveSystemConfigOverride(const QString& configId, const QString& key,
                                             const QJsonValue& value)
{
    QJsonObject perConfig = m_systemConfigOverrides.value(configId).toObject();
    perConfig[key] = value;
    m_systemConfigOverrides[configId] = perConfig;
    emit systemConfigOverridesChanged();
}

void sessionwidget::saveSystemConfigAsDefault(const QString& configId)
{
    // 用当前生效值（本页覆盖优先）生成要写回的默认值
    const QJsonArray systemConfig = getSystemConfig(configId);
    QJsonObject values;
    for (const QJsonValue& val : systemConfig) {
        const QJsonObject field = val.toObject();
        const QString key = field.value("key").toString();
        if (key.isEmpty()) {
            continue;
        }
        values[key] = effectiveSystemConfigValue(configId, key, field.value("value"));
    }
    if (values.isEmpty()) {
        return;
    }

    if (!updateSystemConfigValues(AppPaths::instance().configPath(), configId, values)) {
        Logger::log(QStringLiteral("保存方案默认值失败"));
        return;
    }

    // 默认值已经等于本页的值，清掉覆盖让本页回到「跟随默认」，
    // 避免以后默认值再变时本页还钉在旧值上
    m_systemConfigOverrides.remove(configId);
    emit systemConfigOverridesChanged();
    Logger::log(QStringLiteral("已将当前参数保存为方案默认值"));
    // 默认值写进了 config.json：新建/其他标签页读的是同一份，通知列表同步
    emit profilesChanged(configId);
}

void sessionwidget::resetSystemConfigToDefault(const QString& configId)
{
    if (!m_systemConfigOverrides.contains(configId)) {
        return;
    }

    m_systemConfigOverrides.remove(configId);
    emit systemConfigOverridesChanged();
    // 重建表单，控件值回到 config.json 里的默认值
    showSystemConfigForm(configId);
    Logger::log(QStringLiteral("已恢复方案默认值"));
}

void sessionwidget::applySystemConfigOverrides(QJsonArray& configs, const QString& configId) const
{
    const QJsonObject perConfig = m_systemConfigOverrides.value(configId).toObject();
    if (perConfig.isEmpty()) {
        return; // 没有个性化配置，直接用方案默认值
    }

    for (int i = 0; i < configs.size(); ++i) {
        QJsonObject obj = configs[i].toObject();
        if (obj.value("id").toString() != configId) {
            continue;
        }

        // 只改这份「本次运行」的副本：任务线程启动时整体拷走，
        // 其他标签页读的仍是各自的副本，config.json 完全不动
        QJsonArray systemConfig = obj.value("systemConfig").toArray();
        for (int j = 0; j < systemConfig.size(); ++j) {
            QJsonObject field = systemConfig[j].toObject();
            const QString key = field.value("key").toString();
            if (perConfig.contains(key)) {
                field["value"] = perConfig.value(key);
                systemConfig[j] = field;
            }
        }
        obj["systemConfig"] = systemConfig;
        configs[i] = obj;
        return;
    }
}

bool sessionwidget::isTaskRunning() const
{
    return m_runner != nullptr && m_runner->isRunning();
}

void sessionwidget::stopTask()
{
    if (m_runner) {
        m_runner->stop();
    }
}

GameContext sessionwidget::buildContext() const
{
    GameContext ctx;
    ctx.pid = m_boundPid;
    ctx.hwnd = m_boundHwnd;
    ctx.processName = "onmyoji.exe";
    ctx.logTag = m_logTag;
    ctx.displayName = displayName();
    return ctx;
}

// 卸载 hook DLL 与关窗拦截已上移到 shell：
// 一个进程里只有一个游戏上下文，退出时应统一处理所有已绑定的实例，
// 单个标签页不具备这个视角。

void sessionwidget::loadListWidgetData()
{
    refreshConfig();
    rebuildProgrammeList();
    // 首次加载/增删方案后统一落到第一个可见方案（与原行为一致）。
    // 需要在重建后保留本页选择的场景走 reloadProfiles()
    selectFirstVisibleProgramme();
}

// 重建方案列表条目。只做「条目与 m_configArray 对齐」这件事，
// 不改变本页选中的方案——多标签页下，别的标签页改配置不应该带走本页的选择。
void sessionwidget::rebuildProgrammeList()
{
    // 重建期间屏蔽 itemChanged：clear()/addItem 过程中的条目变化不是用户改名，
    // 若被当成改名写回，会把别的标签页正在编辑的方案覆盖掉
    const QSignalBlocker blocker(ui->listWidget);
    ui->listWidget->clear();

    for (const QJsonValue &val : m_configArray) {
        const QJsonObject obj = val.toObject();

        // 显隐字段过滤：visible 为 false 的方案不在列表中显示；
        // 缺省视为 true，兼容无该字段的旧配置
        if (!obj.value("visible").toBool(true)) {
            continue;
        }

        auto *item = new QListWidgetItem(obj.value("name").toString(), ui->listWidget);
        // 绑定额外数据（Qt::UserRole 是给用户自定义数据用的）
        item->setData(Qt::UserRole, obj.value("id").toString());
        item->setFlags(item->flags() | Qt::ItemIsEditable); // 添加可编辑标志
    }
}

QJsonObject sessionwidget::findConfigById(const QString& configId) const
{
    if (configId.isEmpty()) {
        return QJsonObject();
    }
    for (const QJsonValue &val : m_configArray) {
        if (!val.isObject()) {
            continue;
        }
        const QJsonObject obj = val.toObject();
        if (obj.value("id").toString() == configId) {
            return obj;
        }
    }
    return QJsonObject();
}

// 在列表中定位并选中指定方案，同时把本页的选中状态写到 m_selectedItem。
// 注意：全局 currentItem 是所有会话共用的「最后一次点击」，
// 启动任务与所有写文件的地方都必须以 m_selectedItem 为准。
bool sessionwidget::focusProgrammeInList(const QString& configId)
{
    const QJsonObject obj = findConfigById(configId);
    if (obj.isEmpty() || !obj.value("visible").toBool(true)) {
        return false;
    }

    {
        // 程序化改选中项不应触发任何用户编辑语义
        const QSignalBlocker blocker(ui->listWidget);
        for (int row = 0; row < ui->listWidget->count(); ++row) {
            QListWidgetItem *item = ui->listWidget->item(row);
            if (item && item->data(Qt::UserRole).toString() == configId) {
                ui->listWidget->setCurrentItem(item);
                break;
            }
        }
    }

    m_selectedItem.id = configId;
    m_selectedItem.taskName = obj.value("name").toString();
    commonSetCurrentItem(m_selectedItem.id, m_selectedItem.taskName);
    ui->currentTaskName->setText(m_selectedItem.taskName);
    return true;
}

void sessionwidget::selectFirstVisibleProgramme()
{
    for (const QJsonValue &val : m_configArray) {
        const QJsonObject obj = val.toObject();
        if (!obj.value("visible").toBool(true)) {
            continue;
        }
        if (focusProgrammeInList(obj.value("id").toString())) {
            refreshSelectedProgrammeContent();
            return;
        }
    }

    // 所有方案均隐藏/为空：清空选中状态与方案内容区
    m_selectedItem = ItemInfo{};
    commonSetCurrentItem(QString(), QString());
    ui->currentTaskName->setText(QStringLiteral("（无方案）"));
    if (m_systemConfigForm) {
        m_systemConfigForm->hide();
    }
    ui->tableWidget->clear();
    ui->tableWidget->setRowCount(0);
    ui->tableWidget->setColumnCount(0);
}

// 按本页选择重建内容区。系统方案展示配置表单，普通方案展示步骤表格
void sessionwidget::refreshSelectedProgrammeContent()
{
    const QJsonObject obj = findConfigById(m_selectedItem.id);
    if (obj.isEmpty()) {
        return;
    }

    if (obj.value("type").toString() == QStringLiteral("system")) {
        ui->tableWidget->clear();
        ui->tableWidget->setRowCount(0);
        ui->tableWidget->setColumnCount(0);
        ui->programmeContentAddBtn->setDisabled(true);
        // 按该系统方案的 systemConfig 生成可编辑表单(无自定义配置则退回原提示)
        showSystemConfigForm(m_selectedItem.id);
        return;
    }

    ui->programmeContentAddBtn->setDisabled(false);
    showStepsInTable(obj.value("steps").toArray());
}

// 其他标签页改了 config.json：重建列表，但保留本页自己的选择。
// 只重建「本页正在看的方案」的内容区，别的方案被改动时不打断本页操作。
void sessionwidget::reloadProfiles(const QString& changedConfigId)
{
    if (m_reloadingProfiles) {
        return;
    }
    QScopedValueRollback<bool> guard(m_reloadingProfiles, true);

    const QString selectedId = m_selectedItem.id;

    refreshConfig();
    rebuildProgrammeList();

    if (!selectedId.isEmpty() && focusProgrammeInList(selectedId)) {
        if (changedConfigId.isEmpty() || changedConfigId == selectedId) {
            refreshSelectedProgrammeContent();
        }
        return;
    }

    // 本页选中的方案已被删除/隐藏/被导入替换：退回第一个可见方案
    selectFirstVisibleProgramme();
}

// 点击了方案列表
void sessionwidget::onItemClicked(QListWidgetItem *item) {
    if (!item) return;

    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    // 兜底：列表可能是旧的（别的标签页刚改过 config.json）。
    // 点到的方案在内存里找不到时补读一次文件再重试，避免「点了没反应」
    if (findConfigById(id).isEmpty()) {
        refreshConfig();
        rebuildProgrammeList();
    }

    if (!focusProgrammeInList(id)) {
        Logger::log(QStringLiteral("该方案已不存在，方案列表已同步为最新配置"));
        selectFirstVisibleProgramme();
        return;
    }

    refreshSelectedProgrammeContent();
}

// 按系统方案的 systemConfig 描述动态生成可编辑表单。
// 控件变更只记在本标签页的覆盖值里（随 windows.json 持久化），不写 config.json：
// 同一方案被多个标签页共用时，各实例的运行参数互不覆盖
void sessionwidget::showSystemConfigForm(const QString &configId)
{
    // 每次销毁并重建容器：避免复用旧 QFormLayout 清理时的 takeAt 告警
    if (m_systemConfigForm) {
        m_systemConfigForm->deleteLater();
        m_systemConfigForm = nullptr;
    }

    // systemConfig 为空(如十周年999)时同样生成表单：显示标题与方案级 tip，只是没有配置项。
    // 不再用 systemConfigTips 静态提示兜底——它被 raise 后的 tableWidget 盖住，永远不可见。
    const QJsonArray systemConfig = getSystemConfig(configId);
    ui->systemConfigTips->hide();

    // 表单容器，位置对齐任务表格区域
    m_systemConfigForm = new QWidget(ui->tableWidget->parentWidget());
    m_systemConfigForm->setGeometry(ui->tableWidget->geometry());
    // 边框 + 背景色，营造独立配置区域的分隔感（用 #objectName 限定，避免影响内部控件）
    m_systemConfigForm->setObjectName("systemConfigForm");
    m_systemConfigForm->setStyleSheet(
        "#systemConfigForm {"
        "  background-color: #f7f9fc;"
        "  border: 1px solid #d0d7e2;"
        "  border-radius: 8px;"
        "}"
        // 配置项行容器默认会被全局 QWidget 底色覆盖，单独置为透明，露出卡片背景
        "#systemConfigField { background: transparent; }"
        "#systemConfigHeader { background: transparent; }");

    // 配置项超出容器高度时出现纵向滚动条，内容不足时不显示；
    // 横向滚动条关闭，避免表单被横向拖动、配置项按可视宽度自适应
    auto *scrollArea = new QScrollArea(m_systemConfigForm);
    scrollArea->setObjectName("systemConfigScroll");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 全局 QSS 给所有 QWidget 设了底色，会让 viewport 盖住卡片边框和背景，
    // 这里单独把 viewport 命名为透明，保证卡片右侧/下侧边框始终可见
    scrollArea->viewport()->setObjectName("systemConfigViewport");
    scrollArea->viewport()->setAutoFillBackground(false);
    scrollArea->setStyleSheet(
        "#systemConfigScroll { background: transparent; }"
        "#systemConfigViewport { background: transparent; }");

    auto *formContainer = new QWidget(scrollArea);
    formContainer->setObjectName("systemConfigContent");
    formContainer->setStyleSheet("#systemConfigContent { background: transparent; }");

    auto *form = new QFormLayout(formContainer);
    form->setContentsMargins(16, 16, 16, 16);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 区域标题移到卡片顶部固定行（见下方 rootLayout），此处不再占表单一行

    // 方案级 tip：整个方案的功能使用说明，显示在标题下方
    const QString systemTip = getSystemTip(configId);
    if (!systemTip.isEmpty()) {
        auto *tipLabel = new QLabel(systemTip, m_systemConfigForm);
        tipLabel->setWordWrap(true);
        tipLabel->setStyleSheet(
            "color: #6b7280; font-size: 9pt; background-color: #eef2f7;"
            "border: 1px solid #d0d7e2; border-radius: 6px; padding: 8px;");
        form->addRow(tipLabel);
    }

    // 「阵容配置」入口：与 systemConfig 其他项一样是 label + 控件的一行。
    // 只在超鬼王方案出现，且可由方案对象的 showBossConfig 关掉；
    // 弹窗里配的是「鬼王名称 → 阵容/绿标/红标」列表，随 config.json 持久化
    if (canShowBossConfigButton(configId)) {
        auto *entryRow = new QWidget(m_systemConfigForm);
        entryRow->setObjectName("systemConfigField");
        auto *entryLayout = new QHBoxLayout(entryRow);
        entryLayout->setContentsMargins(0, 0, 0, 0);
        entryLayout->setSpacing(8);

        auto *editBtn = new QPushButton(QStringLiteral("编辑"), entryRow);
        editBtn->setCursor(Qt::PointingHandCursor);
        editBtn->setToolTip(QStringLiteral("配置鬼王对应的阵容、绿标位置与红标 BOSS"));
        entryLayout->addWidget(editBtn);
        entryLayout->addStretch(1);

        connect(editBtn, &QPushButton::clicked, this, [this, configId]() {
            BossConfigDialog dialog(m_selectedItem.taskName, getBossConfig(configId), this);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            updateBossConfig(AppPaths::instance().configPath(), configId, dialog.list());
            // 鬼王列表存在 config.json 里，属于全局数据：其他标签页同步刷新
            emit profilesChanged(configId);
        });

        form->addRow(QStringLiteral("阵容配置"), entryRow);
    }

    for (const QJsonValue &val : systemConfig) {
        const QJsonObject field = val.toObject();
        const QString key = field["key"].toString();
        const QString label = field["label"].toString();
        const QString control = field["control"].toString();
        const QString itemTip = field.value("tip").toString();
        // 显示本标签页的值：有覆盖用覆盖，没有则用方案默认值(config.json)
        const QJsonValue currentValue =
            effectiveSystemConfigValue(configId, key, field.value("value"));

        // 每个配置项用一个垂直容器承载「控件 + 可选 tip 提示」，便于统一加入表单
        auto *fieldWidget = new QWidget(m_systemConfigForm);
        fieldWidget->setObjectName("systemConfigField");
        auto *fieldLayout = new QVBoxLayout(fieldWidget);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setSpacing(4);

        if (control == "number") {
            if (field.value("decimals").toInt(0) <= 0) {
                // 整数：QSpinBox
                auto *spin = new NoWheelSpinBox(fieldWidget);
                spin->setRange(field.value("min").toInt(0), field.value("max").toInt(100));
                spin->setSingleStep(field.value("step").toInt(1));
                spin->setValue(currentValue.toInt());
                connect(spin, qOverload<int>(&QSpinBox::valueChanged), this,
                        [=](int v) { saveSystemConfigOverride(configId, key, v); });
                fieldLayout->addWidget(spin);
            } else {
                // 小数：QDoubleSpinBox
                auto *spin = new NoWheelDoubleSpinBox(fieldWidget);
                spin->setDecimals(field.value("decimals").toInt(2));
                spin->setRange(field.value("min").toDouble(0.0), field.value("max").toDouble(1.0));
                spin->setSingleStep(field.value("step").toDouble(0.1));
                spin->setValue(currentValue.toDouble());
                connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                        [=](double v) { saveSystemConfigOverride(configId, key, v); });
                fieldLayout->addWidget(spin);
            }
        } else if (control == "switch") {
            // 开关：QCheckBox
            auto *check = new QCheckBox(fieldWidget);
            check->setChecked(currentValue.toBool());
            connect(check, &QCheckBox::toggled, this,
                    [=](bool v) { saveSystemConfigOverride(configId, key, v); });
            fieldLayout->addWidget(check);
        } else if (control == "multiSelect") {
            // 多选：一组 QCheckBox，值存为数组（如 [1,2,3]）
            auto *boxWidget = new QWidget(fieldWidget);
            auto *boxLayout = new QHBoxLayout(boxWidget);
            boxLayout->setContentsMargins(0, 0, 0, 0);
            boxLayout->setSpacing(8);

            const QJsonArray options = field.value("options").toArray();
            const QJsonArray selected = currentValue.toArray();
            QList<QCheckBox*> boxes;
            QList<QJsonValue> optionValues;
            for (const QJsonValue &opt : options) {
                auto *check = new QCheckBox(
                    opt.isDouble() ? QStringLiteral("%1星").arg(opt.toInt()) : opt.toString(),
                    boxWidget);
                bool checked = false;
                for (const QJsonValue &sel : selected) {
                    if (sel == opt) {
                        checked = true;
                        break;
                    }
                }
                check->setChecked(checked);
                boxLayout->addWidget(check);
                boxes.append(check);
                optionValues.append(opt);
            }
            boxLayout->addStretch(1);

            // 收集当前勾选项；表单重建后旧控件销毁，旧 lambda 不会再生效
            const auto collectChecked = [boxes, optionValues]() {
                QJsonArray values;
                for (int i = 0; i < boxes.size(); ++i) {
                    if (boxes.at(i)->isChecked()) {
                        values.append(optionValues.at(i));
                    }
                }
                return values;
            };
            for (QCheckBox *check : boxes) {
                connect(check, &QCheckBox::toggled, this,
                        [this, configId, key, collectChecked](bool) {
                            saveSystemConfigOverride(configId, key,
                                                     QJsonValue(collectChecked()));
                        });
            }
            fieldLayout->addWidget(boxWidget);
        } else if (control == "select") {
            // 下拉：QComboBox
            auto *combo = new NoWheelComboBox(fieldWidget);
            for (const QJsonValue &opt : field.value("options").toArray()) {
                combo->addItem(opt.toString());
            }
            combo->setCurrentText(currentValue.toString());
            connect(combo, &QComboBox::currentTextChanged, this,
                    [=](const QString &v) { saveSystemConfigOverride(configId, key, v); });
            fieldLayout->addWidget(combo);
        } else if (control == "input") {
            // 文本输入：QLineEdit
            auto *edit = new QLineEdit(fieldWidget);
            edit->setText(currentValue.toString());
            edit->setPlaceholderText(field.value("placeholder").toString());
            if (field.contains("maxLength")) {
                edit->setMaxLength(field.value("maxLength").toInt());
            }
            connect(edit, &QLineEdit::textChanged, this,
                    [=](const QString &v) { saveSystemConfigOverride(configId, key, v); });
            fieldLayout->addWidget(edit);
        }

        // 配置项级 tip：单个选项的解释说明，显示在控件下方
        if (!itemTip.isEmpty()) {
            auto *itemTipLabel = new QLabel(itemTip, fieldWidget);
            itemTipLabel->setWordWrap(true);
            itemTipLabel->setStyleSheet("color: #9ca3af; font-size: 8pt;");
            fieldLayout->addWidget(itemTipLabel);
        }

        form->addRow(label, fieldWidget);
    }

    scrollArea->setWidget(formContainer);

    // 标题行固定贴在卡片顶部（不随表单内容滚动）；方案自己的入口按钮
    // 放在下面的表单里，与 systemConfig 其他配置项保持同一种「label + 控件」形态
    auto *header = new QWidget(m_systemConfigForm);
    header->setObjectName("systemConfigHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("方案配置"), header);
    title->setStyleSheet("font-weight: bold; font-size: 12pt; color: #333;");
    headerLayout->addWidget(title);
    headerLayout->addStretch(1);

    // 「存为默认值」：系统方案参数平时只改本标签页的覆盖值，
    // 想把当前这套参数变成新标签页的起点时，点这里写回 config.json
    auto *resetBtn = new QPushButton(QStringLiteral("恢复默认"), header);
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setToolTip(QStringLiteral("丢弃本标签页的参数修改，回到方案默认值"));
    connect(resetBtn, &QPushButton::clicked, this,
            [this, configId]() { resetSystemConfigToDefault(configId); });
    headerLayout->addWidget(resetBtn);

    auto *saveDefaultBtn = new QPushButton(QStringLiteral("存为默认值"), header);
    saveDefaultBtn->setCursor(Qt::PointingHandCursor);
    saveDefaultBtn->setToolTip(
        QStringLiteral("把本标签页当前参数写回方案默认值(config.json)，新建标签页将使用这套值"));
    connect(saveDefaultBtn, &QPushButton::clicked, this,
            [this, configId]() { saveSystemConfigAsDefault(configId); });
    headerLayout->addWidget(saveDefaultBtn);

    auto *rootLayout = new QVBoxLayout(m_systemConfigForm);
    // 留出内边距，避免滚动区/内容贴边盖住卡片右侧与下侧边框
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(6);
    rootLayout->addWidget(header);
    rootLayout->addWidget(scrollArea, 1);

    m_systemConfigForm->show();
    m_systemConfigForm->raise();
}

//加载数据到任务表格
void sessionwidget::showStepsInTable(const QJsonArray &steps) {
    ui->systemConfigTips->hide();
    if (m_systemConfigForm) {
        m_systemConfigForm->hide();
    }
    ui->tableWidget->raise();
    ui->tableWidget->clear(); // 清空表格
    ui->tableWidget->setRowCount(steps.size());
    ui->tableWidget->setColumnCount(5); // 新增序号列
    QStringList headers = {"序号", "任务名称", "类型", "操作",""};
    ui->tableWidget->setHorizontalHeaderLabels(headers);

    // 单元格按钮外包一层容器留出边距，避免按钮铺满整格粘在一起
    auto wrapCellWidget = [](QWidget* inner) {
        auto* container = new QWidget();
        auto* layout = new QHBoxLayout(container);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->addWidget(inner);
        return container;
    };

    for (int i = 0; i < steps.size(); ++i) {
        QJsonObject step = steps[i].toObject();

        // 序号
        ui->tableWidget->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1))); // 从1开始

        // 任务名称
        QString taskName = step["taskName"].toString();
        ui->tableWidget->setItem(i, 1, new QTableWidgetItem(taskName));

        // 类型
        QString type = step["type"].toString();
        ui->tableWidget->setItem(i, 2, new QTableWidgetItem(type));

        // 编辑按钮
        QPushButton *editBtn = new QPushButton("编辑");
        ui->tableWidget->setCellWidget(i, 3, wrapCellWidget(editBtn));
        connect(editBtn, &QPushButton::clicked, this, [this, step]() {
            // 步骤属于本页选中的方案：配置 id 以 m_selectedItem 为准，
            // 不能用全局 currentItem（别的标签页切换方案后会指到别处）
            const QString configId = m_selectedItem.id;
            if (configId.isEmpty()) {
                return;
            }
            // 弹窗修改，或进入编辑模式
            EditTaskDialog* dlg = new EditTaskDialog(EditMode::Edit,step,configId,nullptr);
            dlg->show();
            connect(dlg, &EditTaskDialog::accepted, this, [this, dlg, configId]() {
                QJsonObject data = dlg->resultData();
                if (data.isEmpty()) {
                    return;
                }

                // 保存回 JSON 文件 & 刷新表格
                if (data["type"].toString() == "OPENCV") {
                    saveBase64ImageToFile(data);
                }
                // 以文件最新内容为基准，避免覆盖别的标签页刚做的改动
                refreshConfig();
                updateConfigInJsonFile(AppPaths::instance().configPath(), configId, data);
                QTimer::singleShot(0, dlg, &QObject::deleteLater); // 延迟一拍
                showCurrentSelectStepsInTable();
                emit profilesChanged(configId);
            });
            connect(dlg, &EditTaskDialog::imagePathRequested, this, [this](const QString& path) { showOpenCVIdentifyImage(path); });
        });

        // 删除按钮
        QPushButton *delBtn = new QPushButton("删除");
        ui->tableWidget->setCellWidget(i, 4, wrapCellWidget(delBtn));
        connect(delBtn, &QPushButton::clicked, this, [this, step]() {
            // 添加确认弹窗
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "确认删除",
                                        "确定要删除这个步骤吗？",
                                        QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                const QString configId = m_selectedItem.id;
                if (configId.isEmpty()) {
                    return;
                }
                // 以文件最新内容为基准，避免覆盖别的标签页刚做的改动
                refreshConfig();
                removeConfigById(AppPaths::instance().configPath(), configId, step["stepsId"].toString());
                showCurrentSelectStepsInTable();
                emit profilesChanged(configId);
            }
        });

    }

    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->resizeRowsToContents();
}

void sessionwidget::showCurrentSelectStepsInTable()
{
    refreshConfig();
    // 只看本页自己的选择：全局 currentItem 可能已被别的标签页改掉
    if (m_selectedItem.id.isEmpty())
    {
        Logger::log(QString("当前选择方案ID为空"));
        return;
    }

    if (findConfigById(m_selectedItem.id).isEmpty())
    {
        Logger::log("没有ConfigId: " + m_selectedItem.id);
        return;
    }

    refreshSelectedProgrammeContent();
}

//开启当前选中的任务
void sessionwidget::startTaskButtonClick()
{
    if (m_selectedItem.id.isEmpty())
    {
        Logger::log(QString("没有选中脚本方案"));
        return;
    }

    // 只限制本标签页自身：其他标签页在跑各自的任务是允许的
    if (m_runner->isRunning()) {
        Logger::log(QStringLiteral("当前标签页的任务已在运行中"));
        return;
    }

    if (m_boundPid == 0) {
        // 只有一个标签页时沿用旧逻辑：自动使用第一个游戏实例，不必手动绑定；
        // 多标签页时无法判断本页该操作哪一个实例，必须由用户显式选择
        if (!isOnlySession()) {
            Logger::log(QStringLiteral("请先点击「绑定窗口」选择本标签页要操作的游戏实例"));
            return;
        }
        if (!autoBindFirstInstance()) {
            return;
        }
    }

    // 共用实例提醒：不阻止启动（用户可能就是要这样对照操作），
    // 但每次共用关系只提示一次，避免每轮循环都弹窗
    if (!m_sharedPeers.isEmpty() && m_warnedSharedPid != m_boundPid) {
        m_warnedSharedPid = m_boundPid;
        warnSharedInstance(
            this, QStringLiteral("本标签页正在与其他标签页共用游戏实例"),
            QStringLiteral("以下标签页与本页绑定的是同一个游戏实例（PID %1）：\n    %2\n\n"
                           "同时运行任务会互相干扰（点击落在同一位置、流程互相打乱）。\n\n"
                           "任务照常启动。如果这不是本意，请先停止其中一个标签页。")
                .arg(m_boundPid)
                .arg(m_sharedPeers.join(QStringLiteral("\n    "))));

        Logger::log(QStringLiteral("⚠ 与其他标签页共用 PID %1（%2），同时运行任务可能互相干扰")
                        .arg(m_boundPid)
                        .arg(m_sharedPeers.join(QStringLiteral("、"))));
    }

    // 同步 UI 线程的预览上下文，编辑器里点「测试步骤」时作用于同一窗口
    activateBinding();

    QJsonObject obj = QJsonObject(); //默认为空
    //读取对应的方案步骤数据
    for (const QJsonValue &val : m_configArray) {
        if (!val.isObject()) {
            continue;
        }
        obj = val.toObject();
        if (obj["id"].toString() == m_selectedItem.id) {
            break;
        }
    }
    QJsonArray steps = obj["steps"].toArray();

    int cycleCount = ui->taskCycleNumber->text().toInt();

    // 系统方案参数是本标签页自己的设置：写进本次运行的配置副本，不动 config.json。
    // 多个标签页同时跑同一方案时，各自读到各自的值。
    QJsonArray runtimeConfigs = m_configArray;
    applySystemConfigOverrides(runtimeConfigs, m_selectedItem.id);

    // 异步启动：任务在独立线程执行，界面立刻恢复响应。
    // 方案选择与配置数组一并传入——任务线程写入自己的副本，
    // 之后别的标签页怎么切换方案都不影响本次运行。
    m_runner->start(steps, cycleCount, buildContext(), m_selectedItem, runtimeConfigs);
}

//关闭当前选中的任务
void sessionwidget::stopTaskButtonClick()
{
    if (!m_runner->isRunning()) {
        return;
    }

    // 停止是「协作式」的：只在当前步骤执行完后退出循环，
    // 因此按钮先反馈等待状态，避免用户重复点击或误以为没生效。
    ui->stopTaskButton->setText(QStringLiteral("正在停止..."));
    ui->stopTaskButton->setEnabled(false);
    m_runner->stop();
}

// 任务运行状态切换：控制按钮可用性与循环次数控件的显示形态
void sessionwidget::setTaskRunningState(bool running)
{
    ui->startTaskButton->setEnabled(!running);
    ui->stopTaskButton->setEnabled(running);
    ui->stopTaskButton->setText(QStringLiteral("停止"));

    if (running) {
        // 运行中：隐藏可编辑的循环次数输入框，用进度标签占位
        ui->label_5->setText(QStringLiteral("运行中"));
        ui->taskCycleNumber->hide();
        if (!m_cycleProgressLabel) {
            m_cycleProgressLabel = new QLabel(this);
            // 比输入框略宽：无限循环时需显示「(∞) 已执行 N 次」，右侧为空白区域不遮挡其他控件
            const QRect spinRect = ui->taskCycleNumber->geometry();
            m_cycleProgressLabel->setGeometry(spinRect.x(), spinRect.y(), 160, spinRect.height());
            // m_cycleProgressLabel->setStyleSheet(
            //     "border: 1px solid #c0c4d0; border-radius: 4px;"
            //     "background-color: #f7f9fc; padding-left: 4px;");
            m_cycleProgressLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_cycleProgressLabel->show();
            m_cycleProgressLabel->raise();
        }
    } else {
        // 停止/结束后：恢复默认的循环次数输入框
        ui->label_5->setText(QStringLiteral("循环次数"));
        if (m_cycleProgressLabel) {
            m_cycleProgressLabel->hide();
            m_cycleProgressLabel->deleteLater();
            m_cycleProgressLabel = nullptr;
        }
        ui->taskCycleNumber->show();
    }
}

// 显示循环进度：(已执行/总次数)；total <= 0 表示无限循环
void sessionwidget::showCycleProgress(int completed, int total)
{
    if (!m_cycleProgressLabel) {
        return;
    }
    if (total <= 0) {
        m_cycleProgressLabel->setText(QString("(∞) 已执行 %1 次").arg(completed));
    } else {
        m_cycleProgressLabel->setText(QString("(%1/%2)").arg(completed).arg(total));
    }
}

void sessionwidget::appendLogToUI(const QString &msg)
{
    if (!ui || !ui->plainTextEdit) return;

    // appendPlainText 本身会自动滚动到底并移动光标，无需再手动操作
    ui->plainTextEdit->appendPlainText(msg);
}

// 按钮点击槽函数
void sessionwidget::onProgrammeAddBtnClicked()
{
    bool ok;
    QString configName = QInputDialog::getText(
        this,
        tr("添加方案"),
        tr("请输入方案名称:"),
        QLineEdit::Normal,
        "",
        &ok
    );

    if (ok && !configName.isEmpty()) {
        // 以文件里的最新配置为基准：m_configArray 可能落后（别的标签页刚改过），
        // 直接拿旧数组覆盖写会把别人的改动抹掉
        refreshConfig();

        // 记录添加前的方案 id，添加后据此找出新方案并自动选中它，
        // 用户不必再去列表里找刚建的这一条
        QSet<QString> before;
        for (const QJsonValue &val : m_configArray) {
            before.insert(val.toObject().value("id").toString());
        }

        addConfigToJsonFile(AppPaths::instance().configPath(), configName);
        // 输入了有效内容
        Logger::log("已添加新的配置: " + configName);

        refreshConfig();
        rebuildProgrammeList();

        QString newId;
        for (const QJsonValue &val : m_configArray) {
            const QJsonObject obj = val.toObject();
            const QString id = obj.value("id").toString();
            if (!before.contains(id)) {
                newId = id;
                break;
            }
        }

        if (!newId.isEmpty() && focusProgrammeInList(newId)) {
            refreshSelectedProgrammeContent();
        } else {
            selectFirstVisibleProgramme();
        }

        // 方案列表变了：其他标签页必须重建，否则新方案只在当前标签页可见
        emit profilesChanged(newId);
    } else {
        // 用户取消或输入为空
        Logger::log(QString("用户未输入或无效的配置名称"));
    }
}

// 按钮点击槽函数
void sessionwidget::onProgrammeRemoveBtnClicked()
{
    if (m_selectedItem.id.isEmpty())
    {
        Logger::log(QString("未选中配置"));
        return;
    }
    // 弹出确认对话框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        this,
        tr("确认删除"),
        tr("确定要删除配置 “%1” 吗？").arg(m_selectedItem.taskName),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes)
    {
        // 以文件里的最新配置为基准，避免用旧数组覆盖掉别的标签页的改动
        refreshConfig();
        if (removeConfigById(AppPaths::instance().configPath(), m_selectedItem.id))
        {
            Logger::log("已删除配置: " + m_selectedItem.taskName);
            // 刷新列表，由 loadListWidgetData 重新选定第一个可见方案，
            // 保证界面选中项与 m_selectedItem 同步
            loadListWidgetData();
            // 方案列表变了：其他标签页同步重建
            emit profilesChanged(QString());
        }
        else
        {
            Logger::log("删除配置失败: " + m_selectedItem.taskName);
        }
    }
    else
    {
        Logger::log(QString("已取消删除"));
    }
}

void sessionwidget::onBindWindowBtnClicked()
{
    platform::PickedWindow picked;
    if (!BindWindowDialog::selectWindow(this, &picked)) {
        return;
    }

    // 空实例表示用户在对话框里点了「解除绑定」
    if (!picked.valid()) {
        bindInstance(0, nullptr);
        activateBinding();
        Logger::log(QStringLiteral("已解除本标签页的窗口绑定，恢复按进程名自动查找"));
        return;
    }

    // 点选可能落在非游戏窗口上（权限不足时无法按进程名过滤），先让用户确认
    if (!picked.imageName.isEmpty()
        && picked.imageName.compare(QStringLiteral("onmyoji.exe"), Qt::CaseInsensitive) != 0) {
        const auto reply = QMessageBox::question(
            this, QStringLiteral("确认绑定"),
            QStringLiteral("选中的窗口属于「%1」，不是阴阳师游戏窗口。仍要绑定吗？")
                .arg(picked.imageName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    // 共用实例提醒：同一游戏窗口已被别的标签页绑定时不拦截，但必须明确告知
    if (m_peerLookup) {
        const QStringList peers = m_peerLookup(picked.pid);
        if (!peers.isEmpty()) {
            warnSharedInstance(
                this, QStringLiteral("该窗口已被其他标签页绑定"),
                QStringLiteral("PID %1 已被下列标签页绑定：\n    %2\n\n"
                               "两个标签页操作同一个游戏实例时会争抢截图与点击，"
                               "同时运行任务会互相干扰（点击落在同一位置、流程互相打乱）。\n\n"
                               "绑定仍会生效。如非本意，建议改绑另一个游戏实例。")
                    .arg(picked.pid)
                    .arg(peers.join(QStringLiteral("\n    "))));

            Logger::log(QStringLiteral("⚠ PID %1 已由其他标签页绑定（%2），本标签页仍绑定成功，"
                                       "同时运行可能互相干扰")
                            .arg(picked.pid)
                            .arg(peers.join(QStringLiteral("、"))));
        }
    }

    bindInstance(picked.pid, picked.hwnd,
                 picked.title.isEmpty() ? picked.imageName : picked.title);

    // 绑定立即生效：后续截图/点击都作用到这个实例
    activateBinding();

    Logger::log(QStringLiteral("已绑定游戏窗口：%1（PID %2，客户区 %3×%4）")
                    .arg(displayName())
                    .arg(picked.pid)
                    .arg(picked.clientWidth)
                    .arg(picked.clientHeight));
}

// 缩略图右上角的「获取截图」：手动截一张本标签页绑定实例的画面。
// 多开时标签页标题上虽然有 PID，但看不出窗口里是哪个账号，
// 点一下就能确认这个标签页对应的到底是哪一个游戏窗口。
void sessionwidget::onCaptureShotBtnClicked()
{
    if (m_boundPid == 0) {
        // 唯一标签页沿用启动任务时的自动绑定；多标签页必须由用户显式选择，
        // 否则截到的是「第一个」实例，反而更容易看错
        if (!isOnlySession() || !autoBindFirstInstance()) {
            Logger::log(QStringLiteral("本标签页还没有绑定游戏窗口，请先点击「绑定窗口」"));
            QMessageBox::information(
                this, QStringLiteral("获取截图"),
                QStringLiteral("本标签页还没有绑定游戏窗口。\n\n"
                               "请先点击「绑定窗口」选择要操作的游戏实例，再获取截图。"));
            return;
        }
    }

    // 截图走 UI 线程的 GameWindow 上下文：先确认本页绑定的实例还活着，再指向本页的窗口。
    // 这里不用 locate()：它找不到绑定的实例时会回退成「按进程名找第一个」，
    // 那正好会截到别的实例——而截错实例正是这个按钮要避免的事
    HWND hwnd = IsWindow(m_boundHwnd) ? m_boundHwnd
                                      : platform::findWindowByPid(m_boundPid);
    if (!platform::isProcessAlive(m_boundPid) || hwnd == nullptr) {
        ui->openCVIdentifyLabel->clear();
        ui->openCVIdentifyLabel->setText(QStringLiteral("窗口已失效"));
        Logger::log(QStringLiteral("获取截图失败：绑定的游戏实例已失效（PID %1），请重新绑定")
                        .arg(m_boundPid));
        return;
    }
    GameWindow::instance().bindTo(m_boundPid, hwnd);

    // 截图本身要几百毫秒（含窗口尺寸校正），给个等待反馈，
    // 否则用户会以为按钮没响应而反复点
    QToolButton* button = ui->captureShotButton;
    button->setEnabled(false);
    button->setText(QStringLiteral("截图中..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const cv::Mat img = capture::captureGameWindow();

    QApplication::restoreOverrideCursor();
    button->setText(QStringLiteral("获取截图"));
    button->setEnabled(true);

    if (img.empty()) {
        ui->openCVIdentifyLabel->clear();
        ui->openCVIdentifyLabel->setText(QStringLiteral("截图失败"));
        Logger::log(QStringLiteral("获取截图失败：没有截到游戏画面，请确认游戏窗口是否还在、"
                                   "截图方式设置是否正确"));
        return;
    }

    Logger::log(QStringLiteral("已获取当前实例画面（%1×%2，PID %3）")
                    .arg(img.cols)
                    .arg(img.rows)
                    .arg(m_boundPid));

    // 传深拷贝：showOpenCVIdentifyImage 会就地把 BGR 转成 RGB
    showOpenCVIdentifyImage(QString(), img.clone());
}

void sessionwidget::updateBindStateLabel()
{
    if (!ui->bindStateLabel) {
        return;
    }

    // 「绑定窗口」按钮跟着绑定状态换一套视觉：未绑定时用醒目的琥珀色，
    // 提示「还差这一步才能开跑」；已绑定则退回白底描边的次级样式，
    // 不再和启动/停止抢视线（见 Theme.cpp 里 #bindWindowButton 的规则）
    const bool bound = (m_boundPid != 0);
    QToolButton* bindBtn = ui->bindWindowButton;
    bindBtn->setText(bound ? QStringLiteral("更换绑定") : QStringLiteral("绑定窗口"));
    bindBtn->setToolTip(
        bound ? QStringLiteral("当前绑定：%1（PID %2）\n点击可切换到其他游戏窗口")
                    .arg(displayName())
                    .arg(m_boundPid)
              : QStringLiteral("本标签页还没有绑定游戏实例，点击选择一个游戏窗口"));
    // QSS 用 [bound="true"] 区分两套配色，改完属性必须重新 polish 才会生效
    bindBtn->setProperty("bound", bound);
    bindBtn->style()->unpolish(bindBtn);
    bindBtn->style()->polish(bindBtn);

    if (m_boundPid == 0) {
        ui->bindStateLabel->setStyleSheet(QString());
        ui->bindStateLabel->setToolTip(QString());
        ui->bindStateLabel->setText(
            QStringLiteral("绑定窗口：未绑定（按进程名自动查找第一个实例）"));
        return;
    }

    // 共用实例：允许，但状态栏必须常驻可见警示。
    // 标签只有 346px 宽，长文案会被裁掉，因此警示前置、详细内容放 tooltip。
    if (!m_sharedPeers.isEmpty()) {
        ui->bindStateLabel->setStyleSheet(
            QStringLiteral("color: #c0392b; font-weight: bold;"));
        ui->bindStateLabel->setText(
            QStringLiteral("⚠ 与其他标签页共用实例（PID %1）").arg(m_boundPid));
        ui->bindStateLabel->setToolTip(
            QStringLiteral("绑定窗口：%1（PID %2）\n\n"
                           "与其他标签页共用同一个游戏实例：\n    %3\n\n"
                           "同时运行任务会互相干扰（点击落在同一位置、流程互相打乱）。")
                .arg(displayName())
                .arg(m_boundPid)
                .arg(m_sharedPeers.join(QStringLiteral("\n    "))));
        return;
    }

    ui->bindStateLabel->setStyleSheet(QString());
    ui->bindStateLabel->setText(
        QStringLiteral("绑定窗口：%1（PID %2）").arg(displayName()).arg(m_boundPid));
    ui->bindStateLabel->setToolTip(ui->bindStateLabel->text());
}

void sessionwidget::onSettingBtnClicked()
{
    SettingDialog dialog(this);
    // 设置内导入方案配置成功后，刷新主窗口的方案列表与表单
    connect(&dialog, &SettingDialog::configImported, this, [this]() {
        commonSetCurrentItem(QString(), QString());
        if (m_systemConfigForm) {
            m_systemConfigForm->deleteLater();
            m_systemConfigForm = nullptr;
        }
        loadListWidgetData();
        if (m_configArray.isEmpty()) {
            ui->tableWidget->clear();
            ui->tableWidget->setRowCount(0);
            ui->tableWidget->setColumnCount(0);
            ui->currentTaskName->setText(QStringLiteral("（无方案）"));
        }
        // 导入会整体替换 config.json：其他标签页也必须重建列表
        emit profilesChanged(QString());
    });
    if (dialog.exec() == QDialog::Accepted) {
        // 设置已保存和应用
        Logger::log(QString("Setting配置已修改"));
    }
}


void sessionwidget::onProgrammeContentAddBtnClicked()
{
    // 步骤加到本页选中的方案上：全局 currentItem 可能被别的标签页改掉
    const QString configId = m_selectedItem.id;
    if (configId.isEmpty()) {
        Logger::log(QStringLiteral("当前未选中方案"));
        return;
    }

    QJsonObject empty;
    EditTaskDialog* dlg = new EditTaskDialog(EditMode::Add,empty,configId,nullptr); // 非模态
    dlg->show();
    connect(dlg, &EditTaskDialog::accepted, this, [this, dlg, configId]() {
        QJsonObject data = dlg->resultData();
        // 保存 JSON
        if (data.isEmpty()) {
            return;
        }

        if (data["type"].toString() == "OPENCV") {
            saveBase64ImageToFile(data);
        }
        // 以文件最新内容为基准，避免覆盖别的标签页刚做的改动
        refreshConfig();
        addConfigToJsonFile(AppPaths::instance().configPath(),configId,data);
        QTimer::singleShot(0, dlg, &QObject::deleteLater);  // 延迟一拍
        showCurrentSelectStepsInTable();
        // 步骤变了：其他标签页若正看着这个方案，内容区要跟着刷新
        emit profilesChanged(configId);
    });
    connect(dlg, &EditTaskDialog::imagePathRequested, this, [this](const QString& path) { showOpenCVIdentifyImage(path); });
}

void sessionwidget::onProgrammeUpBtnClicked()
{
    int currentRow = ui->tableWidget->currentRow();
    if (currentRow >= 0) {
        refreshConfig();
        moveProgramme(AppPaths::instance().configPath(), m_selectedItem.id, currentRow, true);
        showCurrentSelectStepsInTable();
        ui->tableWidget->selectRow(currentRow-1);
        emit profilesChanged(m_selectedItem.id);
    } else {
        Logger::log(QString("没有选择方案内容的任何行"));
    }
}

void sessionwidget::onProgrammeDownBtnClicked()
{
    int currentRow = ui->tableWidget->currentRow();
    if (currentRow >= 0) {
        refreshConfig();
        moveProgramme(AppPaths::instance().configPath(), m_selectedItem.id, currentRow, false);
        showCurrentSelectStepsInTable();
        ui->tableWidget->selectRow(currentRow+1);
        emit profilesChanged(m_selectedItem.id);
    } else {
        Logger::log(QString("没有选择方案内容的任何行"));
    }
}

void sessionwidget::showOpenCVIdentifyImage(const QString& savePath,
                                            const cv::Mat& providedImage) const
{
    // 内存回显跨线程送达时直接使用随信号带来的图像：
    // 任务线程的 ScriptActions 实例与 UI 线程的不是同一个对象，
    // 这里再去调 instance().memoryResultImage() 只会拿到空图
    cv::Mat img = providedImage;

    if (img.empty() && savePath.isEmpty()) {
        qWarning() << "[WARN] showOpenCVIdentifyImage: 路径为空";
        ui->openCVIdentifyLabel->clear();
        ui->openCVIdentifyLabel->setText("无图像");
        return;
    }

    if (img.empty()) {
        // 本线程内的回显（编辑器测试步骤）：persistScreenshot 关闭时结果图
        // 只存在当前线程实例的缓存里，用内存 key 直接取
        if (savePath == ScriptActions::memoryResultKey()) {
            img = ScriptActions::instance().memoryResultImage();
        } else {
            img = vision::imreadQt(savePath);
        }
    }
    if (img.empty()) {
        qWarning() << "[ERROR] 无法加载图片:" << savePath;
        ui->openCVIdentifyLabel->clear();
        ui->openCVIdentifyLabel->setText("加载失败");
        return;
    }

    // 转换为 Qt 可识别格式（BGR → RGB）
    cv::cvtColor(img, img, cv::COLOR_BGR2RGB);

    // 封装成 QImage（不拷贝数据）
    QImage qimg(
        img.data,
        img.cols,
        img.rows,
        static_cast<int>(img.step),
        QImage::Format_RGB888
    );

    // 缩放显示：保持比例完整显示在 QLabel 内
    QSize labelSize = ui->openCVIdentifyLabel->size();
    QPixmap pixmap = QPixmap::fromImage(qimg).scaled(
        labelSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    // 设置显示
    ui->openCVIdentifyLabel->setPixmap(pixmap);
    ui->openCVIdentifyLabel->setAlignment(Qt::AlignCenter);
    ui->openCVIdentifyLabel->setScaledContents(false);  // 不拉伸变形
}
