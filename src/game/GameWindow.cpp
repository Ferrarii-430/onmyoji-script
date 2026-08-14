#include "src/game/GameWindow.h"

#include <QDebug>
#include <algorithm>
#include <cmath>

#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/SettingManager.h"
#include "src/game/capture/Dx11HookCapture.h"
#include "src/platform/DPIHelper.h"
#include "src/platform/MouseSimulator.h"
#include "src/platform/ProcessUtils.h"

namespace {

// 从顶层窗口开始逐层命中测试，定位点击坐标下真正接收输入的子窗口，
// 并把坐标换算到该子窗口的客户区坐标系。
HWND resolveMessageTarget(HWND topWindow, cv::Point& clientPoint)
{
    HWND current = topWindow;
    POINT pt{clientPoint.x, clientPoint.y};

    while (true) {
        HWND child = ChildWindowFromPointEx(current, pt,
            CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT);
        if (!child || child == current) {
            break;
        }
        MapWindowPoints(current, child, &pt, 1);
        current = child;
    }

    clientPoint = cv::Point(pt.x, pt.y);
    return current;
}

} // namespace

GameWindow& GameWindow::instance()
{
    static GameWindow instance;
    return instance;
}

bool GameWindow::locate()
{
    DWORD pid = platform::findPidByProcessName(target_);
    if (pid == 0) {
        Logger::log(QString("找不到桌面版游戏进程： %1").arg(target_.c_str()));
        return false;
    }
    hwnd_ = platform::findWindowByPid(pid);

    if (!hwnd_) {
        Logger::log(QString("阴阳师游戏窗口未找到"));
        return false;
    }
    return true;
}

QString GameWindow::processId()
{
    if (hwnd_ == nullptr) {
        qWarning() << "窗口句柄为空，无法获取进程ID";
        return QString();
    }

    if (!IsWindow(hwnd_)) {
        qWarning() << "窗口句柄无效";
        hwnd_ = nullptr;
        return QString();
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd_, &processId);

    if (processId == 0) {
        qWarning() << "无法获取窗口进程ID，错误代码:" << GetLastError();
        return QString();
    }

    return QString::number(processId);
}

