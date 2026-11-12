#include "mainwindow.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <opencv2/core/version.hpp>

#include "src/core/AppPaths.h"
#include "src/core/AppVersion.h"
#include "src/core/Logger.h"
#include "src/core/SettingManager.h"
#include "src/core/UpdateChecker.h"
#include "src/core/WindowBindingStore.h"
#include "src/game/GameWindow.h"
#include "src/game/capture/Dx11HookCapture.h"
#include "src/platform/ProcessUtils.h"
#include "src/ui/session/sessionwidget.h"
#include "src/vision/ClassNameCache.h"
#include "src/vision/OcrEngine.h"
#include "src/vision/YOLODetector.h"

namespace {

// 单个会话面板的固定尺寸（沿用原主界面的绝对定位尺寸，
// 界面本身没有布局管理器，拉伸会导致控件错位）
constexpr int kSessionWidth = 979;
constexpr int kSessionHeight = 550;

} // namespace

mainwindow::mainwindow(QWidget *parent)
    : QWidget(parent) {
    setWindowTitle(QStringLiteral("onmyoji-script v") + APP_VERSION);

    setupUi();

    // 日志分发：worker 线程产生日志时带有会话标识（Logger::setThreadTag），
    // 据此投递到对应标签页；UI 线程的日志没有标识，落到当前标签页。
    // 必须用队列投递回到 UI 线程——sink 会在任意工作线程被调用。
    Logger::setSink([this](const QString& tag, const QString& msg) {
        QMetaObject::invokeMethod(this, [this, tag, msg]() {
            sessionwidget* target = tag.isEmpty() ? nullptr : findSessionByTag(tag);
            if (!target) {
                target = currentSession();
            }
            if (target) {
                target->appendLogToUI(msg);
            }
        }, Qt::QueuedConnection);
    });

    m_restoring = true;
    restoreSessions();
    m_restoring = false;

    initGlobalServices();

    setFixedSize(kSessionWidth + 2, kSessionHeight + 80);
}

mainwindow::~mainwindow() {
    // Logger 是全局单例，比本对象活得久；不清理会在下一条日志里访问悬空指针
    Logger::setSink(nullptr);
}

void mainwindow::setupUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    rootLayout->addWidget(m_tabs, 1);

    // 底部工具条：新建标签页。
    // 不用 QTabWidget 的 cornerWidget —— documentMode 下它可能不显示，
    // 那样就没有任何入口能开第二个实例了。
    auto* bottomBar = new QHBoxLayout();
    bottomBar->setContentsMargins(8, 4, 8, 6);
    bottomBar->setSpacing(8);

    auto* addButton = new QToolButton(this);
    addButton->setText(QStringLiteral("+ 新建标签页"));
    addButton->setToolTip(QStringLiteral("新建一个标签页，绑定另一个游戏实例"));
    connect(addButton, &QToolButton::clicked, this, [this]() {
        sessionwidget* session = createSession();
        m_tabs->setCurrentWidget(session);
    });
    bottomBar->addWidget(addButton);

    auto* hintLabel = new QLabel(
        QStringLiteral("每个标签页绑定一个游戏窗口，可同时运行、互不影响"), this);
    hintLabel->setStyleSheet(QStringLiteral("color: #888;"));
    bottomBar->addWidget(hintLabel);
    bottomBar->addStretch(1);

    rootLayout->addLayout(bottomBar);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &mainwindow::closeSession);

    // 切换标签页时同步 UI 线程的预览上下文（编辑器测试步骤作用于该实例）。
    // 任务执行线程持有自己固定的上下文，不受切换影响，因此运行中也可以自由切换。
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        auto* session = qobject_cast<sessionwidget*>(m_tabs->widget(index));
        if (session) {
            session->activateBinding();
        }
    });
}

