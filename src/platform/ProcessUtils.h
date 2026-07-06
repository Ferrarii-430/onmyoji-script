#ifndef PROCESSUTILS_H
#define PROCESSUTILS_H

#include <windows.h>
#include <string>

namespace platform {

// 通过进程名（不区分大小写）查找进程 PID，未找到返回 0
DWORD findPidByProcessName(const std::string& procName);

// 查找属于指定进程的第一个可见、有标题的顶层窗口
HWND findWindowByPid(DWORD pid);

// 为当前进程启用 SeDebugPrivilege
bool enableDebugPrivilege();

} // namespace platform

#endif // PROCESSUTILS_H