void GameWindow::ensureMinWidthForCapture(int minWidth)
{
    if (!IsWindow(hwnd_)) {
        return;
    }

    // 判断当前窗口宽度。优先使用上次截图尺寸（反映 hook swap chain 实际尺寸）；
    // 首次截图时 lastCaptureSize_ 为 0，需要主动获取窗口客户区尺寸。
    int currentWidth = lastCaptureSize_.width;
    const bool wasIconic = IsIconic(hwnd_) != 0;

    // 获取窗口"正常"位置/尺寸（非最小化/非最大化状态下的矩形）。
    // 注意：阴阳师窗口被系统隐藏时，rcNormalPosition 可能返回系统占位区
    // (-30000 附近, 30x30)。位置若在系统隐藏区，恢复时改用屏幕中心。
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(WINDOWPLACEMENT);
    if (!GetWindowPlacement(hwnd_, &wp)) {
        Logger::log(QString("窗口宽度调整: GetWindowPlacement 失败，放弃调整"));
        return;
    }
    const RECT placementRect = wp.rcNormalPosition;
    const int placementW = placementRect.right - placementRect.left;
    const int placementH = placementRect.bottom - placementRect.top;
    const bool isHiddenPlacement = placementRect.left < -10000 || placementRect.left > 10000;

    // 首次截图且窗口最小化：GetClientRect 返回 0，需临时恢复以探测真实尺寸
    bool sizeProbed = false;
    if (currentWidth == 0 && wasIconic) {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        core::waitWithEventProcessing(50);
        sizeProbed = true;
    }

    // 首次截图：获取客户区尺寸作为判断依据
    if (currentWidth == 0) {
        RECT clientRect{};
        if (GetClientRect(hwnd_, &clientRect)) {
            currentWidth = clientRect.right;
        }
    }

    // 尺寸足够，无需调整
    if (currentWidth >= minWidth) {
        if (sizeProbed) {
            ShowWindow(hwnd_, SW_MINIMIZE);
            core::waitWithEventProcessing(50);
        }
        // Logger::log(QString("窗口宽度调整: 当前宽度 %1 >= %2，无需调整")
        //             .arg(currentWidth).arg(minWidth));
        return;
    }

    Logger::log(QString("窗口宽度调整: 当前宽度 %1 < %2，开始调整窗口尺寸")
                .arg(currentWidth).arg(minWidth));
    Logger::log(QString("窗口宽度调整: placement 位置 (%1, %2), 尺寸 %3x%4, isHidden=%5")
                .arg(placementRect.left).arg(placementRect.top)
                .arg(placementW).arg(placementH).arg(isHiddenPlacement));

    // 若窗口最小化且尚未为探测而恢复，现在恢复（不激活，不抢前台）
    if (wasIconic && !sizeProbed) {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        core::waitWithEventProcessing(50);
        Logger::log(QString("窗口宽度调整: 窗口已从最小化恢复（不激活）"));
    }

    // 阴阳师(Unity) 不接受外部直接指定的窗口尺寸，会按窗口高度自行推导 16:9 客户区：
    //   客户高 = 窗口高 - 自定义边框(实测约 78px)，客户宽 = 客户高 * 16 / 9
    // （实测: 窗口 998x622 -> 渲染 966x544；窗口 632x416 -> 渲染 600x338）
    // 因此只设置"宽度"不会生效，必须通过设置高度间接控制渲染分辨率。
    // 这里按最小渲染宽度反推固定目标尺寸（16 的倍数并留 16px 余量防止边框误差），
    // 每次都设置为同一个值，保证调整后渲染分辨率稳定一致。
    constexpr int kFrameW = 32; // 实测窗口外框与客户区的宽度差
    constexpr int kFrameH = 78; // 实测窗口外框与客户区的高度差
    const int clientW = ((minWidth + 31) / 16) * 16; // ≥ minWidth 且为 16 倍数
    const int clientH = clientW * 9 / 16;
    const int winW = clientW + kFrameW;
    const int winH = clientH + kFrameH;

    // 先移到屏幕外避免用户看到闪烁
    SetWindowPos(hwnd_, HWND_BOTTOM,
                 -10000, -10000, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE);

    // 模拟用户拖动边框调整尺寸的完整消息序列。
    // 仅 SetWindowPos 改窗口矩形不会触发 Unity 调用 Screen.SetResolution，
    // 必须发送 WM_ENTERSIZEMOVE / WM_EXITSIZEMOVE / WM_SIZE 让 Unity 检测到尺寸变化。
    SendMessage(hwnd_, WM_ENTERSIZEMOVE, 0, 0);
    SetWindowPos(hwnd_, nullptr,
                 -10000, -10000,
                 winW, winH,
                 SWP_NOACTIVATE | SWP_NOZORDER);
    RECT newSize{};
    GetClientRect(hwnd_, &newSize);
    SendMessage(hwnd_, WM_SIZE, SIZE_RESTORED,
                MAKELPARAM(newSize.right, newSize.bottom));
    SendMessage(hwnd_, WM_EXITSIZEMOVE, 0, 0);
    Logger::log(QString("窗口宽度调整: 已发送尺寸调整消息 (客户区 %1x%2, 窗口 %3x%4)")
                .arg(clientW).arg(clientH).arg(winW).arg(winH));

    // 等待 Unity 监听 WM_EXITSIZEMOVE 后调用 Screen.SetResolution 重建 swap chain
    core::waitWithEventProcessing(500);

    // 恢复原位置。若 placement 在系统隐藏区，改用屏幕中心。
    int restoreX = placementRect.left;
    int restoreY = placementRect.top;
    if (isHiddenPlacement) {
        restoreX = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
        restoreY = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;
        Logger::log(QString("窗口宽度调整: placement 在系统隐藏区，恢复位置改用屏幕中心 (%1, %2)")
                    .arg(restoreX).arg(restoreY));
    } else {
        // 窗口变宽后原位置可能超出屏幕（如原本贴近右缘）导致右侧被遮挡，需重新计算恢复位置。
        // 注意两点：
        // 1. 用显示器坐标（MonitorFromPoint）而不是 SM_CXSCREEN（后者仅主屏，多显示器不准）；
        // 2. 游戏在尺寸调整后常自行把窗口恢复到原来的大小（如 752x483 -> 988x616），
        //    因此用 max(临时尺寸, 原尺寸) 做边界校验，确保恢复后无论哪种尺寸都完整可见。
        POINT anchor{placementRect.left + 1, placementRect.top + 1};
        const HMONITOR hMon = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(MONITORINFO);
        if (hMon && GetMonitorInfo(hMon, &mi)) {
            // 用 rcWork（工作区，已排除任务栏）而非 rcMonitor
            Logger::log(QString("窗口宽度调整: 工作区 [%1,%2 - %3,%4], 预计恢复 X=%5, 预计宽=%6")
                        .arg(mi.rcWork.left).arg(mi.rcWork.top)
                        .arg(mi.rcWork.right).arg(mi.rcWork.bottom)
                        .arg(restoreX).arg(qMax(winW, placementW)));
            const int workLeft = mi.rcWork.left;
            const int workRight = mi.rcWork.right;
            const int workTop = mi.rcWork.top;
            const int workBottom = mi.rcWork.bottom;
            // 留 16px 余量，避免窗口边框正好压在工作区边缘或贴边任务栏
            constexpr int kMargin = 16;
            const int finalW = qMax(winW, placementW) + kMargin;
            const int finalH = qMax(winH, placementH) + kMargin;
            if (restoreX + finalW > workRight) {
                restoreX = qMax(static_cast<int>(workLeft),
                                static_cast<int>(workRight) - finalW);
                Logger::log(QString("窗口宽度调整: 原位置贴右缘，横向位置修正为 (%1, %2)")
                            .arg(restoreX).arg(restoreY));
            }
            if (restoreY + finalH > workBottom) {
                restoreY = qMax(static_cast<int>(workTop),
                                static_cast<int>(workBottom) - finalH);
                Logger::log(QString("窗口宽度调整: 原位置贴下缘，纵向位置修正为 (%1, %2)")
                            .arg(restoreX).arg(restoreY));
            }
        }
    }
    SetWindowPos(hwnd_, nullptr,
                 restoreX, restoreY,
                 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    Logger::log(QString("窗口宽度调整: 位置已恢复至 (%1, %2)").arg(restoreX).arg(restoreY));

    // 二次校正：游戏(Unity)的 Screen.SetResolution 是异步的，恢复位置后才把窗口
    // resize 回原尺寸，且该过程可能改变窗口位置（每次运行 placement 逐渐漂移）。
    // 等游戏 resize 完成后读取实际窗口矩形，超出工作区则强制拉回。
    core::waitWithEventProcessing(1000);
    if (!IsIconic(hwnd_)) {
        RECT actual{};
        if (GetWindowRect(hwnd_, &actual)) {
            const HMONITOR hMon = MonitorFromPoint(
                POINT{(actual.left + actual.right) / 2, (actual.top + actual.bottom) / 2},
                MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{};
            mi.cbSize = sizeof(MONITORINFO);
            if (hMon && GetMonitorInfo(hMon, &mi)) {
                const int aw = actual.right - actual.left;
                const int ah = actual.bottom - actual.top;
                int ax = actual.left;
                int ay = actual.top;
                if (ax + aw > mi.rcWork.right) ax = mi.rcWork.right - aw;
                if (ay + ah > mi.rcWork.bottom) ay = mi.rcWork.bottom - ah;
                if (ax < mi.rcWork.left) ax = mi.rcWork.left;
                if (ay < mi.rcWork.top) ay = mi.rcWork.top;
                if (ax != actual.left || ay != actual.top) {
                    SetWindowPos(hwnd_, nullptr, ax, ay, 0, 0,
                                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
                    Logger::log(QString("窗口宽度调整: 二次校正，实际矩形 (%1,%2 %3x%4) 超出工作区，已移至 (%5, %6)")
                                .arg(actual.left).arg(actual.top).arg(aw).arg(ah)
                                .arg(ax).arg(ay));
                }
            }
        }
    }

    // 恢复最小化状态（hook 截图支持最小化，此后按新尺寸出帧）
    if (wasIconic) {
        ShowWindow(hwnd_, SW_MINIMIZE);
        core::waitWithEventProcessing(50);
        Logger::log(QString("窗口宽度调整: 窗口已重新最小化"));
    }

    // 清空上次截图尺寸，强制下次截图重新获取（避免用到调整前的旧值）
    lastCaptureSize_ = cv::Size();

    Logger::log(QString("窗口宽度调整完成"));
}

void GameWindow::clickInWindow(const cv::Point& clickPoint)
{
    if (!IsWindow(hwnd_)) {
        Logger::log(QString("错误: 无效的窗口句柄"));
        return;
    }

    RECT mapClientRect{};
    GetClientRect(hwnd_, &mapClientRect);
    const bool isDegenerateClientRect = IsIconic(hwnd_)
        || mapClientRect.right <= 0
        || mapClientRect.bottom <= 0;

    const cv::Point clientPoint = mapCapturePointToClient(clickPoint);
    Logger::log(QString("点击坐标映射: 捕获(%1,%2) -> 客户区(%3,%4), 捕获尺寸: %5x%6, 客户区尺寸: %7x%8")
               .arg(clickPoint.x).arg(clickPoint.y)
               .arg(clientPoint.x).arg(clientPoint.y)
               .arg(lastCaptureSize_.width)
               .arg(lastCaptureSize_.height)
               .arg(mapClientRect.right)
               .arg(mapClientRect.bottom));

    if (isDegenerateClientRect) {
        if (lastCaptureSize_.width <= 0 || lastCaptureSize_.height <= 0) {
            Logger::log(QString("错误: 点击坐标 (%1, %2) 超出客户区范围").arg(clientPoint.x).arg(clientPoint.y));
            return;
        }
    } else if (!DPIHelper::IsPointInClientRect(hwnd_, clientPoint)) {
        Logger::log(QString("错误: 点击坐标 (%1, %2) 超出客户区范围").arg(clientPoint.x).arg(clientPoint.y));
        return;
    }

    MouseSimulator simulator;
    simulator.SetHumanLikeMode(true);
    simulator.SetRandomDelayRange(20, 100);
    simulator.SetJitterLevel(3);

    RECT clientRect, windowRect;
    GetClientRect(hwnd_, &clientRect);
    GetWindowRect(hwnd_, &windowRect);

    Logger::log(QString("窗口信息 - 客户区: %1x%2, 窗口: %3x%4, DPI缩放: %5")
               .arg(clientRect.right).arg(clientRect.bottom)
               .arg(windowRect.right - windowRect.left).arg(windowRect.bottom - windowRect.top)
               .arg(DPIHelper::GetWindowDPIScaling(hwnd_)));

    POINT screenPoint = { clientPoint.x, clientPoint.y };
    if (!isDegenerateClientRect) {
        ClientToScreen(hwnd_, &screenPoint);
    }

    Logger::log(QString("坐标转换 - 客户区: (%1, %2) -> 屏幕: (%3, %4)")
               .arg(clientPoint.x).arg(clientPoint.y)
               .arg(screenPoint.x).arg(screenPoint.y));

    bool success = false;
    QString mouseClickMode = SETTING_CONFIG.getMouseClickMode();

    cv::Point messagePoint = clientPoint;
    HWND messageTarget = hwnd_;
    if (!isDegenerateClientRect) {
        messageTarget = resolveMessageTarget(hwnd_, messagePoint);
        if (messageTarget != hwnd_) {
            Logger::log(QString("消息点击目标为子窗口: (%1, %2) -> (%3, %4)")
                       .arg(clientPoint.x).arg(clientPoint.y)
                       .arg(messagePoint.x).arg(messagePoint.y));
        }
    }

    if (mouseClickMode == "Hook" || mouseClickMode == "Dx11Hook") {
        // 通过已注入的 DX11 hook DLL 在游戏进程内部投递鼠标消息，
        // 适合 Unity 游戏后台运行；坐标使用截图(后备缓冲区)坐标系，
        // 由 DLL 侧换算为客户区坐标，因此这里传入原始 clickPoint。
        // Hook 模式不依赖窗口客户区状态，即使窗口最小化也能正常工作。
        const QString pid = processId();
        if (pid.isEmpty()) {
            Logger::log(QString("Hook点击失败：无法获取进程ID"));
        } else {
            success = capture::clickByDllInjection(pid, clickPoint.x, clickPoint.y);
        }
    } else if (isDegenerateClientRect) {
        Logger::log(QString("窗口最小化或客户区为0，使用后台消息点击(PostMessage)"));
        success = simulator.StealthMessageClick(messageTarget, messagePoint.x, messagePoint.y);
    } else if (mouseClickMode == "PostMessage") {
        success = simulator.StealthMessageClick(messageTarget, messagePoint.x, messagePoint.y);
    } else if (mouseClickMode == "InputMouse") {
        // InputMouse 模式使用 SendInput 模拟硬件点击，该 API 只将输入路由到前台窗口，
        // 因此必须先将目标窗口带到前台，否则点击会发到当前前台窗口导致无效
        BringWindowToTop(hwnd_);
        SetForegroundWindow(hwnd_);

        POINT start = MouseSimulator::GetCurrentPosition();
        POINT targetScreen = { screenPoint.x, screenPoint.y };
        success = simulator.ExecuteTrajectoryWithClick(start, targetScreen,
                                                     TrajectoryType::BEZIER,
                                                     SETTING_CONFIG.getMouseSpeed() * 10);
    } else {
        Logger::log(QString("无法识别的鼠标点击模式，执行默认策略"));
        success = simulator.StealthMessageClick(messageTarget, messagePoint.x, messagePoint.y);
    }

    if (success) {
        Logger::log(QString("点击成功"));
    } else {
        Logger::log(QString("点击失败"));
    }
}

cv::Point GameWindow::mapCapturePointToClient(const cv::Point& capturePoint)
{
    if (!IsWindow(hwnd_) || lastCaptureSize_.width <= 0 || lastCaptureSize_.height <= 0) {
        return capturePoint;
    }

    RECT clientRect{};
    RECT windowRect{};
    if (!GetClientRect(hwnd_, &clientRect) || !GetWindowRect(hwnd_, &windowRect)) {
        return capturePoint;
    }

    const int clientWidth = clientRect.right;
    const int clientHeight = clientRect.bottom;
    const int windowWidth = windowRect.right - windowRect.left;
    const int windowHeight = windowRect.bottom - windowRect.top;
    const int captureWidth = lastCaptureSize_.width;
    const int captureHeight = lastCaptureSize_.height;
    const bool isDegenerateClientRect = IsIconic(hwnd_)
        || clientWidth <= 0
        || clientHeight <= 0;

    cv::Point mapped = capturePoint;
    constexpr int tolerance = 2;

    if (isDegenerateClientRect) {
        mapped.x = std::clamp(mapped.x, 0, std::max(0, captureWidth - 1));
        mapped.y = std::clamp(mapped.y, 0, std::max(0, captureHeight - 1));
        return mapped;
    }

    if (std::abs(captureWidth - windowWidth) <= tolerance
        && std::abs(captureHeight - windowHeight) <= tolerance) {
        POINT clientTopLeft{0, 0};
        if (ClientToScreen(hwnd_, &clientTopLeft)) {
            const int offsetX = clientTopLeft.x - windowRect.left;
            const int offsetY = clientTopLeft.y - windowRect.top;
            mapped.x -= offsetX;
            mapped.y -= offsetY;
        }
    } else if (captureWidth != clientWidth || captureHeight != clientHeight) {
        const double scaleX = static_cast<double>(clientWidth) / std::max(1, captureWidth);
        const double scaleY = static_cast<double>(clientHeight) / std::max(1, captureHeight);
        mapped.x = static_cast<int>(std::round(mapped.x * scaleX));
        mapped.y = static_cast<int>(std::round(mapped.y * scaleY));
    }

    mapped.x = std::clamp(mapped.x, 0, std::max(0, clientWidth - 1));
    mapped.y = std::clamp(mapped.y, 0, std::max(0, clientHeight - 1));

    return mapped;
}

void GameWindow::postKey(UINT virtualKey)
{
    PostMessage(hwnd_, WM_KEYDOWN, virtualKey, 0);
    core::waitWithEventProcessing(10);
    PostMessage(hwnd_, WM_KEYUP, virtualKey, 0);
}
