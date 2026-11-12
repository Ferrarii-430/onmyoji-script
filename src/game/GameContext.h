#ifndef GAMECONTEXT_H
#define GAMECONTEXT_H

#include <windows.h>
#include <QString>
#include <string>

// 单个游戏实例的运行上下文。
// 每个会话（标签页）持有一份，启动任务时固定下来传给 TaskRunner；
// 任务线程据此绑定 GameWindow、拼接跨进程命名对象名、给日志打会话标签。
struct GameContext {
    DWORD pid = 0;
    HWND hwnd = nullptr;
    std::string processName = "onmyoji.exe";

    // 会话标识：日志分发据此把输出投递到对应标签页
    QString logTag;
    // 界面展示名（窗口标题或进程名）
    QString displayName;

    bool isValid() const { return pid != 0; }
};

namespace game {

// 跨进程命名对象名 = 固定前缀 + "_" + 目标进程 PID。
// 注入 DLL 用 GetCurrentProcessId() 生成同样的后缀（见 dx11_hook/src/main.cpp），
// 多开时每个游戏进程各自一套共享内存与事件，互不覆写。
std::wstring captureSharedName(DWORD pid);
std::wstring captureRequestEventName(DWORD pid);
std::wstring captureReadyEventName(DWORD pid);
std::wstring clickSharedName(DWORD pid);
std::wstring clickRequestEventName(DWORD pid);

} // namespace game

#endif // GAMECONTEXT_H
