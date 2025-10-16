#include "MouseSimulator.h"

// 构造函数
MouseSimulator::MouseSimulator() {
    std::random_device rd;
    m_randomEngine = std::mt19937(rd());
}

// 基础移动到指定位置
bool MouseSimulator::MoveTo(int x, int y) {
    if (!IsPositionValid(x, y)) {
        return false;
    }

    return SetCursorPos(x, y);
}

// 在当前位置点击
bool MouseSimulator::Click() {
    POINT currentPos = GetCurrentPosition();
    return ClickAt(currentPos.x, currentPos.y);
}

// 移动到指定位置并点击
bool MouseSimulator::ClickAt(int x, int y) {
    if (!MoveTo(x, y)) {
        return false;
    }

    INPUT inputs[2] = {0};

    // 鼠标按下
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    // 鼠标释放
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    UINT result = SendInput(2, inputs, sizeof(INPUT));
    return result == 2;
}

// 执行轨迹移动并点击
bool MouseSimulator::ExecuteTrajectoryWithClick(POINT start, POINT end,
                                               TrajectoryType trajectoryType,
                                               int steps,
                                               int delayBetweenPoints) {
    if (!IsPositionValid(start.x, start.y) || !IsPositionValid(end.x, end.y)) {
        return false;
    }

    // 生成轨迹点
    std::vector<POINT> trajectory = GenerateComplexTrajectory(start, end, trajectoryType, steps);

    if (trajectory.empty()) {
        return false;
    }

    // 执行轨迹移动
    for (const auto& point : trajectory) {
        if (!MoveTo(point.x, point.y)) {
            return false;
        }
        Sleep(delayBetweenPoints);
    }

    // 在终点执行点击
    return Click();
}

// 隐蔽点击
bool MouseSimulator::StealthClick(int x, int y, bool useHybrid) {
    if (!IsPositionValid(x, y)) {
        return false;
    }

    if (useHybrid) {
        // 混合使用多种方法增加隐蔽性
        if (GetRandomInRange(0, 100) < 70) {
            return StealthHardwareClick(x, y);
        } else {
            // 随机选择一个窗口发送消息（这里使用桌面窗口）
            return StealthMessageClick(GetDesktopWindow(), x, y);
        }
    } else {
        return StealthHardwareClick(x, y);
    }
}

// 人类行为模拟点击
bool MouseSimulator::HumanLikeClick(int x, int y, HWND targetWindow) {
    if (!IsPositionValid(x, y)) {
        return false;
    }

    // 1. 添加随机微小移动
    AddRandomMicroMovements(x, y);

    // 2. 随机延迟
    int preClickDelay = GetRandomDelay();
    Sleep(preClickDelay);

    // 3. 移动到精确位置
    if (!MoveTo(x, y)) {
        return false;
    }

    // 4. 执行点击（随机选择方法）
    bool result = false;
    if (GetRandomInRange(0, 100) < 80) {
        result = StealthHardwareClick(x, y);
    } else if (targetWindow) {
        result = StealthMessageClick(targetWindow, x, y);
    } else {
        result = Click();
    }

    // 5. 点击后随机行为
    if (result && m_humanLikeMode) {
        PostClickBehavior();
    }

    return result;
}

// 执行点击序列
bool MouseSimulator::ExecuteClickSequence(const std::vector<POINT>& clickPoints,
                                         TrajectoryType trajectoryType,
                                         int minDelay, int maxDelay) {
    if (clickPoints.size() < 2) {
        return false;
    }

    for (size_t i = 0; i < clickPoints.size() - 1; ++i) {
        // 移动到下一个点并点击
        if (!ExecuteTrajectoryWithClick(clickPoints[i], clickPoints[i + 1], trajectoryType)) {
            return false;
        }

        // 随机延迟
        int delay = GetRandomInRange(minDelay, maxDelay);
        Sleep(delay);
    }

    return true;
}

// 轨迹生成实现
std::vector<POINT> MouseSimulator::GenerateLinearTrajectory(POINT start, POINT end, int steps) {
    std::vector<POINT> points;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        int x = start.x + (int)(t * (end.x - start.x));
        int y = start.y + (int)(t * (end.y - start.y));
        points.push_back({ x, y });
    }
    return points;
}

std::vector<POINT> MouseSimulator::GenerateBezierTrajectory(POINT start, POINT end, int steps) {
    std::vector<POINT> points;

    // 生成控制点
    POINT control1 = {
        start.x + (end.x - start.x) / 3,
        start.y - (end.y - start.y) / 4
    };
    POINT control2 = {
        start.x + 2 * (end.x - start.x) / 3,
        end.y + (end.y - start.y) / 4
    };

    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        float u = 1 - t;

        // 三次贝塞尔曲线公式
        int x = (int)(u * u * u * start.x +
                      3 * u * u * t * control1.x +
                      3 * u * t * t * control2.x +
                      t * t * t * end.x);
        int y = (int)(u * u * u * start.y +
                      3 * u * u * t * control1.y +
                      3 * u * t * t * control2.y +
                      t * t * t * end.y);

        points.push_back({ x, y });
    }
    return points;
}

