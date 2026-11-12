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
#include <QTimer>
#include <QDebug>
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

// 鬼王列表配置是「系统方案-超鬼王」独有的功能，方案 id 直接写死在程序里：
// 只有它显示「自定义配置」入口，其余系统方案一律不显示
constexpr const char* kSuperGhostKingConfigId = "627CE2C8-01C9-4828-B4AE-4F5A360E7946";

// 是否显示该方案的「自定义配置」按钮：先按 id 命中超鬼王，
// 再看方案对象的 showBossConfig 字段是否有意关掉（缺省视为显示）
bool canShowBossConfigButton(const QString &configId)
{
    if (configId.compare(QString::fromLatin1(kSuperGhostKingConfigId),
                         Qt::CaseInsensitive) != 0) {
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

    //绑定方案内容上移按钮
    connect(ui->programmeContentUpButton, &QToolButton::clicked,
            this, &sessionwidget::onProgrammeUpBtnClicked);

    //绑定方案内容下移按钮
    connect(ui->programmeContentDownButton, &QToolButton::clicked,
            this, &sessionwidget::onProgrammeDownBtnClicked);

    //监听编辑方案名称
    connect(ui->listWidget, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        // 这里可以保存修改到配置文件等
        updateProgrammeContent(AppPaths::instance().configPath(), currentItem.id, item->text());
        loadListWidgetData();
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
    QString title = m_sharedPeers.isEmpty() ? base
                                            : base + QStringLiteral(" ⚠共用");

    // 多标签页可同时运行，标题上标出运行中的那个便于快速识别
    if (m_runner && m_runner->isRunning()) {
        return QStringLiteral("▶ ") + title;
    }
    return title;
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

    ui->listWidget->clear();

    QJsonArray arr = m_configArray;

    // 显隐字段过滤：visible 为 false 的方案不在列表中显示；
    // 缺省视为 true，兼容无该字段的旧配置
    QJsonArray visibleArr;
    for (int i = 0; i < arr.size(); i++) {
        if (arr[i].toObject()["visible"].toBool(true)) {
            visibleArr.append(arr[i]);
        }
    }

    for (int i = 0; i < visibleArr.size(); i++) {
        QJsonObject obj = visibleArr[i].toObject();

        QString name = obj["name"].toString();
        QString id   = obj["id"].toString();   // 假设 JSON 里有 id 字段

        // 创建列表项
        QListWidgetItem *item = new QListWidgetItem(name);

        // 绑定额外数据（Qt::UserRole 是给用户自定义数据用的）
        item->setData(Qt::UserRole, id);
        item->setFlags(item->flags() | Qt::ItemIsEditable); // 添加可编辑标志

        ui->listWidget->addItem(item);
    }

    if (visibleArr.size() > 0)
    {
        QJsonObject step = visibleArr[0].toObject();
        QString id = step["id"].toString();
        QString name = step["name"].toString();
        // 默认选中项必须同时写入本标签页的 m_selectedItem：
        // 全局 currentItem 是所有会话共用的，启动任务只看 m_selectedItem
        m_selectedItem.id = id;
        m_selectedItem.taskName = name;
        commonSetCurrentItem(id,name);
        ui->currentTaskName->setText(name);
        showStepsInTable(visibleArr[0].toObject()["steps"].toArray()); //默认显示第一个可见配置的方案内容
    }
    else
    {
        // 所有方案均隐藏：清空选中状态与方案内容表格
        m_selectedItem = ItemInfo{};
        commonSetCurrentItem(QString(), QString());
        ui->currentTaskName->setText(QStringLiteral("（无方案）"));
        ui->tableWidget->clear();
        ui->tableWidget->setRowCount(0);
        ui->tableWidget->setColumnCount(0);
    }
}

// 点击了方案列表
void sessionwidget::onItemClicked(QListWidgetItem *item) {
    if (!item) return;
    // 获取显示文本
    QString name = item->text();
    QString id   = item->data(Qt::UserRole).toString();
    // Logger::log(QString( "onItemClicked->选中项 name:" + name + ", id:" + id));

    // 记住本标签页的选择：全局 currentItem 是各会话共用的，
    // 别的标签页切换方案会把它改掉，启动任务时不能依赖它
    m_selectedItem.id = id;
    m_selectedItem.taskName = name;

    commonSetCurrentItem(id,name);
    ui->currentTaskName->setText(name);

    QJsonObject obj = QJsonObject(); //默认为空
    //读取对应的方案步骤数据到表格
    for (const QJsonValue &val : m_configArray) {
        if (!val.isObject()) {
            continue;
        }
        obj = val.toObject();
        if (obj["id"].toString() == id) {
            break;
        }
    }

    QJsonArray steps = obj["steps"].toArray();
    if (obj["type"] == "system")
    {
        ui->tableWidget->clear(); // 清空表格
        ui->tableWidget->setRowCount(0);
        ui->tableWidget->setColumnCount(0);
        ui->programmeContentAddBtn->setDisabled(true);
        // 按该系统方案的 systemConfig 生成可编辑表单(无自定义配置则退回原提示)
        showSystemConfigForm(id);
    }else
    {
        ui->programmeContentAddBtn->setDisabled(false);
        showStepsInTable(steps);
    }
}

// 按系统方案的 systemConfig 描述动态生成可编辑表单，控件变更即写回 config.json
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
            BossConfigDialog dialog(currentItem.taskName, getBossConfig(configId), this);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            updateBossConfig(AppPaths::instance().configPath(), configId, dialog.list());
        });

        form->addRow(QStringLiteral("阵容配置"), entryRow);
    }

    const QString configPath = AppPaths::instance().configPath();

    for (const QJsonValue &val : systemConfig) {
        const QJsonObject field = val.toObject();
        const QString key = field["key"].toString();
        const QString label = field["label"].toString();
        const QString control = field["control"].toString();
        const QString itemTip = field.value("tip").toString();

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
                spin->setValue(field.value("value").toInt());
                connect(spin, qOverload<int>(&QSpinBox::valueChanged), this,
                        [=](int v) { updateSystemConfigValue(configPath, configId, key, v); });
                fieldLayout->addWidget(spin);
            } else {
                // 小数：QDoubleSpinBox
                auto *spin = new NoWheelDoubleSpinBox(fieldWidget);
                spin->setDecimals(field.value("decimals").toInt(2));
                spin->setRange(field.value("min").toDouble(0.0), field.value("max").toDouble(1.0));
                spin->setSingleStep(field.value("step").toDouble(0.1));
                spin->setValue(field.value("value").toDouble());
                connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                        [=](double v) { updateSystemConfigValue(configPath, configId, key, v); });
                fieldLayout->addWidget(spin);
            }
        } else if (control == "switch") {
            // 开关：QCheckBox
            auto *check = new QCheckBox(fieldWidget);
            check->setChecked(field.value("value").toBool());
            connect(check, &QCheckBox::toggled, this,
                    [=](bool v) { updateSystemConfigValue(configPath, configId, key, v); });
            fieldLayout->addWidget(check);
        } else if (control == "select") {
            // 下拉：QComboBox
            auto *combo = new NoWheelComboBox(fieldWidget);
            for (const QJsonValue &opt : field.value("options").toArray()) {
                combo->addItem(opt.toString());
            }
            combo->setCurrentText(field.value("value").toString());
            connect(combo, &QComboBox::currentTextChanged, this,
                    [=](const QString &v) { updateSystemConfigValue(configPath, configId, key, v); });
            fieldLayout->addWidget(combo);
        } else if (control == "input") {
            // 文本输入：QLineEdit
            auto *edit = new QLineEdit(fieldWidget);
            edit->setText(field.value("value").toString());
            edit->setPlaceholderText(field.value("placeholder").toString());
            if (field.contains("maxLength")) {
                edit->setMaxLength(field.value("maxLength").toInt());
            }
            connect(edit, &QLineEdit::textChanged, this,
                    [=](const QString &v) { updateSystemConfigValue(configPath, configId, key, v); });
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
            // 弹窗修改，或进入编辑模式
            EditTaskDialog* dlg = new EditTaskDialog(EditMode::Edit,step,currentItem.id,nullptr);
            dlg->show();
            connect(dlg, &EditTaskDialog::accepted,[dlg, this]() {
                QJsonObject data = dlg->resultData();
                if (data.isEmpty()) {
                    return;
                }

                // 保存回 JSON 文件 & 刷新表格
                if (data["type"].toString() == "OPENCV") {
                    saveBase64ImageToFile(data);
                }
                updateConfigInJsonFile(AppPaths::instance().configPath(), currentItem.id, data);
                QTimer::singleShot(0, dlg, &QObject::deleteLater); // 延迟一拍
                showCurrentSelectStepsInTable();
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
                // qDebug() << "点击了删除: 行=" << i << " id=" << step["stepsId"].toString();
                removeConfigById(AppPaths::instance().configPath(), currentItem.id, step["stepsId"].toString());
                showCurrentSelectStepsInTable();
            }
        });

    }

    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->resizeRowsToContents();
}

