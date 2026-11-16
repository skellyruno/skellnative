#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cstdint>

namespace spacer_capture {
using namespace Steinberg;
using namespace Steinberg::Vst;

inline void pump(unsigned timeMs) {
    const auto end = GetTickCount64() + timeMs;
    do {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(2);
    } while (GetTickCount64() < end);
}

class Surface {
public:
    Surface(int width, int height) : width_(width), height_(height) {
        window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"Spacer capture",
                                 WS_POPUP | WS_CLIPCHILDREN, 48, 48, width, height, nullptr,
                                 nullptr, GetModuleHandleW(nullptr), nullptr);
        require(window != nullptr, "Create screenshot parent");
    }
    ~Surface() { DestroyWindow(window); }

    void capture(const std::filesystem::path& path, bool renderEditor) const {
        const bool offscreen =
            48 + width_ > GetSystemMetrics(SM_CXSCREEN) ||
            48 + height_ > GetSystemMetrics(SM_CYSCREEN);
        if (!renderEditor && !offscreen) {
            ShowWindow(window, SW_SHOWNOACTIVATE);
            SetWindowPos(window, HWND_TOPMOST, 48, 48, width_, height_, SWP_NOACTIVATE);
            RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
            pump(160);
        }
        auto source = GetDC(nullptr);
        auto dest = CreateCompatibleDC(source);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width_;
        info.bmiHeader.biHeight = -height_;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        auto bitmap = CreateDIBSection(dest, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        auto old = bitmap ? SelectObject(dest, bitmap) : nullptr;
        require(source && dest && bitmap && old && bits, "Allocate real screenshot surface");
        // Paint native editors off-screen so desktop windows and the taskbar
        // cannot contaminate captures, even when the size fits the display.
        bool copied = false;
        if (renderEditor || offscreen) {
            auto child = GetWindow(window, GW_CHILD);
            require(child != nullptr, "Real editor child required for offscreen capture");
            const auto pixels = static_cast<std::size_t>(width_) * height_;
            std::memset(bits, 0xA5, pixels * sizeof(std::uint32_t));
            SendMessageW(child, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dest), PRF_CLIENT);
            const auto* raster = static_cast<const std::uint32_t*>(bits);
            copied = std::find(raster, raster + pixels, 0xA5A5A5A5u) == raster + pixels;
        } else {
            copied = BitBlt(dest, 0, 0, width_, height_, source, 48, 48, SRCCOPY) != 0;
        }
        BITMAPFILEHEADER header{};
        header.bfType = 0x4d42;
        header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader);
        const auto bytes = static_cast<DWORD>(width_ * height_ * 4);
        header.bfSize = header.bfOffBits + bytes;
        std::ofstream out(path, std::ios::binary);
        if (copied && bits) {
            out.write(reinterpret_cast<const char*>(&header), sizeof(header));
            out.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
            out.write(static_cast<const char*>(bits), bytes);
        }
        SelectObject(dest, old);
        DeleteObject(bitmap);
        DeleteDC(dest);
        ReleaseDC(nullptr, source);
        require(copied && bits && out.good(), "Save screenshot");
    }

    HWND window;
private:
    int width_, height_;
};

inline void capture(IEditController& controller, const std::filesystem::path& output, bool cycleThemes,
                    int width = 495, int height = 405, bool renderEditor = false) {
    std::filesystem::create_directories(output.parent_path());
    Surface parent(width, height);
    {
        const auto view = owned(controller.createView(ViewType::kEditor));
        require(view != nullptr && view->attached(parent.window, kPlatformTypeHWND) == kResultOk,
                "Attach screenshot editor");
        if (FUnknownPtr<IPlugViewContentScaleSupport> dpi{view})
            require(dpi->setContentScaleFactor(1.f) == kResultOk, "Set 100% editor scale");
        ViewRect rect{0, 0, width, height};
        require(view->onSize(&rect) == kResultOk, "Resize screenshot editor");
        pump(80);
        const auto child = GetWindow(parent.window, GW_CHILD);
        RECT childBounds{};
        require(child && GetClientRect(child, &childBounds)
                    && childBounds.right == width && childBounds.bottom == height,
                "Editor client size differs from capture size");
        if (renderEditor)
            require(SendMessageW(child, WM_NCHITTEST, 0, 0) != HTTRANSPARENT,
                    "Editor HWND is transparent to pointer events");
        for (int i = 0; i < (cycleThemes ? 4 : 1); ++i) {
            auto path = output;
            path += "-" + std::to_string(i) + ".bmp";
            parent.capture(path, renderEditor);
            if (cycleThemes) {
                const auto point = MAKELPARAM(25, 24);
                SendMessageW(child, WM_LBUTTONDOWN, MK_LBUTTON, point);
                SendMessageW(child, WM_LBUTTONUP, 0, point);
                pump(30);
            }
        }
        require(view->removed() == kResultOk, "Remove screenshot editor");
    }
    require(GetWindow(parent.window, GW_CHILD) == nullptr, "Stale editor child after removal");
}

inline void captureScales(IEditController& controller, const std::filesystem::path& output,
                          int width, int height) {
    for (const int percent : {80, 100, 125, 150, 200}) {
        auto prefix = output;
        prefix += "-" + std::to_string(percent) + "percent";
        capture(controller, prefix, false, (width * percent + 50) / 100,
                (height * percent + 50) / 100, true);
    }
}
} // namespace spacer_capture
