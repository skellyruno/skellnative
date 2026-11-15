#pragma once
// Host-neutral control ABI consumed by SkellLang native schema v2.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace skellnative::sdk {
enum class NativeControlFlags { automate, bypass, readonly };

struct NativeControl {
    std::uint32_t id;
    const char16_t* title;
    const char* key;
    float low, high, initial, skew;
    std::int32_t steps = 0;
    const char16_t* units = u"";
    NativeControlFlags flags = NativeControlFlags::automate;

    float canonical(float n) const noexcept {
        if (!std::isfinite(n)) n = normalized(initial);
        n = std::clamp(n, 0.f, 1.f);
        return steps > 0 ? std::round(n * static_cast<float>(steps)) / static_cast<float>(steps) : n;
    }
    float plain(float n) const noexcept {
        return low + (high - low) * std::pow(canonical(n), 1.f / skew);
    }
    float normalized(float p) const noexcept {
        if (!std::isfinite(p)) p = initial;
        const float n = std::pow(std::clamp((p - low) / (high - low), 0.f, 1.f), skew);
        return steps > 0 ? std::round(n * static_cast<float>(steps)) / static_cast<float>(steps) : n;
    }
};
} // namespace skellnative::sdk