void sessionwidget::showCurrentSelectStepsInTable()
{
    refreshConfig();
    if (currentItem.id.isEmpty())
    {
        Logger::log(QString("当前选择方案ID为空"));
        return;
    }


    bool hasConfigId = false;
    for (int i = 0; i < m_configArray.size(); ++i)
    {
        QJsonObject obj = m_configArray[i].toObject();
        if (obj["id"].toString() == currentItem.id)
        {
            showStepsInTable(obj["steps"].toArray());
            hasConfigId = true;
            break;
        }
    }

    if (!hasConfigId)
    {
        Logger::log("没有ConfigId: " + currentItem.id);
    }
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
        Logger::log(QStringLiteral("请先点击「绑定窗口」选择本标签页要操作的游戏实例"));
        return;
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

    // 异步启动：任务在独立线程执行，界面立刻恢复响应。
    // 方案选择与配置数组一并传入——任务线程写入自己的副本，
    // 之后别的标签页怎么切换方案都不影响本次运行。
    m_runner->start(steps, cycleCount, buildContext(), m_selectedItem, m_configArray);
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
        addConfigToJsonFile(AppPaths::instance().configPath(), configName);
        // 输入了有效内容
        Logger::log("已添加新的配置: " + configName);
        loadListWidgetData();
    } else {
        // 用户取消或输入为空
        Logger::log(QString("用户未输入或无效的配置名称"));
    }
}

