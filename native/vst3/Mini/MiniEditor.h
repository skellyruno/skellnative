#pragma once
// Skell Mini Native DEV editor. Real Win32 child HWND; no JUCE or VSTGUI.
// All GDI allocation is strictly on the UI thread; audio callbacks never draw.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "public.sdk/source/common/pluginview.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <SkellNative/Controls.h>
#include <SkellDraw/Win32FlatControls.h>
#include <windowsx.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace Skelly::NativeMini {
class MiniEditor final : public Steinberg::CPluginView {
public:
    MiniEditor(Steinberg::Vst::EditController& controller,
               const skellnative::sdk::NativeControl* controls, std::size_t count)
        : controller_(controller), controls_(controls), count_(count) {
        controller_.addRef();
        last_.fill(std::numeric_limits<float>::quiet_NaN());
        setRect(Steinberg::ViewRect(0, 0, kWidth, kHeight));
    }
    ~MiniEditor() override {
        removed();
        controller_.release();
    }
    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString platform) override {
        return platform && std::strcmp(platform, Steinberg::kPlatformTypeHWND) == 0
            ? Steinberg::kResultOk : Steinberg::kResultFalse;
    }
    Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString platform) override {
        if (window_ || !parent || isPlatformTypeSupported(platform) != Steinberg::kResultOk)
            return Steinberg::kResultFalse;
        const auto& size = getRect();
        HWND handle = CreateWindowExW(0, L"STATIC", L"Skell Mini",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS, 0, 0,
            size.getWidth(), size.getHeight(), static_cast<HWND>(parent),
            nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!handle) return Steinberg::kResultFalse;
        if (!SetPropW(handle, kProperty, this)) {
            DestroyWindow(handle);
            return Steinberg::kResultFalse;
        }
        SetLastError(0);
        const auto original = SetWindowLongPtrW(handle, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&MiniEditor::windowProc));
        if (!original && GetLastError()) {
            RemovePropW(handle, kProperty);
            DestroyWindow(handle);
            return Steinberg::kResultFalse;
        }
        previous_ = reinterpret_cast<WNDPROC>(original);
        window_ = handle;
        SetTimer(handle, kTimer, 50, nullptr);
        InvalidateRect(handle, nullptr, FALSE);
        return Steinberg::CPluginView::attached(parent, platform);
    }
    Steinberg::tresult PLUGIN_API removed() override {
        if (window_) {
            finishDrag();
            KillTimer(window_, kTimer);
            SetWindowLongPtrW(window_, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(previous_));
            RemovePropW(window_, kProperty);
            DestroyWindow(window_);
            window_ = nullptr;
        }
        releaseBuffer();
        return Steinberg::CPluginView::removed();
    }
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* rect) override {
        if (!rect || rect->getWidth() < 1 || rect->getHeight() < 1)
            return Steinberg::kInvalidArgument;
        const auto result = Steinberg::CPluginView::onSize(rect);
        if (window_) {
            SetWindowPos(window_, nullptr, 0, 0, rect->getWidth(),
                rect->getHeight(), SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(window_, nullptr, FALSE);
        }
        return result;
    }
    Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultTrue; }
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* rect) override {
        if (!rect) return Steinberg::kInvalidArgument;
        const auto width = std::clamp(rect->getWidth(), 384, 960);
        rect->right = rect->left + width;
        rect->bottom = rect->top + static_cast<int>(std::lround(
            static_cast<double>(width) * kHeight / kWidth));
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API onWheel(float distance) override {
        if (hover_ >= 0 && hover_ < static_cast<int>(std::min(count_, std::size_t{4}))) {
            const auto index = static_cast<std::size_t>(hover_);
            const float value = controller_.getParamNormalized(controls_[index].id);
            gestureOnce(index, std::clamp(value + distance * .025f, 0.f, 1.f));
            return Steinberg::kResultOk;
        }
        return Steinberg::kResultFalse;
    }

private:
    static constexpr int kWidth = 480, kHeight = 290;
    static constexpr UINT_PTR kTimer = 18;
    static constexpr wchar_t kProperty[] = L"SkellMiniNativeSkellDrawView";
    static constexpr skelldraw::Rect kBypass{346, 45, 112, 27};
    static constexpr skelldraw::Rect knobRect(int i) {
        return {24 + 116 * i, 116, 87, 87};
    }
    static constexpr skelldraw::win32::FlatPalette kPalette{
        0x080b09, 0x111a13, 0x142017, 0x35503c, 0xeff7ed,
        0xa4b5a6, 0x617267, 0x74ff68, 0x071008, 0x1e3525, 0x335f37
    };
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
        auto* editor = static_cast<MiniEditor*>(GetPropW(hwnd, kProperty));
        return editor ? editor->dispatch(hwnd, message, w, l)
                      : DefWindowProcW(hwnd, message, w, l);
    }
    skelldraw::Point logical(LPARAM l) const noexcept {
        return {GET_X_LPARAM(l) * kWidth / std::max(1, getRect().getWidth()),
                GET_Y_LPARAM(l) * kHeight / std::max(1, getRect().getHeight())};
    }
    int hit(skelldraw::Point point) const noexcept {
        if (count_ > 4 && kBypass.contains(point)) return 4;
        for (int i = 0; i < static_cast<int>(std::min(count_, std::size_t{4})); ++i)
            if (knobRect(i).contains(point)) return i;
        return -1;
    }
    float normalized(std::size_t index) const noexcept {
        return static_cast<float>(controller_.getParamNormalized(controls_[index].id));
    }
    void edit(std::size_t index, float value) noexcept {
        const auto snapped = controls_[index].canonical(value);
        controller_.setParamNormalized(controls_[index].id, snapped);
        controller_.performEdit(controls_[index].id, snapped);
        if (window_) InvalidateRect(window_, nullptr, FALSE);
    }
    void gestureOnce(std::size_t index, float value) noexcept {
        controller_.beginEdit(controls_[index].id);
        edit(index, value);
        controller_.endEdit(controls_[index].id);
    }
    void finishDrag() noexcept {
        if (drag_ >= 0) {
            controller_.endEdit(controls_[static_cast<std::size_t>(drag_)].id);
            drag_ = -1;
        }
        if (GetCapture() == window_) ReleaseCapture();
    }
    LRESULT dispatch(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
        const auto point = logical(l);
        switch (message) {
            case WM_ERASEBKGND: return 1;
            case WM_PAINT: {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(hwnd, &ps);
                if (dc) { paint(dc); EndPaint(hwnd, &ps); }
                return 0;
            }
            case WM_PRINTCLIENT:
                paint(reinterpret_cast<HDC>(w));
                return 0;
            case WM_TIMER:
                if (w == kTimer) {
                    bool changed = false;
                    for (std::size_t i = 0; i < std::min(count_, std::size_t{5}); ++i) {
                        const float n = normalized(i);
                        if (n != last_[i]) { changed = true; last_[i] = n; }
                    }
                    if (changed) InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                break;
            case WM_LBUTTONDOWN: {
                const int i = hit(point);
                if (i < 0) break;
                SetFocus(hwnd);
                if (i == 4) { gestureOnce(4, normalized(4) < .5f ? 1.f : 0.f); return 0; }
                drag_ = i;
                startY_ = point.y;
                startN_ = normalized(static_cast<std::size_t>(i));
                controller_.beginEdit(controls_[i].id);
                SetCapture(hwnd);
                return 0;
            }
            case WM_MOUSEMOVE:
                hover_ = hit(point);
                if (drag_ >= 0) {
                    edit(static_cast<std::size_t>(drag_), std::clamp(
                        startN_ + static_cast<float>(startY_ - point.y) / 220.f, 0.f, 1.f));
                }
                return 0;
            case WM_LBUTTONUP: finishDrag(); return 0;
            case WM_CAPTURECHANGED: if (drag_ >= 0) finishDrag(); return 0;
            case WM_LBUTTONDBLCLK: {
                const int i = hit(point);
                if (i >= 0 && i < 4)
                    gestureOnce(static_cast<std::size_t>(i),
                        controls_[i].normalized(controls_[i].initial));
                return 0;
            }
            case WM_MOUSEWHEEL:
                onWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(w)) / WHEEL_DELTA);
                return 0;
            default: break;
        }
        return CallWindowProcW(previous_, hwnd, message, w, l);
    }
    void releaseBuffer() noexcept {
        if (memoryDc_) {
            SelectObject(memoryDc_, oldBitmap_);
            DeleteObject(bitmap_);
            DeleteDC(memoryDc_);
        }
        memoryDc_ = nullptr; bitmap_ = nullptr; oldBitmap_ = nullptr;
        bitmapWidth_ = 0; bitmapHeight_ = 0;
    }
    void paint(HDC dc) noexcept {
        if (!dc || !window_) return;
        RECT client{};
        if (!GetClientRect(window_, &client)) return;
        const int width = client.right, height = client.bottom;
        if (width < 1 || height < 1) return;
        if (!memoryDc_ || width != bitmapWidth_ || height != bitmapHeight_) {
            releaseBuffer();
            memoryDc_ = CreateCompatibleDC(dc);
            bitmap_ = CreateCompatibleBitmap(dc, width, height);
            if (!memoryDc_ || !bitmap_) { releaseBuffer(); return; }
            oldBitmap_ = SelectObject(memoryDc_, bitmap_);
            bitmapWidth_ = width; bitmapHeight_ = height;
        }
        const int saved = SaveDC(memoryDc_);
        SetMapMode(memoryDc_, MM_ANISOTROPIC);
        SetWindowExtEx(memoryDc_, kWidth, kHeight, nullptr);
        SetViewportExtEx(memoryDc_, width, height, nullptr);
        const skelldraw::win32::FlatPainter p(memoryDc_, kPalette);
        p.rect({0, 0, kWidth, kHeight}, kPalette.background);
        p.card({12, 13, 456, 266});
        p.text(L"SKELL", {26, 25, 88, 32}, 22, true, kPalette.text, DT_LEFT);
        p.text(L"MINI", {118, 25, 76, 32}, 22, true, kPalette.accent, DT_LEFT);
        p.text(L"NATIVE  /  DEV", {27, 64, 230, 20}, 11, false, kPalette.muted, DT_LEFT);
        p.segment(26, 97, 454, 97, kPalette.line);
        constexpr const wchar_t* titles[] = {L"RETUNE", L"AMOUNT", L"WET MIX", L"A4"};
        for (std::size_t i = 0; i < std::min(count_, std::size_t{4}); ++i) {
            const auto rect = knobRect(static_cast<int>(i));
            wchar_t value[32]{};
            const float n = normalized(i);
            const float plain = controls_[i].plain(n);
            std::swprintf(value, 32, i == 3 ? L"%.0f Hz" : i == 0 ? L"%.0f ms" : L"%.0f%%", static_cast<double>(plain));
            p.rotary(rect, n, false, static_cast<int>(i) == hover_, static_cast<int>(i) == drag_, value);
            p.text(titles[i], {rect.x - 2, 216, rect.width + 4, 23},
                   11, true, kPalette.text);
        }
        if (count_ > 4) {
            p.button(kBypass, L"BYPASS", normalized(4) >= .5f, hover_ == 4, false);
        }
        p.text(L"PINNED VST3  /  SKELLDRAW", {27, 253, 427, 16}, 10,
               false, kPalette.muted, DT_LEFT);
        RestoreDC(memoryDc_, saved);
        BitBlt(dc, 0, 0, width, height, memoryDc_, 0, 0, SRCCOPY);
    }
    Steinberg::Vst::EditController& controller_;
    const skellnative::sdk::NativeControl* controls_;
    std::size_t count_;
    std::array<float, 5> last_{};
    HWND window_ = nullptr;
    WNDPROC previous_ = nullptr;
    HDC memoryDc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    int bitmapWidth_ = 0, bitmapHeight_ = 0;
    int hover_ = -1, drag_ = -1, startY_ = 0;
    float startN_ = 0.f;
};
} // namespace Skelly::NativeMini
