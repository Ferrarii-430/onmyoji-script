#include "src/platform/ProcessUtils.h"

#include <tlhelp32.h>
#include <algorithm>
#include <atomic>
#include <cwctype>

namespace platform {

namespace {

// 进程名比较：统一转小写后全等匹配（与原有实现的行为保持一致）
bool equalsIgnoreCase(const std::wstring& lhs, const std::wstring& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (std::towlower(lhs[i]) != std::towlower(rhs[i])) {
            return false;
        }
    }
    return true;
}

// 进程名只可能是 ASCII（exe 文件名），按字节直接扩展即可
std::wstring widenAscii(const std::string& text)
{
    return std::wstring(text.begin(), text.end());
}

// 系统外壳窗口类名：桌面、任务栏、开始菜单等。
// 点选时如果把这些当成候选，用户点到空白处就会绑到 explorer 上。
bool isShellWindowClass(const std::wstring& className)
{
    static const wchar_t* kShellClasses[] = {
        L"Progman",
        L"WorkerW",
        L"Shell_TrayWnd",
        L"Shell_SecondaryTrayWnd",
        L"DV2ControlHost",
        L"Windows.UI.Core.CoreWindow",
        L"ApplicationFrameWindow"
    };

    for (const wchar_t* candidate : kShellClasses) {
        if (className == candidate) {
            return true;
        }
    }
    return false;
}

} // namespace

DWORD findPidByProcessName(const std::string& procName)
{
    // 单实例语义保持不变：多开时取 PID 最小的那个，与旧实现的
    // 「快照枚举顺序中第一个匹配项」在绝大多数情况下一致
    const std::vector<DWORD> pids = findAllPidsByProcessName(procName);
    return pids.empty() ? 0 : pids.front();
}

