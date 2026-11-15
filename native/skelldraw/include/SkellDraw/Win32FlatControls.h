#pragma once
#include <SkellDraw/Win32Painter.h>

namespace skelldraw::win32 {

struct FlatPalette {
    std::uint32_t background, card, panel, line, text, muted, faint, accent, onAccent, hover, pressed;
};

// Flat controls in design coordinates. The caller owns viewport scaling.
class FlatPainter {
public:
    FlatPainter(HDC dc, const FlatPalette& palette) : dc_(dc), palette_(palette) {}

    void rect(Rect r, std::uint32_t rgb) const {
        fill(dc_, r.x, r.y, r.width, r.height, color(rgb));
    }

    void segment(int x, int y, int endX, int endY, std::uint32_t rgb, int width = 1) const {
        line(dc_, x, y, endX, endY, color(rgb), width);
    }

    void text(const wchar_t* content, Rect r, int size, bool bold, std::uint32_t rgb,
              UINT align = DT_CENTER) const {
        label(dc_, content, size, bold ? FW_BOLD : FW_NORMAL, r.x, r.y, r.width, r.height, color(rgb), align);
    }

    void outline(Rect r, std::uint32_t rgb) const {
        rect({r.x, r.y, r.width, 1}, rgb);
        rect({r.x, r.y + r.height - 1, r.width, 1}, rgb);
        rect({r.x, r.y, 1, r.height}, rgb);
        rect({r.x + r.width - 1, r.y, 1, r.height}, rgb);
    }

    void card(Rect r) const {
        rect(r, palette_.card);
        outline(r, palette_.line);
        for (const int x : {r.x, r.x + r.width - 1}) {
            for (const int y : {r.y, r.y + r.height - 1}) {
                segment(x, y, x + (x == r.x ? 9 : -9), y, palette_.muted);
                segment(x, y, x, y + (y == r.y ? 9 : -9), palette_.muted);
            }
        }
    }

    void button(Rect r, const wchar_t* title, bool on, bool hot, bool down) const {
        rect(r, on ? palette_.accent : down ? palette_.pressed : hot ? palette_.hover : palette_.panel);
        outline(r, on ? palette_.accent : hot ? palette_.muted : palette_.line);
        text(title, r, 12, true, on ? palette_.onAccent : hot ? palette_.accent : palette_.text);
    }

    void circle(int x, int y, int width, int height, std::uint32_t rgb, std::uint32_t border) const {
        ellipse(dc_, x, y, x + width, y + height, color(rgb), color(border));
    }

    void rotary(Rect r, float position, bool bipolar, bool hot, bool down, const wchar_t* value) const {
        constexpr float pi = 3.14159265358979323846f;
        const auto x = r.x + r.width / 2;
        const auto y = r.y + r.height / 2;
        const float radius = static_cast<float>(r.width) * 0.385f;
        const auto point = [&](float fraction, float distance) {
            const auto angle = pi * (1.25f + 1.5f * fraction);
            return POINT{x + std::lround(distance * std::sin(angle)),
                         y - std::lround(distance * std::cos(angle))};
        };
        for (int i = 0; i <= 10; ++i) {
            const auto a = point(i / 10.f, radius + 2.f);
            const auto b = point(i / 10.f, radius + (i % 5 == 0 ? 7.f : 5.f));
            segment(a.x, a.y, b.x, b.y, palette_.faint);
        }
        const auto arc = [&](float start, float end, std::uint32_t rgb) {
            constexpr int segments = 48;
            std::array<POINT, segments + 1> points{};
            for (int i = 0; i <= segments; ++i)
                points[i] = point(start + (end - start) * i / segments, radius);
            const auto pen = CreatePen(PS_SOLID, 3, color(rgb));
            if (!pen) return;
            const auto old = SelectObject(dc_, pen);
            Polyline(dc_, points.data(), static_cast<int>(points.size()));
            SelectObject(dc_, old);
            DeleteObject(pen);
        };
        arc(0, 1, palette_.line);
        arc(bipolar ? .5f : 0.f, position, palette_.accent);
        const int body = std::lround(radius - 7.f);
        circle(x - body, y - body, 2 * body, 2 * body, hot ? palette_.hover : palette_.panel, palette_.line);
        const auto marker = point(position, radius);
        rect({marker.x - 2, marker.y - 2, 5, 5}, palette_.text);
        text(value, r, 12, true, down ? palette_.accent : palette_.text);
    }

    void range(Rect r, float low, float high) const {
        const int y = r.y + r.height / 2;
        const int left = r.x + 6;
        const int span = r.width - 12;
        const int a = left + std::lround(span * low);
        const int b = left + std::lround(span * high);
        rect({left, y - 2, span, 4}, palette_.line);
        rect({a, y - 2, b - a, 4}, palette_.accent);
        for (const int x : {a, b}) {
            rect({x - 5, y - 13, 10, 26}, palette_.text);
            outline({x - 5, y - 13, 10, 26}, palette_.line);
            rect({x, y - 7, 1, 14}, palette_.background);
        }
    }

    template<std::size_t N>
    void curve(const std::array<float, N>& values, Rect r, float minimum, float maximum,
               std::uint32_t rgb, bool invert = false, int width = 1) const {
        std::array<POINT, N> points{};
        for (std::size_t i = 0; i < N; ++i) {
            const auto level = unit((values[i] - minimum) / (maximum - minimum));
            points[i] = {r.x + static_cast<LONG>(i * r.width / (N - 1)),
                         r.y + std::lround(r.height * (invert ? level : 1.f - level))};
        }
        auto pen = CreatePen(PS_SOLID, width, color(rgb));
        if (!pen) return;
        auto old = SelectObject(dc_, pen);
        Polyline(dc_, points.data(), static_cast<int>(N));
        SelectObject(dc_, old);
        DeleteObject(pen);
    }

private:
    HDC dc_;
    const FlatPalette& palette_;
};

} // namespace skelldraw::win32