void mainwindow::initGlobalServices()
{
    if (SETTING_CONFIG.loadConfig()) {
        Logger::log(QStringLiteral("Setting配置加载成功！"));
    }

    if (ClassNameCache::initialize(AppPaths::instance().classesNamePath())) {
        Logger::log(QStringLiteral("classes.txt配置加载成功！"));
    }

    Logger::log(QStringLiteral("OpenCV 版本 %1").arg(CV_VERSION));

    // 模型加载是进程级一次性工作，多个标签页共享同一份推理引擎
    if (YOLODetector::getInstance().initialize(
            AppPaths::instance().onmyojiYoloOnnxPath().toStdWString())) {
        Logger::log(QStringLiteral("onmyoji-yolo-v5 加载成功！"));
    }

    if (vision::initInProcessOcr()) {
        Logger::log(QStringLiteral("PP-OCRv6 进程内引擎加载成功！"));
    }

    // 延迟检查更新，避免阻塞首屏；仅在发现新版本时弹窗
    QTimer::singleShot(3000, this, []() {
        UpdateChecker::instance().checkAsync(false);
    });
}

void mainwindow::restoreSessions()
{
    WINDOW_BINDINGS.load();

    int restored = 0;
    for (const WindowBinding& item : WINDOW_BINDINGS.all()) {
        if (item.pid == 0 || !platform::isProcessAlive(item.pid)) {
            continue;
        }

        HWND hwnd = platform::findWindowByPid(item.pid);
        if (!hwnd) {
            continue;
        }

        sessionwidget* session = createSession();
        session->bindInstance(item.pid, hwnd, item.displayName);
        ++restored;
    }

    if (restored == 0) {
        createSession();
    }

    if (m_tabs->count() > 0) {
        m_tabs->setCurrentIndex(0);
        if (auto* session = currentSession()) {
            session->activateBinding();
        }
    }

    if (restored > 0) {
        Logger::log(QStringLiteral("已恢复 %1 个游戏实例标签页").arg(restored));
    }

    // 恢复出来的绑定可能包含重复（历史配置或用户手动改过 windows.json），
    // 这里统一算一次，重复时在标题与日志上给出提示
    refreshSharedBindings();
}

sessionwidget* mainwindow::createSession()
{
    auto* session = new sessionwidget(this);
    session->setFixedSize(kSessionWidth, kSessionHeight);

    const int index = m_tabs->addTab(session, session->tabTitle());
    m_tabs->setTabToolTip(index, QStringLiteral("右键可关闭；每个标签页绑定一个游戏实例"));

    connect(session, &sessionwidget::titleChanged, this, [this, session]() {
        refreshTabTitle(session);
    });

    // 绑定前查重：绑定同一个 PID 时给出提醒（不拦截）
    session->setPeerLookup([this, session](DWORD pid) {
        return peerDisplayNamesFor(static_cast<quint32>(pid), session);
    });

    // 唯一标签页时无需手动绑定：启动任务会自动用第一个游戏实例。
    // 每次都现查数量，标签页增减后立即生效。
    session->setSingleSessionProbe([this, session]() {
        return m_tabs->count() == 1 && m_tabs->indexOf(session) == 0;
    });

    connect(session, &sessionwidget::bindingChanged, this, [this]() {
        if (m_restoring) {
            return;
        }
        persistSessions();
        refreshSharedBindings();
    });

    connect(session, &sessionwidget::taskFinished, this, [this, session]() {
        // 关闭标签页时若任务在跑，等它真正结束再删
        if (session->property("closeAfterStop").toBool()) {
            m_tabs->removeTab(m_tabs->indexOf(session));
            session->deleteLater();
            if (m_tabs->count() == 0) {
                createSession();
            }
            // 少了一个标签页，共用关系可能已解除
            persistSessions();
            refreshSharedBindings();
            return;
        }

        if (m_pendingClose) {
            // 可能还有别的标签页在收尾，等全部停下再关窗口
            if (anyTaskRunning()) {
                return;
            }
            m_pendingClose = false;
            // 延后一拍再关，避免在信号处理栈里递归触发 closeEvent
            QTimer::singleShot(0, this, [this]() { close(); });
        }
    });

    return session;
}

