#ifndef WINDOWCAPTURE_H
#define WINDOWCAPTURE_H

#include <windows.h>
#include <opencv2/core/mat.hpp>

namespace capture {

// 使用 PrintWindow / BitBlt 将窗口内容捕获为 BGR 图像
bool captureWindowToMat(HWND hwnd, cv::Mat& outBGR);

// 带重试的 PrintWindow 截图（最多 5 次，每次间隔 1 秒）
bool captureByPrintWindow(HWND hwnd, cv::Mat& winImg);

} // namespace capture

#endif // WINDOWCAPTURE_H
