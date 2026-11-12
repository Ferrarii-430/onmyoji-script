#include "src/platform/WindowPicker.h"

#include <QCloseEvent>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWidget>

#include "src/platform/ProcessUtils.h"

namespace platform {

namespace {

// 点选超时保护：用户中途离开时遮罩不能永久挡住桌面
constexpr int kPickTimeoutMs = 5 * 60 * 1000;

// 虚拟桌面（Qt 逻辑坐标）：多显示器时合并各屏 geometry，
// 遮罩必须盖住整个桌面，否则鼠标移到副屏就失去高亮。
QRect virtualDesktopGeometry()
{
    QRect rect;
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (const QScreen* screen : screens) {
        rect = rect.united(screen->geometry());
    }
    if (rect.isEmpty()) {
        rect = QRect(0, 0, 1920, 1080);
    }
    return rect;
}

// 物理像素 -> 逻辑像素的换算因子。遮罩跨屏时取主屏缩放因子，
// 混合 DPI 多屏下副屏的高亮框会有偏移，单屏与同缩放多屏场景精确。
qreal overlayDpr()
{
    const QScreen* primary = QGuiApplication::primaryScreen();
    return primary ? primary->devicePixelRatio() : 1.0;
}

// 全屏遮罩：一半是「暗场」，一半是「取景器」。
// 遮罩自身接收鼠标，避免点选时误触到下方的游戏窗口；
// 而 findTopLevelWindowAtPoint 会跳过本进程窗口，因此仍能识别出下方目标。
//
// 输入处理刻意不调用 grabKeyboard()：键盘抓取一旦没有成对释放，
// 整个程序（甚至系统）都收不到键盘输入，表现为彻底卡死。改用 QTimer
// 轮询 GetAsyncKeyState，焦点丢失时 ESC / 右键同样有效。
class PickerOverlay : public QWidget
{
public:
    PickerOverlay(const QString& processName, PickedWindow* out)
        : m_processName(processName)
        , m_out(out)
    {
        // 用 Qt::Window 而非 Qt::Tool：Tool 窗口在部分场景下不参与激活，
        // 拿不到焦点时连鼠标捕获都可能异常，取景器会变成「点了没反应」。
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::CrossCursor);
        setGeometry(virtualDesktopGeometry());

        m_hint = processName.isEmpty()
            ? QStringLiteral("点击选择要绑定的窗口　·　右键或 ESC 取消")
            : QStringLiteral("点击选择 %1 的窗口　·　右键或 ESC 取消").arg(processName);
    }

    // 阻塞运行直到用户确认或取消
    void run()
    {
        // 权限不足时 OpenProcess 读不到进程名，点选会退化为「总是选不中」
        enableDebugPrivilege();

        show();
        raise();
        activateWindow();
        setFocus(Qt::ActiveWindowFocusReason);

        // 进入时鼠标可能本就处于按下状态（例如刚点完按钮），
        // 先同步一次，避免这次按键被误判成新的点击
        m_leftWasDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        m_rightWasDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        updateTargetAtCursor();

        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(50);
        connect(m_pollTimer, &QTimer::timeout, this, &PickerOverlay::pollInput);
        m_pollTimer->start();

        m_elapsed.start();

        QEventLoop loop;
        m_loop = &loop;
        loop.exec();
        m_loop = nullptr;
    }

    bool accepted() const { return m_accepted; }

protected:
    void mouseMoveEvent(QMouseEvent*) override
    {
        updateTargetAtCursor();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            confirmAtCursor();
        } else if (event->button() == Qt::RightButton) {
            cancel();
        }
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Escape) {
            cancel();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void closeEvent(QCloseEvent* event) override
    {
        // 遮罩被外部关闭（Alt+F4 等）等同取消，不交给默认流程去销毁窗口
        event->ignore();
        cancel();
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        const QRect full = rect();
        const QColor dim(0, 0, 0, 110);

        if (m_target) {
            const QRect target = targetRectLocal();
            if (target.isValid() && !target.isEmpty()) {
                // 目标区域留亮，其余四块压暗。分块填充比 CompositionMode_Clear
                // 稳妥：Clear 在部分显卡驱动上会把整层一起清掉。
                painter.fillRect(QRect(0, 0, full.width(), target.top()), dim);
                painter.fillRect(QRect(0, target.bottom() + 1, full.width(),
                                       full.height() - target.bottom() - 1), dim);
                painter.fillRect(QRect(0, target.top(), target.left(), target.height()), dim);
                painter.fillRect(QRect(target.right() + 1, target.top(),
                                       full.width() - target.right() - 1, target.height()), dim);

                painter.setPen(QPen(QColor(90, 200, 250), 2));
                painter.drawRect(target.adjusted(0, 0, -1, -1));
            } else {
                painter.fillRect(full, dim);
            }
        } else {
            painter.fillRect(full, dim);
        }

        // 顶部提示条：未命中时显示操作说明，命中后显示目标窗口信息
        const QString label = m_target ? describeTargetText() : m_hint;

        QFont font = painter.font();
        font.setPointSize(10);
        painter.setFont(font);

        const QFontMetrics metrics(font);
        const int boxWidth = metrics.horizontalAdvance(label) + 32;
        const int boxHeight = metrics.height() + 16;
        const QRect box((full.width() - boxWidth) / 2, 24, boxWidth, boxHeight);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(24, 24, 24, 225));
        painter.drawRoundedRect(box, 6, 6);

        painter.setPen(QColor(240, 240, 240));
        painter.drawText(box, Qt::AlignCenter, label);
    }

private:
    // 不依赖窗口焦点的输入兜底：ESC 取消、右键取消、左键确认
    void pollInput()
    {
        const bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const bool rightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        const bool escDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

        if (escDown || (rightDown && !m_rightWasDown)) {
            cancel();
            return;
        }

        if (leftDown && !m_leftWasDown) {
            confirmAtCursor();
            return;
        }

        m_leftWasDown = leftDown;
        m_rightWasDown = rightDown;

        if (m_elapsed.isValid() && m_elapsed.elapsed() > kPickTimeoutMs) {
            cancel();
            return;
        }

        updateTargetAtCursor();
    }