std::vector<POINT> MouseSimulator::GenerateComplexTrajectory(POINT start, POINT end,
                                                            TrajectoryType type, int steps) {
    switch (type) {
        case TrajectoryType::LINEAR:
            return GenerateLinearTrajectory(start, end, steps);

        case TrajectoryType::BEZIER:
            return GenerateBezierTrajectory(start, end, steps);

        case TrajectoryType::S_CURVE: {
            std::vector<POINT> points;
            for (int i = 0; i <= steps; i++) {
                float t = (float)i / steps;
                float s = sin(t * 3.14159f);

                int x = start.x + (int)(t * (end.x - start.x));
                int y = start.y + (int)(s * 30 + t * (end.y - start.y));

                points.push_back({ x, y });
            }
            return points;
        }

        case TrajectoryType::RANDOM_WALK: {
            std::vector<POINT> points;
            points.push_back(start);
            POINT current = start;

            int totalSteps = steps * 2;
            for (int i = 0; i < totalSteps; i++) {
                float progress = (float)i / totalSteps;

                int targetX = start.x + (int)(progress * (end.x - start.x));
                int targetY = start.y + (int)(progress * (end.y - start.y));

                int offsetX = GetRandomInRange(-10, 10);
                int offsetY = GetRandomInRange(-10, 10);

                current.x = targetX + offsetX;
                current.y = targetY + offsetY;

                points.push_back(current);
            }
            points.push_back(end);
            return points;
        }

        default:
            return GenerateLinearTrajectory(start, end, steps);
    }
}

std::vector<POINT> MouseSimulator::GenerateJitterTrajectory(int startX, int startY, int endX, int endY) {
    std::vector<POINT> points;

    POINT start = {startX, startY};
    POINT end = {endX, endY};
    POINT current = start;

    int steps = GetRandomInRange(3, 8);
    for (int i = 0; i < steps; i++) {
        float progress = (float)i / steps;
        int targetX = start.x + (int)(progress * (end.x - start.x));
        int targetY = start.y + (int)(progress * (end.y - start.y));

        int jitterX = GetRandomInRange(-m_jitterLevel, m_jitterLevel);
        int jitterY = GetRandomInRange(-m_jitterLevel, m_jitterLevel);

        current.x = targetX + jitterX;
        current.y = targetY + jitterY;

        points.push_back(current);
    }

    points.push_back(end);
    return points;
}

// 点击实现
bool MouseSimulator::HardwareClick(int x, int y) {
    if (!MoveTo(x, y)) {
        return false;
    }

    INPUT inputs[2] = {0};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

bool MouseSimulator::MessageClick(HWND hWnd, int x, int y) {
    LPARAM lParam = MAKELPARAM(x, y);

    PostMessage(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
    Sleep(GetRandomInRange(25, 60));
    PostMessage(hWnd, WM_LBUTTONUP, 0, lParam);

    return true;
}

bool MouseSimulator::StealthHardwareClick(int x, int y) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    INPUT inputDown = {0};
    inputDown.type = INPUT_MOUSE;
    inputDown.mi.dx = x * 65535 / screenWidth;
    inputDown.mi.dy = y * 65535 / screenHeight;
    inputDown.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE;

    INPUT inputUp = inputDown;
    inputUp.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE;

    SendInput(1, &inputDown, sizeof(INPUT));
    Sleep(GetRandomInRange(30, 75));
    UINT result = SendInput(1, &inputUp, sizeof(INPUT));

    return result == 1;
}

bool MouseSimulator::StealthMessageClick(HWND hWnd, int x, int y) {
    // 添加前置消息混淆检测
    PostMessage(hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(x-1, y-1));
    Sleep(GetRandomInRange(5, 15));

    return MessageClick(hWnd, x, y);
}

// 辅助方法
void MouseSimulator::AddRandomMicroMovements(int targetX, int targetY) {
    POINT currentPos = GetCurrentPosition();

    // 使用新签名的函数
    std::vector<POINT> jitter = GenerateJitterTrajectory(currentPos.x, currentPos.y, targetX, targetY);

    for (const auto& point : jitter) {
        MoveTo(point.x, point.y);
        Sleep(GetRandomInRange(3, 10));
    }
}

void MouseSimulator::PostClickBehavior() {
    if (GetRandomInRange(0, 100) < 30) {
        POINT pos = GetCurrentPosition();
        MoveTo(pos.x + GetRandomInRange(-5, 5), pos.y + GetRandomInRange(-5, 5));
    }
}

int MouseSimulator::GetRandomDelay() {
    return GetRandomInRange(m_minDelay, m_maxDelay);
}

int MouseSimulator::GetRandomInRange(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_randomEngine);
}

// 静态工具方法
POINT MouseSimulator::GetCurrentPosition() {
    POINT point;
    GetCursorPos(&point);
    return point;
}

bool MouseSimulator::IsPositionValid(int x, int y) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    return x >= 0 && x < screenWidth && y >= 0 && y < screenHeight;
}