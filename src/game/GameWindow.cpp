#include "src/game/GameWindow.h"

#include <QDebug>
#include <algorithm>
#include <cmath>

#include "src/core/EventLoopUtils.h"
#include "src/core/Logger.h"
#include "src/core/SettingManager.h"
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

    if (isDegenerateClientRect) {
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
