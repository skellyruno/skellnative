#pragma once
// Host-neutral control ABI consumed by SkellLang native schema v2.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>

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

// Validate on initialization, not in the real-time callback. Reject ambiguous
// persistent identities and degenerate parameter maps before a host can save
// incompatible state. This is additive: existing valid parameter IDs are stable.
inline bool validControlTable(const NativeControl* entries, std::size_t count) noexcept {
    if (!entries || count == 0 || count > 1024) return false;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& c = entries[i];
        if (!c.title || !c.key || !*c.key || !c.units
            || !std::isfinite(c.low) || !std::isfinite(c.high)
            || !std::isfinite(c.initial) || !std::isfinite(c.skew)
            || !(c.low < c.high) || c.initial < c.low || c.initial > c.high
            || c.skew <= 0.f || c.steps < 0 || c.steps > 65535)
            return false;
        if (c.flags == NativeControlFlags::bypass && c.steps != 1) return false;
        if (c.flags == NativeControlFlags::readonly && c.steps != 0) return false;
        for (std::size_t j = 0; j < i; ++j) {
            if (c.id == entries[j].id || std::strcmp(c.key, entries[j].key) == 0)
                return false;
        }
    }
    return true;
}
} // namespace skellnative::sdk
