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
    struct Ctx { DWORD pid; HWND res = nullptr; };
    Ctx ctx{pid};

    auto enumProc = [](HWND hwnd, LPARAM lparam) -> BOOL {
        Ctx* p = (Ctx*)lparam;
        DWORD wpid = 0;
        GetWindowThreadProcessId(hwnd, &wpid);
        if (wpid != p->pid) return TRUE;
        if (!IsWindowVisible(hwnd)) return TRUE;
        wchar_t buf[256] = {};
        GetWindowTextW(hwnd, buf, sizeof(buf)/sizeof(wchar_t));
        if (wcslen(buf) == 0) return TRUE;
        p->res = hwnd;
        return FALSE;
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
