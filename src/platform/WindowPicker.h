#ifndef WINDOWPICKER_H
#define WINDOWPICKER_H

#include <windows.h>
#include <QString>

namespace platform {

// 点选捕获的结果快照。绑定后仅使用 hwnd/pid，其余字段供界面展示与日志。
struct PickedWindow {
    HWND hwnd = nullptr;
    DWORD pid = 0;
    QString title;         // 窗口标题
    QString imageName;     // 进程可执行文件名，如 onmyoji.exe
    QString imagePath;     // 可执行文件完整路径
    int clientWidth = 0;
    int clientHeight = 0;

    bool valid() const { return hwnd != nullptr && pid != 0; }
};

// 采集指定窗口的展示信息（标题/进程/客户区尺寸）
PickedWindow describeWindow(HWND hwnd);

// 模态点选：全屏遮罩 + 鼠标下的目标窗口高亮显示，
// 左键确认，右键或 ESC 取消。
// processName 为空表示接受任意进程的窗口；本脚本进程自身的窗口始终被排除。
// 返回 false 表示用户取消或未命中有效窗口，此时 out 内容未被修改。
bool pickWindow(const QString& processName, PickedWindow* out);

} // namespace platform

#endif // WINDOWPICKER_H
