#ifndef DX11HOOKCAPTURE_H
#define DX11HOOKCAPTURE_H

#include <QString>
#include <opencv2/core/mat.hpp>

namespace capture {

// 通过注入的 DX11 hook DLL 截图（优先读取共享内存，落盘 PNG 作为回退）
bool captureByDllInjection(const QString& targetPid, cv::Mat& winImg);

// 修改注入 DLL 的日志输出路径
bool dllSetLogPath(const QString& targetPid);

// 停止/卸载注入的 DX11 hook
bool dllStopHook(const QString& targetPid);

} // namespace capture

#endif // DX11HOOKCAPTURE_H