// 按钮点击槽函数
void sessionwidget::onProgrammeRemoveBtnClicked()
{
    if (currentItem.id.isEmpty())
    {
        Logger::log(QString("未选中配置"));
        return;
    }
    // 弹出确认对话框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        this,
        tr("确认删除"),
        tr("确定要删除配置 “%1” 吗？").arg(currentItem.taskName),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes)
    {
        if (removeConfigById(AppPaths::instance().configPath(), currentItem.id))
        {
            Logger::log("已删除配置: " + currentItem.taskName);
            // 刷新列表，由 loadListWidgetData 重新选定第一个可见方案，
            // 保证界面选中项与 m_selectedItem 同步
            loadListWidgetData();
        }
        else
        {
            Logger::log("删除配置失败: " + currentItem.taskName);
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

void sessionwidget::updateBindStateLabel()
{
    if (!ui->bindStateLabel) {
        return;
    }

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
    });
    if (dialog.exec() == QDialog::Accepted) {
        // 设置已保存和应用
        Logger::log(QString("Setting配置已修改"));
    }
}


void sessionwidget::onProgrammeContentAddBtnClicked()
{
    QJsonObject empty;
    EditTaskDialog* dlg = new EditTaskDialog(EditMode::Add,empty,currentItem.id,nullptr); // 非模态
    dlg->show();
    connect(dlg, &EditTaskDialog::accepted, [dlg, this]() {
        QJsonObject data = dlg->resultData();
        // 保存 JSON
        if (data.isEmpty()) {
            return;
        }

        if (data["type"].toString() == "OPENCV") {
            saveBase64ImageToFile(data);
        }
        addConfigToJsonFile(AppPaths::instance().configPath(),currentItem.id,data);
        QTimer::singleShot(0, dlg, &QObject::deleteLater);  // 延迟一拍
        showCurrentSelectStepsInTable();
    });
    connect(dlg, &EditTaskDialog::imagePathRequested, this, [this](const QString& path) { showOpenCVIdentifyImage(path); });
}

void sessionwidget::onProgrammeUpBtnClicked()
{
    int currentRow = ui->tableWidget->currentRow();
    if (currentRow >= 0) {
        moveProgramme(AppPaths::instance().configPath(), currentItem.id, currentRow, true);
        showCurrentSelectStepsInTable();
        ui->tableWidget->selectRow(currentRow-1);
    } else {
        Logger::log(QString("没有选择方案内容的任何行"));
    }
}

void sessionwidget::onProgrammeDownBtnClicked()
{
    int currentRow = ui->tableWidget->currentRow();
    if (currentRow >= 0) {
        moveProgramme(AppPaths::instance().configPath(), currentItem.id, currentRow, false);
        showCurrentSelectStepsInTable();
        ui->tableWidget->selectRow(currentRow+1);
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
