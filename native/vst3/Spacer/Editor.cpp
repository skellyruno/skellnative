#include "Editor.h"
#include "pluginterfaces/base/keycodes.h"
#include "pluginterfaces/vst/vsttypes.h"
#include <windowsx.h>
#include <cwchar>
#include <cstring>

namespace Skelly::NativeSpacer {
using namespace Steinberg;
using skelldraw::Rect;
using skelldraw::Point;
using skelldraw::win32::FlatPainter;
using skelldraw::win32::FlatPalette;

namespace {
constexpr int designWidth = 550, designHeight = 450, logo = parameterCount + 1;
constexpr UINT_PTR pollId = 17;
constexpr wchar_t property[] = L"SkellSpacerNativeEditor";
constexpr Rect band{26, 392, 498, 34}, graph{28, 90, 494, 152};

constexpr FlatPalette themes[]{
    {0x0b0a09, 0x131210, 0x1b1915, 0x2f2b23, 0xe9e1cc, 0x9b937d, 0x4d483c, 0xb6f56a, 0x0b0c08, 0x26231c, 0x3a4a22},
    {0x07060f, 0x0f0c1c, 0x191330, 0x33285c, 0xeae6ff, 0x9d95c9, 0x463e75, 0x2ff3e0, 0x04130f, 0x241b45, 0x0f5a55},
    {0x0a0d0b, 0x111611, 0x1a211a, 0x2c382c, 0xd7dccb, 0x8e9a86, 0x424b3f, 0xff9a3c, 0x160c02, 0x222b22, 0x6b3f12},
    {0xf3f0e8, 0xeae6da, 0xdcd7c8, 0xc2bba8, 0x1f1d18, 0x6a6556, 0xb9b2a0, 0x3a8a1e, 0xf7f5ee, 0xe0dbcb, 0xb9d6a6},
};
constexpr Index knobs[]{amount, smooth, link, pan, attack, release};
constexpr wchar_t const* labels[]{L"AMOUNT", L"SMOOTH", L"LINK", L"PAN  L / R", L"ATTACK", L"RELEASE"};
constexpr Rect slot(int i) { return {27 + i * 87, 270, 62, 62}; }
constexpr Rect bounds(int index) {
    if (index == logo) return {10, 6, 84, 38};
    if (index == live) return {273, 16, 64, 24};
    if (index == freeze) return {342, 16, 100, 24};
    if (index == power) return {448, 16, 88, 24};
    if (index == ms) return {279, 334, 79, 20};
    for (int i = 0; i < 6; ++i)
        if (index == knobs[i]) return slot(i);
    return band;
}

float bandAt(int x) {
    return skelldraw::unit(static_cast<float>(x - band.x - 6) / (band.width - 12));
}

// Geometry from Spacer's reference SkellTheme::drawSpacerMark.
void spacerMark(const FlatPainter& p, const FlatPalette& t) {
    p.circle(17, 14, 22, 20, t.text, t.text);
    p.rect({22, 30, 12, 8}, t.text);
    p.circle(22, 21, 6, 6, t.background, t.background);
    p.circle(29, 21, 6, 6, t.background, t.background);
    p.segment(28, 27, 27, 31, t.background);
    p.segment(28, 27, 30, 31, t.background);
    p.rect({26, 34, 1, 4}, t.background);
    p.rect({29, 34, 1, 4}, t.background);
    constexpr int heights[]{13, 16, 14, 0, 0, 14, 16, 13};
    for (int i = 0; i < 8; ++i)
        if (heights[i]) p.rect({43 + static_cast<int>(std::lround(5.6 * i)), 26 - heights[i] / 2, 4, heights[i]}, t.text);
    p.rect({58, 17, 1, 18}, t.accent);
    p.rect({69, 17, 1, 18}, t.accent);
    p.rect({59, 25, 10, 2}, t.accent);
}
}

Editor::Editor(EditorModel& model, FUnknown* owner) : model_(model), owner_(owner) {
    owner_->addRef();
    setRect(ViewRect(0, 0, 495, 405));
    spectrum_.main.fill(-120.f);
    spectrum_.side.fill(-120.f);
}

Editor::~Editor() {
    removed();
    owner_->release();
}

tresult PLUGIN_API Editor::queryInterface(const TUID id, void** object) {
    if (FUnknownPrivate::iidEqual(id, IPlugViewContentScaleSupport::iid)) {
        *object = static_cast<IPlugViewContentScaleSupport*>(this);
        addRef();
        return kResultOk;
    }
    return CPluginView::queryInterface(id, object);
}

tresult PLUGIN_API Editor::setContentScaleFactor(ScaleFactor scale) {
    if (!std::isfinite(scale) || scale <= 0.f) return kInvalidArgument;
    // ViewRect remains in host-supplied device pixels. The same scale also
    // moves the physical limits when the host changes monitor DPI.
    scale_ = scale;
    return kResultOk;
}

tresult PLUGIN_API Editor::isPlatformTypeSupported(FIDString type) {
    return type && std::strcmp(type, kPlatformTypeHWND) == 0 ? kResultOk : kResultFalse;
}

tresult PLUGIN_API Editor::attached(void* parent, FIDString type) {
    if (window_ || !parent || isPlatformTypeSupported(type) != kResultOk) return kResultFalse;
    window_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | SS_NOTIFY,
                             0, 0, getRect().getWidth(), getRect().getHeight(),
                             static_cast<HWND>(parent), nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window_) return kResultFalse;
    if (!SetPropW(window_, property, this)) {
        DestroyWindow(window_);
        window_ = nullptr;
        return kResultFalse;
    }
    previousProc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&windowProc)));
    if (!previousProc_) {
        RemovePropW(window_, property);
        DestroyWindow(window_);
        window_ = nullptr;
        return kResultFalse;
    }
    SetTimer(window_, pollId, 33, nullptr);
    return CPluginView::attached(parent, type);
}

