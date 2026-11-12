#include "src/game/GameContext.h"

#include "src/game/capture/Dx11CaptureShared.h"

namespace game {

namespace {

std::wstring makePidName(const wchar_t* prefix, DWORD pid)
{
    return std::wstring(prefix) + L"_" + std::to_wstring(pid);
}

} // namespace

std::wstring captureSharedName(DWORD pid)
{
    return makePidName(DX11_SHARED_NAME_PREFIX, pid);
}

std::wstring captureRequestEventName(DWORD pid)
{
    return makePidName(DX11_CAPTURE_REQUEST_EVENT_PREFIX, pid);
}

std::wstring captureReadyEventName(DWORD pid)
{
    return makePidName(DX11_CAPTURE_READY_EVENT_PREFIX, pid);
}

std::wstring clickSharedName(DWORD pid)
{
    return makePidName(DX11_CLICK_SHARED_PREFIX, pid);
}

std::wstring clickRequestEventName(DWORD pid)
{
    return makePidName(DX11_CLICK_REQUEST_EVENT_PREFIX, pid);
}

} // namespace game
