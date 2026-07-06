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
    uint32_t channels;  // 固定为 4 (BGRA)
    uint32_t dataSize;  // = width * height * 4
    // 紧随其后是像素数据（BGRA, top-down, 每行 width*4 字节，无 padding）
};
#pragma pack(pop)

#endif // DX11_CAPTURE_SHARED_H
