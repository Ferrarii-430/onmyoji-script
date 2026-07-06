#ifndef MOUSE_SIMULATOR_H
#define MOUSE_SIMULATOR_H

#include <windows.h>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <random>

enum class TrajectoryType {
    LINEAR,      // 直线
    BEZIER,      // 贝塞尔曲线
    S_CURVE,     // S形曲线
    RANDOM_WALK  // 随机漫步
};

// struct POINT {
//     int x;
//     int y;
// };

class MouseSimulator {
public:
    // 构造函数/析构函数
    MouseSimulator();
    ~MouseSimulator() = default;

    // 基础移动和点击
    static bool MoveTo(int x, int y);
    static bool Click();
    static bool ClickAt(int x, int y);

    // 轨迹移动 + 点击
    bool ExecuteTrajectoryWithClick(POINT start, POINT end,
                                   TrajectoryType trajectoryType = TrajectoryType::BEZIER,
                                   int steps = 30,
                                   int delayBetweenPoints = 10);

    // 高级隐蔽点击方法
    bool StealthClick(int x, int y, bool useHybrid = true);
    bool HumanLikeClick(int x, int y, HWND targetWindow = nullptr);

    // 批量操作
    bool ExecuteClickSequence(const std::vector<POINT>& clickPoints,
                             TrajectoryType trajectoryType = TrajectoryType::BEZIER,
                             int minDelay = 500, int maxDelay = 2000);

    // 配置方法
    void SetHumanLikeMode(bool enable) { m_humanLikeMode = enable; }
    void SetRandomDelayRange(int minMs, int maxMs) {
        m_minDelay = minMs;
        m_maxDelay = maxMs;
    }
    void SetJitterLevel(int level) { m_jitterLevel = level; }

    // 工具方法
    static POINT GetCurrentPosition();
    static bool IsPositionValid(int x, int y);

    // 点击实现
    bool HardwareClick(int x, int y);
    bool MessageClick(HWND hWnd, int x, int y);
    bool StealthHardwareClick(int x, int y);
    bool StealthMessageClick(HWND hWnd, int x, int y);

    // 辅助方法
    void AddRandomMicroMovements(int targetX, int targetY);
    void PostClickBehavior();
    int GetRandomDelay();
    int GetRandomInRange(int min, int max);

private:
    // 轨迹生成
    std::vector<POINT> GenerateLinearTrajectory(POINT start, POINT end, int steps);
    std::vector<POINT> GenerateBezierTrajectory(POINT start, POINT end, int steps);
    std::vector<POINT> GenerateComplexTrajectory(POINT start, POINT end,
                                                TrajectoryType type, int steps);
    std::vector<POINT> GenerateJitterTrajectory(int startX, int startY, int endX, int endY);

    // 配置参数
    bool m_humanLikeMode = true;
    int m_minDelay = 50;
    int m_maxDelay = 200;
    int m_jitterLevel = 5;
    std::mt19937 m_randomEngine;
};

#endif // MOUSE_SIMULATOR_H