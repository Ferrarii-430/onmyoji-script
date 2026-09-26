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

    // 截图前确保窗口宽度不小于 minWidth（基于上次截图尺寸判断）。
    // 窗口最小化时通过"恢复→移至屏幕外改宽→恢复位置→重新最小化"的无感流程调整；
    // 窗口最大化且前台显示时跳过移屏外流程，恢复普通状态后原地调整大小，
    // 避免窗口被压到其他窗口之后（表现为消失/像被最小化）。
    void ensureMinWidthForCapture(int minWidth);

    // 在窗口内执行点击（clickPoint 为截图坐标系）。
    // clickContext 用于日志标注本次点击对象（如 YOLO标签/OCR文字/OpenCV模板/ROI区域），
    // 为空时日志显示通用的「点击成功/点击失败」
    void clickInWindow(const cv::Point& clickPoint, const QString& clickContext = QString());

    // 将截图坐标映射为窗口客户区坐标
    cv::Point mapCapturePointToClient(const cv::Point& capturePoint);

    // 向窗口发送一次按键（按下 + 抬起）
    void postKey(UINT virtualKey);

private:
    GameWindow() = default;

    std::string target_ = "onmyoji.exe";
    HWND hwnd_ = nullptr;
    cv::Size lastCaptureSize_;

    // 上次已确认达标时的 minWidth：相同值且截图尺寸未变小时，
    // ensureMinWidthForCapture 走快速路径跳过逐帧窗口尺寸系统调用
    int lastEnsuredMinWidth_ = 0;
};

#endif // GAMEWINDOW_H