sessionwidget* mainwindow::currentSession() const
{
    return qobject_cast<sessionwidget*>(m_tabs->currentWidget());
}

sessionwidget* mainwindow::findSessionByTag(const QString& tag) const
{
    if (tag.isEmpty()) {
        return nullptr;
    }
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* session = qobject_cast<sessionwidget*>(m_tabs->widget(i));
        if (session && session->logTag() == tag) {
            return session;
        }
    }
    return nullptr;
}

bool mainwindow::anyTaskRunning() const
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* session = qobject_cast<sessionwidget*>(m_tabs->widget(i));
        if (session && session->isTaskRunning()) {
            return true;
        }
    }
    return false;
}

void mainwindow::refreshTabTitle(sessionwidget* session)
{
    const int index = m_tabs->indexOf(session);
    if (index < 0) {
        return;
    }

    m_tabs->setTabText(index, session->tabTitle());

    // 共用实例的完整说明放 tooltip：标题栏放不下，但鼠标一悬停就能看清是谁
    if (session->sharedPeers().isEmpty()) {
        m_tabs->setTabToolTip(index, QStringLiteral("右键可关闭；每个标签页绑定一个游戏实例"));
        return;
    }
    m_tabs->setTabToolTip(
        index,
        QStringLiteral("与其他标签页共用同一个游戏实例：\n    %1\n\n"
                       "同时运行任务会互相干扰。")
            .arg(session->sharedPeers().join(QStringLiteral("\n    "))));
}

void mainwindow::activateSession(sessionwidget* session)
{
    if (session) {
        session->activateBinding();
    }
}

void mainwindow::persistSessions()
{
    QVector<WindowBinding> items;

    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* session = qobject_cast<sessionwidget*>(m_tabs->widget(i));
        if (!session || session->boundPid() == 0) {
            continue;
        }

        WindowBinding binding;
        binding.pid = session->boundPid();
        binding.displayName = session->displayName();
        binding.imageName = QString::fromWCharArray(
            platform::getProcessImageName(session->boundPid()).c_str());
        binding.windowTitle = binding.displayName;
        binding.boundAt = QDateTime::currentSecsSinceEpoch();
        items.push_back(binding);
    }

    WINDOW_BINDINGS.clear();
    for (const WindowBinding& item : items) {
        WINDOW_BINDINGS.add(item);
    }

    if (!WINDOW_BINDINGS.save()) {
        Logger::log(QStringLiteral("窗口绑定记录保存失败：%1")
                        .arg(WindowBindingStore::filePath()));
    }
}

void mainwindow::refreshSharedBindings()
{
    // pid -> 绑定该实例的所有标签页
    QHash<quint32, QVector<sessionwidget *>> owners;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* session = qobject_cast<sessionwidget*>(m_tabs->widget(i));
        if (session && session->boundPid() != 0) {
            owners[static_cast<quint32>(session->boundPid())].append(session);
        }
    }

    QStringList conflicts;

    for (auto it = owners.cbegin(); it != owners.cend(); ++it) {
        const QVector<sessionwidget *>& group = it.value();
        const bool shared = group.size() > 1;

        // 把「还有谁在用同一个实例」下发给每个标签页：
        // 标题加 ⚠共用 角标，状态栏常驻提示
        for (sessionwidget* session : group) {
            QStringList peers;
            if (shared) {
                for (sessionwidget* other : group) {
                    if (other == session) {
                        continue;
                    }
                    peers << QStringLiteral("标签页 %1「%2」")
                                 .arg(m_tabs->indexOf(other) + 1)
                                 .arg(other->displayName());
                }
            }
            session->setSharedPeers(peers);
        }

        if (shared) {
            QStringList tabs;
            for (sessionwidget* session : group) {
                tabs << QStringLiteral("标签页 %1").arg(m_tabs->indexOf(session) + 1);
            }
            conflicts << QStringLiteral("PID %1（%2）")
                             .arg(it.key())
                             .arg(tabs.join(QStringLiteral("、")));
        }
    }

    // 组合没变就不重复记日志（绑定/解绑一次会走到这里多次）
    const QString signature = conflicts.join(QStringLiteral(" | "));
    if (signature == m_lastSharedLog) {
        return;
    }
    m_lastSharedLog = signature;

    if (signature.isEmpty()) {
        Logger::log(QStringLiteral("已没有标签页共用同一个游戏实例"));
        return;
    }

    Logger::log(QStringLiteral("⚠ 多个标签页绑定了同一个游戏实例：%1 —— 允许共用，"
                               "但同时运行任务会互相干扰，请确认这是本意")
                    .arg(signature));
}

