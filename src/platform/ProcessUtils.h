#ifndef PROCESSUTILS_H
#define PROCESSUTILS_H

#include <windows.h>
#include <string>
#include <vector>

namespace platform {

// 通过进程名（不区分大小写）查找进程 PID，未找到返回 0。
// 多开场景请改用 findAllPidsByProcessName，该函数只返回第一个匹配项。
DWORD findPidByProcessName(const std::string& procName);

// 通过进程名（不区分大小写）查找全部匹配的 PID，按 PID 升序返回。
// 多开时同一进程名会存在多个实例，窗口绑定需要区分它们。
std::vector<DWORD> findAllPidsByProcessName(const std::string& procName);

// 查找属于指定进程的第一个可见、有标题的顶层窗口
HWND findWindowByPid(DWORD pid);

// 获取进程可执行文件完整路径；进程不存在或权限不足时返回空字符串
std::wstring getProcessImagePath(DWORD pid);

// 取可执行文件名（不含目录），如 "onmyoji.exe"；失败返回空
std::wstring getProcessImageName(DWORD pid);

// 获取窗口标题文本；失败返回空
std::wstring getWindowTitle(HWND hwnd);

// 获取窗口类名；失败返回空
std::wstring getWindowClassName(HWND hwnd);

// 获取窗口客户区尺寸（像素）；句柄无效返回 false
bool getClientSize(HWND hwnd, int* width, int* height);

// 进程是否仍存活（用于校验已绑定窗口是否失效）
bool isProcessAlive(DWORD pid);

// 判断窗口句柄是否仍有效、可见且属于存活进程
bool isWindowAlive(HWND hwnd);

// 屏幕坐标命中的顶层窗口。
// 会跳过当前进程自身的窗口（点选遮罩）、不可见窗口与系统外壳窗口。
// processName 非空时优先返回该进程名的窗口；但进程名无法读取（权限不足）时
// 不会阻断，仍返回最顶层的候选窗口 —— 点选场景下宁可让用户看到高亮再确认，
// 也不能因为 OpenProcess 失败就永远选不中。
// 未命中返回 nullptr。
HWND findTopLevelWindowAtPoint(POINT screenPoint, const std::string& processName);

// 为当前进程启用 SeDebugPrivilege
bool enableDebugPrivilege();

} // namespace platform

#endif // PROCESSUTILS_H