tresult PLUGIN_API Editor::removed() {
    if (window_) {
        finishDrag();
        KillTimer(window_, pollId);
        SetWindowLongPtrW(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previousProc_));
        RemovePropW(window_, property);
        DestroyWindow(window_);
        window_ = nullptr;
    }
    if (buffer_) {
        SelectObject(buffer_, previousBitmap_);
        DeleteObject(bitmap_);
        DeleteDC(buffer_);
        buffer_ = nullptr;
    }
    return CPluginView::removed();
}

tresult PLUGIN_API Editor::onSize(ViewRect* size) {
    if (!size || size->getWidth() <= 0 || size->getHeight() <= 0) return kInvalidArgument;
    if (window_) {
        SetWindowPos(window_, nullptr, 0, 0, size->getWidth(), size->getHeight(), SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(window_, nullptr, FALSE);
    }
    return CPluginView::onSize(size);
}

tresult PLUGIN_API Editor::checkSizeConstraint(ViewRect* size) {
    if (!size) return kInvalidArgument;
    const auto minWidth = static_cast<int>(std::lround(396 * scale_));
    const auto maxWidth = static_cast<int>(std::lround(990 * scale_));
    const auto width = std::clamp(size->getWidth(), minWidth, maxWidth);
    size->right = size->left + width;
    size->bottom = size->top + std::lround(width * designHeight / static_cast<double>(designWidth));
    return kResultOk;
}

Point Editor::designPoint(int x, int y) const {
    const int width = std::max(1, getRect().getWidth());
    const int height = std::max(1, getRect().getHeight());
    return {x * designWidth / width, y * designHeight / height};
}

int Editor::hit(Point p) const {
    if (band.contains(p)) {
        const auto n = bandAt(p.x);
        return std::abs(n - model_.get(low)) <= std::abs(n - model_.get(high)) ? low : high;
    }
    for (const int index : {logo, int(live), int(freeze), int(power), int(ms)})
        if (bounds(index).contains(p)) return index;
    for (const auto knob : knobs)
        if (bounds(knob).contains(p)) return knob;
    return -1;
}

void Editor::editOnce(Index index, float normalized) {
    model_.begin(index);
    model_.edit(index, skelldraw::unit(normalized));
    model_.end(index);
}

void Editor::selectRange(Index index, float n) {
    const float gap = std::log(1.25f) / std::log(1000.f);
    if (index == low) n = std::min(n, model_.get(high) - gap);
    else n = std::max(n, model_.get(low) + gap);
    model_.edit(index, skelldraw::unit(n));
}

void Editor::finishDrag() {
    if (dragging_ >= 0) {
        model_.end(static_cast<Index>(dragging_));
        dragging_ = -1;
    }
    pressed_ = -1;
    if (GetCapture() == window_) ReleaseCapture();
}

tresult PLUGIN_API Editor::onWheel(float distance) {
    const auto index = hovered_ >= 0 ? hovered_ : focused_;
    if (index < 0 || index >= parameterCount || specs[index].mapping == Mapping::toggle)
        return kResultFalse;
    const auto param = static_cast<Index>(index);
    const auto value = skelldraw::Knob::wheel(model_.get(param), distance, .025f);
    if (index == low || index == high) {
        model_.begin(param);
        selectRange(param, value);
        model_.end(param);
    } else editOnce(param, value);
    return kResultOk;
}

tresult PLUGIN_API Editor::onKeyDown(char16 character, int16 key, int16) {
    if (character == ' ' && focused_ == logo) {
        model_.cycleTheme();
        return kResultOk;
    }
    if (focused_ < 0 || focused_ >= parameterCount) return kResultFalse;
    const auto index = static_cast<Index>(focused_);
    if (character == ' ' && specs[index].mapping == Mapping::toggle) {
        editOnce(index, 1.f - model_.get(index));
        return kResultOk;
    }
    if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT) {
        const int saved = hovered_;
        hovered_ = focused_;
        const auto result = onWheel(key == KEY_UP || key == KEY_RIGHT ? 1.f : -1.f);
        hovered_ = saved;
        return result;
    }
    return kResultFalse;
}

