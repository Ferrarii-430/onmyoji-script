#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <windows.h>
#include <QString>
#include <string>
#include <opencv2/core/types.hpp>

// 游戏窗口管理：负责定位游戏进程/窗口句柄、
// 截图坐标到客户区坐标的映射、以及对窗口的点击/按键输入。
class GameWindow
{
public:
    static GameWindow& instance();

    GameWindow(const GameWindow&) = delete;
    GameWindow& operator=(const GameWindow&) = delete;

    // 根据进程名重新定位游戏窗口句柄，成功返回 true
    bool locate();

    HWND handle() const { return hwnd_; }
    const std::string& targetProcessName() const { return target_; }

    // 获取窗口所属进程 PID（字符串形式），失败返回空
    QString processId();

    // 最近一次截图的尺寸（用于截图坐标 -> 客户区坐标映射）
    void setLastCaptureSize(const cv::Size& size) { lastCaptureSize_ = size; }
    cv::Size lastCaptureSize() const { return lastCaptureSize_; }

    // 在窗口内执行点击（clickPoint 为截图坐标系）
    void clickInWindow(const cv::Point& clickPoint);

    // 将截图坐标映射为窗口客户区坐标
    cv::Point mapCapturePointToClient(const cv::Point& capturePoint);

    // 向窗口发送一次按键（按下 + 抬起）
    void postKey(UINT virtualKey);

private:
    GameWindow() = default;

    std::string target_ = "onmyoji.exe";
    HWND hwnd_ = nullptr;
    cv::Size lastCaptureSize_;
};

#endif // GAMEWINDOW_H