    void updateTargetAtCursor()
    {
        POINT cursor{};
        if (!GetCursorPos(&cursor)) {
            return;
        }

        HWND target = findTopLevelWindowAtPoint(cursor, m_processName.toStdString());
        if (target == m_target) {
            return;
        }

        m_target = target;
        m_targetInfo = describeWindow(target);
        update();
    }

    void confirmAtCursor()
    {
        updateTargetAtCursor();

        if (!m_target || !m_targetInfo.valid()) {
            // 没命中目标时保持取景状态，等用户重新选择
            return;
        }

        if (m_out) {
            *m_out = m_targetInfo;
        }
        m_accepted = true;
        finish();
    }

    QRect targetRectLocal() const
    {
        RECT rc{};
        if (!m_target || !GetWindowRect(m_target, &rc)) {
            return {};
        }

        const qreal dpr = overlayDpr();
        const QPoint topLeft(qRound(rc.left / dpr), qRound(rc.top / dpr));
        const QPoint bottomRight(qRound(rc.right / dpr), qRound(rc.bottom / dpr));

        QRect target(topLeft, bottomRight);
        target.translate(-geometry().topLeft());
        return target;
    }

    QString describeTargetText() const
    {
        const QString name = m_targetInfo.title.isEmpty()
            ? m_targetInfo.imageName
            : m_targetInfo.title;
        return QStringLiteral("%1　·　PID %2　·　%3 × %4")
            .arg(name)
            .arg(m_targetInfo.pid)
            .arg(m_targetInfo.clientWidth)
            .arg(m_targetInfo.clientHeight);
    }

    void cancel()
    {
        m_accepted = false;
        finish();
    }

    void finish()
    {
        if (m_finished) {
            return;
        }
        m_finished = true;

        if (m_pollTimer) {
            m_pollTimer->stop();
        }
        hide();

        if (m_loop) {
            m_loop->quit();
        }
    }

    QString m_processName;
    PickedWindow* m_out = nullptr;
    QEventLoop* m_loop = nullptr;
    QTimer* m_pollTimer = nullptr;
    QElapsedTimer m_elapsed;

    HWND m_target = nullptr;
    PickedWindow m_targetInfo;
    QString m_hint;

    bool m_accepted = false;
    bool m_finished = false;
    bool m_leftWasDown = false;
    bool m_rightWasDown = false;
};

} // namespace

PickedWindow describeWindow(HWND hwnd)
{
    PickedWindow info;
    if (!hwnd || !IsWindow(hwnd)) {
        return info;
    }

    info.hwnd = hwnd;
    GetWindowThreadProcessId(hwnd, &info.pid);

    const std::wstring title = getWindowTitle(hwnd);
    info.title = QString::fromWCharArray(title.c_str());

    const std::wstring imageName = getProcessImageName(info.pid);
    info.imageName = QString::fromWCharArray(imageName.c_str());

    const std::wstring imagePath = getProcessImagePath(info.pid);
    info.imagePath = QString::fromWCharArray(imagePath.c_str());

    getClientSize(hwnd, &info.clientWidth, &info.clientHeight);
    return info;
}

bool pickWindow(const QString& processName, PickedWindow* out)
{
    if (!out) {
        return false;
    }

    PickerOverlay overlay(processName, out);
    overlay.run();
    return overlay.accepted();
}

} // namespace platform
