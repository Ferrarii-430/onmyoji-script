#include "src/platform/ProcessUtils.h"

#include <tlhelp32.h>
#include <algorithm>

namespace platform {

DWORD findPidByProcessName(const std::string& procName)
{
    PROCESSENTRY32W pe = { 0 };
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring exe(pe.szExeFile);
            std::wstring lowerExe = exe;
            std::transform(lowerExe.begin(), lowerExe.end(), lowerExe.begin(), ::towlower);

            std::wstring lowerWanted(procName.begin(), procName.end());
            std::transform(lowerWanted.begin(), lowerWanted.end(), lowerWanted.begin(), ::towlower);

            if (lowerExe == lowerWanted) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

HWND findWindowByPid(DWORD pid)
{
    // 一个进程常有多个可见带标题窗口（启动器、悬浮层等），旧实现取“第一个”
    // 很容易命中尺寸极小的辅助窗口，导致后续按客户区尺寸做坐标换算时坍缩为 (0,0)。
    // 改为在该进程的所有顶层窗口里挑客户区面积最大的那个（真正的游戏主窗口）。
    struct Ctx { DWORD pid; HWND res = nullptr; long bestArea = 0; };
    Ctx ctx{pid};

    auto enumProc = [](HWND hwnd, LPARAM lparam) -> BOOL {
        Ctx* p = (Ctx*)lparam;
        DWORD wpid = 0;
        GetWindowThreadProcessId(hwnd, &wpid);
        if (wpid != p->pid) return TRUE;
        if (!IsWindowVisible(hwnd)) return TRUE;
        if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE; // 跳过归属窗口（弹窗/工具窗）

        RECT rc{};
        if (!GetClientRect(hwnd, &rc)) return TRUE;
        const long area = static_cast<long>(rc.right) * static_cast<long>(rc.bottom);
        if (area > p->bestArea) {
            p->bestArea = area;
            p->res = hwnd;
        }
        return TRUE;
    };

    EnumWindows(enumProc, (LPARAM)&ctx);
    return ctx.res;
}

bool enableDebugPrivilege()
{
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        CloseHandle(hToken);
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL)) {
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return GetLastError() == ERROR_SUCCESS;
}

} // namespace platform