LRESULT CALLBACK Editor::windowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    auto* self = static_cast<Editor*>(GetPropW(hwnd, property));
    return self ? self->windowMessage(hwnd, message, w, l) : DefWindowProcW(hwnd, message, w, l);
}

LRESULT Editor::windowMessage(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    const auto point = designPoint(GET_X_LPARAM(l), GET_Y_LPARAM(l));
    switch (message) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            const auto dc = BeginPaint(hwnd, &ps);
            RECT r{};
            GetClientRect(hwnd, &r);
            paint(dc, r.right, r.bottom);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (w == pollId) { poll(); return 0; }
            break;
        case WM_PRINTCLIENT: {
            RECT r{};
            GetClientRect(hwnd, &r);
            paint(reinterpret_cast<HDC>(w), r.right, r.bottom);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            const int index = hit(point);
            if (index < 0) break;
            SetFocus(hwnd);
            pressed_ = focused_ = index;
            SetCapture(hwnd);
            if (index < parameterCount && specs[index].mapping != Mapping::toggle) {
                dragging_ = index;
                startY_ = point.y;
                initial_ = model_.get(static_cast<Index>(index));
                model_.begin(static_cast<Index>(index));
                if (index == low || index == high) selectRange(static_cast<Index>(index), bandAt(point.x));
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            const int next = hit(point);
            if (next != hovered_) {
                hovered_ = next;
                TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
                TrackMouseEvent(&tracking);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if (dragging_ >= 0) {
                const auto index = static_cast<Index>(dragging_);
                if (index == low || index == high) selectRange(index, bandAt(point.x));
                else model_.edit(index, skelldraw::Knob::drag(initial_, startY_ - point.y, 1.f / 250.f));
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            hovered_ = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP:
            if (dragging_ < 0 && pressed_ >= 0 && pressed_ == hit(point)) {
                if (pressed_ == logo) model_.cycleTheme();
                else {
                    const auto index = static_cast<Index>(pressed_);
                    editOnce(index, 1.f - model_.get(index));
                }
            }
            finishDrag();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDBLCLK: {
            finishDrag();
            const auto index = hit(point);
            if (index >= 0 && index < parameterCount && specs[index].mapping != Mapping::toggle)
                editOnce(static_cast<Index>(index), specs[index].normalized(specs[index].initial));
            return 0;
        }
        case WM_CAPTURECHANGED: finishDrag(); return 0;
        case WM_MOUSEWHEEL:
            onWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(w)) / WHEEL_DELTA);
            return 0;
        case WM_KEYDOWN:
            if (w == VK_TAB) {
                const auto next = GetKeyState(VK_SHIFT) < 0 ? -1 : 1;
                const auto index = focused_ == logo ? parameterCount : focused_;
                focused_ = (index + next + parameterCount + 1) % (parameterCount + 1);
                if (focused_ == parameterCount) focused_ = logo;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (w == VK_SPACE) onKeyDown(' ', 0, 0);
            else if (w >= VK_LEFT && w <= VK_DOWN) {
                constexpr int16 keys[]{KEY_LEFT, KEY_UP, KEY_RIGHT, KEY_DOWN};
                onKeyDown(0, keys[w - VK_LEFT], 0);
            }
            return 0;
        case WM_NCDESTROY: {
            finishDrag();
            RemovePropW(hwnd, property);
            window_ = nullptr;
            break;
        }
        default: break;
    }
    return CallWindowProcW(previousProc_, hwnd, message, w, l);
}

void Editor::poll() {
    const auto reading = model_.spectrum();
    bool dirty = reading.sidechainActive != spectrum_.sidechainActive || reading.rate != spectrum_.rate
        || reading.latency != spectrum_.latency || paintedTheme_ != model_.theme();
    spectrum_.sidechainActive = reading.sidechainActive;
    spectrum_.rate = reading.rate;
    spectrum_.latency = reading.latency;
    for (std::size_t i = 0; i < specs.size(); ++i) dirty |= paintedValues_[i] != model_.get(static_cast<Index>(i));
    for (std::size_t i = 0; i < bins; ++i) {
        auto follow = [&](float& current, float target, float speed) {
            const auto difference = target - current;
            if (std::abs(difference) > .01f) {
                current += difference * speed;
                dirty = true;
            }
        };
        follow(spectrum_.main[i], reading.main[i], reading.main[i] > spectrum_.main[i] ? .6f : .15f);
        follow(spectrum_.side[i], reading.side[i], reading.side[i] > spectrum_.side[i] ? .6f : .15f);
        follow(spectrum_.cut[i], reading.cut[i], .35f);
    }
    if (dirty) InvalidateRect(window_, nullptr, FALSE);
}

void Editor::paint(HDC target, int width, int height) {
    if (!target || width <= 0 || height <= 0) return;
    if (!buffer_) {
        buffer_ = CreateCompatibleDC(target);
        if (!buffer_) return;
    }
    if (width != bufferWidth_ || height != bufferHeight_ || !bitmap_) {
        auto replacement = CreateCompatibleBitmap(target, width, height);
        if (!replacement) return;
        auto previous = SelectObject(buffer_, replacement);
        if (bitmap_) DeleteObject(bitmap_);
        else previousBitmap_ = previous;
        bitmap_ = replacement;
        bufferWidth_ = width;
        bufferHeight_ = height;
    }
    const int saved = SaveDC(buffer_);
    SetMapMode(buffer_, MM_ANISOTROPIC);
    SetWindowExtEx(buffer_, designWidth, designHeight, nullptr);
    SetViewportExtEx(buffer_, width, height, nullptr);
    paintedTheme_ = std::clamp(model_.theme(), 0, 3);
    const auto& t = themes[paintedTheme_];
    const FlatPainter p(buffer_, t);
    for (std::size_t i = 0; i < specs.size(); ++i) paintedValues_[i] = model_.get(static_cast<Index>(i));
    auto active = [&](Index idx) { return paintedValues_[idx] >= .5f; };
    auto valueText = [&](Index index, wchar_t* text) {
        Vst::String128 raw{};
        model_.text(index, raw);
        for (std::size_t i = 0; i < 128; ++i) text[i] = static_cast<wchar_t>(raw[i]);
    };
    auto button = [&](Index index, const wchar_t* text) {
        p.button(bounds(index), text, active(index), hovered_ == index, pressed_ == index);
    };

    p.rect({0, 0, designWidth, designHeight}, t.background);
    if (hovered_ == logo || pressed_ == logo) p.rect(bounds(logo), t.hover);
    spacerMark(p, t);
    p.text(L"SKELL SPACER", {100, 9, 169, 26}, 19, true, t.text, DT_LEFT, 3);
    p.text(L"SPACE / DUCK / SIDECHAIN", {101, 33, 172, 14}, 10, false, t.muted, DT_LEFT, 1);
    button(live, L"LIVE");
    button(freeze, L"FREEZE");
    button(power, L"POWER");
    const auto peak = *std::max_element(spectrum_.cut.begin(), spectrum_.cut.end());
    const auto lit = std::clamp(std::lround(peak * 2.f), 0l, 48l);
    for (int i = 0; i < 48; ++i)
        p.rect({14 + static_cast<int>(std::lround(522 * i / 48.f)), 56, 9, i < lit ? 3 : 2}, i < lit ? t.accent : t.faint);
    p.card({14, 66, 522, 184});
    p.text(L"SPECTRUM", {30, 72, 120, 14}, 11, true, t.muted, DT_LEFT, 2);
    wchar_t details[128]{};
    std::swprintf(details, 128, L"%.1f kHz   /   %u smp latency", spectrum_.rate / 1000., spectrum_.latency);
    p.text(details, {150, 72, 190, 14}, 10, false, t.muted, DT_LEFT);
    const bool pending = active(live) != (spectrum_.latency == 0);
    p.text(pending ? L"MODE PENDING" : spectrum_.sidechainActive ? L"SIDECHAIN OK" : L"NO SIDECHAIN",
           {330, 72, 192, 14}, 11, true, !pending && spectrum_.sidechainActive ? t.accent : 0xe58a6b, DT_RIGHT, 2);
    p.rect(graph, t.background);
    constexpr Rect grid{36, 100, 478, 132};
    const auto lineY = grid.y + std::lround(.42f * grid.height);
    for (int i = 0; i < 3; ++i) {
        constexpr float frequencies[]{100, 1000, 10000};
        constexpr const wchar_t* ticks[]{L"100", L"1k", L"10k"};
        const auto x = grid.x + std::lround(grid.width * specs[low].normalized(frequencies[i]));
        p.segment(x, grid.y, x, grid.y + grid.height, t.line);
        p.text(ticks[i], {x + 3, grid.y + 1, 30, 12}, 10, false, t.muted, DT_LEFT);
    }
    p.curve(spectrum_.main, grid, -100.f, 0.f, t.faint, false, 1, true);
    p.curve(spectrum_.side, grid, -95.f, 5.f, t.text);
    p.segment(grid.x, lineY, grid.x + grid.width, lineY, t.line);
    p.text(L"0 dB", {39, static_cast<int>(lineY) - 12, 40, 11}, 10, false, t.muted, DT_LEFT);
    p.text(L"-24", {39, static_cast<int>(lineY) + 42, 40, 11}, 10, false, t.muted, DT_LEFT);
    p.curve(spectrum_.cut, {grid.x, static_cast<int>(lineY), grid.width, 55}, 0.f, 24.f, t.accent, true, 2);
    std::swprintf(details, 128, L"MAX CUT  %.1f dB", -peak);
    p.text(details, {40, 101, 160, 12}, 10, true, t.accent, DT_LEFT);
    p.rect({330, 221, 8, 7}, t.muted);
    p.text(L"TRACK", {341, 217, 38, 13}, 9, true, t.muted, DT_LEFT, 1);
    p.rect({389, 224, 8, 2}, t.text);
    p.text(L"SIDECHAIN", {400, 217, 66, 13}, 9, true, t.muted, DT_LEFT, 1);
    p.rect({474, 224, 8, 2}, t.accent);
    p.text(L"CUT", {485, 217, 24, 13}, 9, true, t.muted, DT_LEFT, 1);
    if (active(freeze)) {
        p.rect({40, 217, 58, 13}, t.accent);
        p.text(L"FROZEN", {40, 217, 58, 13}, 10, true, t.onAccent);
    }
    if (!active(power)) {
        p.rect({215, 150, 120, 27}, t.background);
        p.text(L"BYPASSED", {215, 150, 120, 27}, 16, true, t.muted);
    }
    p.outline(graph, t.line);

    p.card({14, 256, 522, 104});
    for (int i = 0; i < 6; ++i) {
        const auto index = knobs[i];
        valueText(index, details);
        if (i) p.segment(14 + i * 87, 268, 14 + i * 87, 348, t.line);
        p.rotary(slot(i), paintedValues_[index], index == pan, hovered_ == index, pressed_ == index, details);
        p.text(index == pan && active(ms) ? L"PAN  M / S" : labels[i], {14 + i * 87, 336, 87, 16},
               11, true, t.muted, DT_CENTER, 2);
    }
    if (hovered_ == ms || pressed_ == ms) p.outline(bounds(ms), t.accent);

    p.card({14, 366, 522, 64});
    p.text(L"LOW CUT", {30, 374, 70, 14}, 11, true, t.muted, DT_LEFT, 1);
    p.text(L"HIGH CUT", {450, 374, 72, 14}, 11, true, t.muted, DT_RIGHT, 1);
    p.text(L"BAND", {14, 374, 522, 14}, 11, true, t.muted, DT_CENTER, 2);
    valueText(low, details);
    p.text(details, {92, 374, 100, 14}, 12, true, t.text, DT_LEFT);
    valueText(high, details);
    p.text(details, {340, 374, 100, 14}, 12, true, t.text, DT_RIGHT);
    p.range(band, paintedValues_[low], paintedValues_[high]);
    p.text(L"v0.1.0 DEV", {14, 432, 90, 14}, 11, false, t.muted, DT_LEFT);
    if (GetFocus() == window_ && focused_ >= 0) p.outline(bounds(focused_), t.muted);
    RestoreDC(buffer_, saved);
    BitBlt(target, 0, 0, width, height, buffer_, 0, 0, SRCCOPY);
}

} // namespace Skelly::NativeSpacer