QStringList mainwindow::peerDisplayNamesFor(quint32 pid, sessionwidget* self) const
{
    QStringList peers;
    if (pid == 0) {
        return peers;
    }

    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* session = qobject_cast<sessionwidget*>(m_tabs->widget(i));
        if (!session || session == self) {
            continue;
        }
        if (static_cast<quint32>(session->boundPid()) != pid) {
            continue;
        }
        peers << QStringLiteral("标签页 %1「%2」").arg(i + 1).arg(session->displayName());
    }
    return peers;
}

void mainwindow::closeSessionDeferred(sessionwidget* session)
{
    session->setProperty("closeAfterStop", true);
    session->stopTask();
}

void mainwindow::closeSession(int index)
{
    auto* session = qobject_cast<sessionwidget*>(m_tabs->widget(index));
    if (!session) {
        return;
    }

    if (session->isTaskRunning()) {
        const auto reply = QMessageBox::question(
            this, QStringLiteral("关闭标签页"),
            QStringLiteral("该标签页的任务正在运行，是否停止并关闭？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }

        // run() 在 UI 线程阻塞，不能直接删；置标记等 taskFinished 再处理
        closeSessionDeferred(session);
        return;
    }

    m_tabs->removeTab(index);
    session->deleteLater();

    if (m_tabs->count() == 0) {
        createSession();
    }

    persistSessions();
    refreshSharedBindings();
}

void mainwindow::unloadAllHooks()
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* session = qobject_cast<sessionwidget*>(m_tabs->widget(i));
        if (!session) {
            continue;
        }

        const DWORD pid = session->boundPid();
        if (pid == 0 || !platform::isProcessAlive(pid)) {
            continue;
        }
        if (!capture::isDllInjected(pid)) {
            continue;
        }

        Logger::log(QStringLiteral("正在从游戏进程卸载 hook DLL（PID %1）...").arg(pid));
        if (capture::dllStopHook(QString::number(pid))) {
            Logger::log(QStringLiteral("hook DLL 已卸载（PID %1）").arg(pid));
        } else {
            Logger::log(QStringLiteral("hook DLL 卸载未确认（PID %1，不影响退出）").arg(pid));
        }
    }
}

void mainwindow::closeEvent(QCloseEvent *event)
{
    if (!anyTaskRunning()) {
        unloadAllHooks();
        Logger::setSink(nullptr);
        event->accept();
        return;
    }

    if (m_pendingClose) {
        // 已在等待任务结束，拒绝这次重复的关闭请求
        event->ignore();
        return;
    }

    const auto reply = QMessageBox::question(
        this, QStringLiteral("退出确认"),
        QStringLiteral("有标签页正在运行任务，是否停止全部任务并退出？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        event->ignore();
        return;
    }

    Logger::log(QStringLiteral("停止所有任务并退出..."));
    m_pendingClose = true;

    // 每个会话各自停止；全部 finished 后由 createSession 里的
    // taskFinished 回调重新触发关闭流程
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* session = qobject_cast<sessionwidget*>(m_tabs->widget(i));
        if (session && session->isTaskRunning()) {
            session->stopTask();
        }
    }
    event->ignore();
}