std::vector<DWORD> findAllPidsByProcessName(const std::string& procName)
{
    std::vector<DWORD> pids;

    const std::wstring wanted = widenAscii(procName);
    if (wanted.empty()) {
        return pids;
    }

    PROCESSENTRY32W pe = { 0 };
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return pids;

    if (Process32FirstW(snap, &pe)) {
        do {
            if (equalsIgnoreCase(pe.szExeFile, wanted)) {
                pids.push_back(pe.th32ProcessID);
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    // 快照枚举顺序不稳定，排序后界面展示与绑定顺序才可复现
    std::sort(pids.begin(), pids.end());
    return pids;
}

HWND findWindowByPid(DWORD pid)
{
    // 一个进程常有多个可见带标题窗口（启动器、悬浮层等），旧实现取“第一个”
    // 很容易命中尺寸极小的辅助窗口，导致后续按客户区尺寸做坐标换算时坍缩为 (0,0)。
    // 改为在该进程的所有顶层窗口里挑客户区面积最大的那个（真正的游戏主窗口）。
    // 优先：无 owner 的最大窗口；回退：任意可见窗口里最大的，保证总能找到
    struct Ctx {
        DWORD pid;
        HWND best = nullptr;      // 无 owner 窗口中客户区最大者
        long bestArea = 0;
        HWND fallback = nullptr;  // 所有可见窗口中客户区最大者
        long fallbackArea = -1;
    };
    Ctx ctx{pid};

    auto enumProc = [](HWND hwnd, LPARAM lparam) -> BOOL {
        Ctx* p = (Ctx*)lparam;
        DWORD wpid = 0;
        GetWindowThreadProcessId(hwnd, &wpid);
        if (wpid != p->pid) return TRUE;
        if (!IsWindowVisible(hwnd)) return TRUE;

        RECT rc{};
        GetClientRect(hwnd, &rc);
        const long area = static_cast<long>(rc.right) * static_cast<long>(rc.bottom);

        if (area > p->fallbackArea) {
            p->fallbackArea = area;
            p->fallback = hwnd;
        }
        if (GetWindow(hwnd, GW_OWNER) == nullptr && area > p->bestArea) {
            p->bestArea = area;
            p->best = hwnd;
        }
        return TRUE;
    };

    EnumWindows(enumProc, (LPARAM)&ctx);
    return ctx.best ? ctx.best : ctx.fallback;
}

std::wstring getProcessImagePath(DWORD pid)
{
    if (pid == 0) {
        return {};
    }

    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) {
        return {};
    }

    wchar_t buffer[MAX_PATH * 2] = { 0 };
    DWORD size = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    std::wstring result;
    if (QueryFullProcessImageNameW(proc, 0, buffer, &size)) {
        result.assign(buffer, size);
    }
    CloseHandle(proc);
    return result;
}

std::wstring getProcessImageName(DWORD pid)
{
    const std::wstring path = getProcessImagePath(pid);
    if (path.empty()) {
        return {};
    }
    const size_t pos = path.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? path : path.substr(pos + 1);
}

std::wstring getWindowTitle(HWND hwnd)
{
    if (!IsWindow(hwnd)) {
        return {};
    }

    const int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) {
        return {};
    }

    std::wstring title(static_cast<size_t>(len) + 1, L'\0');
    const int copied = GetWindowTextW(hwnd, title.data(), len + 1);
    if (copied <= 0) {
        return {};
    }
    title.resize(static_cast<size_t>(copied));
    return title;
}

std::wstring getWindowClassName(HWND hwnd)
{
    if (!IsWindow(hwnd)) {
        return {};
    }

    wchar_t buffer[256] = { 0 };
    const int len = GetClassNameW(hwnd, buffer, static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
    if (len <= 0) {
        return {};
    }
    return std::wstring(buffer, static_cast<size_t>(len));
}

bool getClientSize(HWND hwnd, int* width, int* height)
{
    if (!IsWindow(hwnd)) {
        return false;
    }

    RECT rc{};
    if (!GetClientRect(hwnd, &rc)) {
        return false;
    }

    if (width) {
        *width = static_cast<int>(rc.right - rc.left);
    }
    if (height) {
        *height = static_cast<int>(rc.bottom - rc.top);
    }
    return true;
}

bool isProcessAlive(DWORD pid)
{
    if (pid == 0) {
        return false;
    }

    HANDLE proc = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) {
        return false;
    }

    // 句柄可打开但进程已退出时，信号态立即可等待到
    const DWORD wait = WaitForSingleObject(proc, 0);
    CloseHandle(proc);
    return wait == WAIT_TIMEOUT;
}

bool isWindowAlive(HWND hwnd)
{
    if (!IsWindow(hwnd)) {
        return false;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid != 0 && isProcessAlive(pid);
}

HWND findTopLevelWindowAtPoint(POINT screenPoint, const std::string& processName)
{
    const DWORD selfPid = GetCurrentProcessId();
    const std::wstring wanted = widenAscii(processName);

    // WindowFromPoint 返回最顶层的窗口（含子控件），提升到顶层后沿 Z 序向下遍历。
    // 点选遮罩属于本进程，必须跳过，否则永远只能选中遮罩自己。
    //
    // 进程名匹配只作为「优先」条件：OpenProcess 在权限不足时必然失败，
    // 若把它当成硬性过滤，用户会表现为「点了没任何反应」。这里保留
    // fallback，让调用方至少能拿到候选窗口并显示高亮。
    HWND fallback = nullptr;
    HWND hwnd = GetAncestor(WindowFromPoint(screenPoint), GA_ROOT);
    while (hwnd) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);

        if (pid != 0 && pid != selfPid && IsWindowVisible(hwnd)
            && !isShellWindowClass(getWindowClassName(hwnd))) {
            if (wanted.empty()) {
                return hwnd;
            }

            const std::wstring imageName = getProcessImageName(pid);
            if (!imageName.empty() && equalsIgnoreCase(imageName, wanted)) {
                return hwnd;
            }

            if (fallback == nullptr) {
                fallback = hwnd;
            }
        }

        hwnd = GetWindow(hwnd, GW_HWNDNEXT);
    }
    return fallback;
}

bool enableDebugPrivilege()
{
    // 提权成功一次后进程内不会失效，缓存结果避免截图热路径每帧重复
    // OpenProcessToken/AdjustTokenPrivileges 系统调用（调用方每次截图都会调用）。
    // 多会话并发时各任务线程都会走到这里，用原子量避免数据竞争
    static std::atomic<bool> cached{false};
    if (cached.load(std::memory_order_relaxed)) {
        return true;
    }

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
    const bool ok = GetLastError() == ERROR_SUCCESS;
    cached.store(ok, std::memory_order_relaxed);
    return ok;
}

} // namespace platform
