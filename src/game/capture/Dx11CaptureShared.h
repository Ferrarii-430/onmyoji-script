//
// DX11 截图共享内存协议。
// 该文件的结构体布局必须与注入 DLL (onmyoji-dx11-hook/src/main.cpp) 中的
// Dx11CaptureShared 完全一致，用于 DLL 与本进程之间直接传递原始像素数据，
// 避免 PNG 落盘/解码中转。
//
#ifndef DX11_CAPTURE_SHARED_H
#define DX11_CAPTURE_SHARED_H

#include <cstdint>

// 命名 file mapping 的名称（同一会话内跨进程可见）
#define DX11_SHARED_NAME L"OnmyojiDx11CaptureShared"

// 跨进程截图请求事件名称（auto-reset）。script 进程通过 OpenEvent/SetEvent
// 直接触发 DLL 截图，无需每次都启动 remote_capture_call.exe。
#define DX11_CAPTURE_REQUEST_EVENT_NAME L"OnmyojiDx11CaptureRequest"

static const uint32_t DX11_SHARED_MAGIC = 0x31315844; // 'DX11'
static const uint32_t DX11_SHARED_VERSION = 1;

#pragma pack(push, 4)
struct Dx11CaptureShared {
    uint32_t magic;     // DX11_SHARED_MAGIC
    uint32_t version;   // DX11_SHARED_VERSION
    uint32_t sequence;  // 每次成功写入自增
    uint32_t status;    // 0 = 成功，非 0 = 无有效数据
    uint32_t width;
    uint32_t height;
    uint32_t channels;  // 固定为 4
    uint32_t dataSize;  // = width * height * 4
    // 紧随其后是后备缓冲区的原始像素（DLL 不做通道交换），top-down、每行
    // width*4 字节、无 padding。字节序即后备缓冲区格式：onmyoji 为
    // DXGI_FORMAT_R8G8B8A8_UNORM，故实际为 R,G,B,A，读取端需按 RGBA 处理。
};
#pragma pack(pop)

// ============================================================================
// 坐标点击（后台输入注入）共享内存协议。
// 该文件的结构体布局必须与注入 DLL (onmyoji-dx11-hook/src/main.cpp) 中的
// Dx11ClickCommand 完全一致。DLL 已注入游戏进程，向 swap chain 输出窗口
// PostMessage 鼠标消息实现后台点击。script 进程把 (x,y) 写入共享内存并
// SetEvent 请求事件，轮询 doneSeq 确认 DLL 已投递。
// ============================================================================
#define DX11_CLICK_SHARED_NAME L"OnmyojiDx11ClickShared"
#define DX11_CLICK_REQUEST_EVENT_NAME L"OnmyojiDx11ClickRequest"

static const uint32_t DX11_CLICK_MAGIC = 0x314B4C43; // 'CLK1'
static const uint32_t DX11_CLICK_VERSION = 1;

#pragma pack(push, 4)
struct Dx11ClickCommand {
    uint32_t magic;     // DX11_CLICK_MAGIC
    uint32_t version;   // DX11_CLICK_VERSION
    uint32_t sequence;  // 写端每次请求自增
    uint32_t doneSeq;   // DLL 执行投递后置为本次 sequence
    int32_t  x;         // 截图像素坐标（与后备缓冲区一致）
    int32_t  y;
};
#pragma pack(pop)

#endif // DX11_CAPTURE_SHARED_H
