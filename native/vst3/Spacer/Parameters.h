#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Skelly::NativeSpacer {

enum Index { amount, smooth, link, attack, release, low, high, freeze, power, pan, ms, live, parameterCount };
enum class Mapping { linear, skewed, frequency, toggle };

struct ParameterSpec {
    const char* key;
    const wchar_t* title;
    const wchar_t* unit;
    float minimum, maximum, interval, initial, centre;
    Mapping mapping;

    // JUCE 8.0.12 hashes the string ID and clears the sign bit.
    constexpr std::uint32_t id() const {
        std::uint32_t hash = 0;
        for (auto* c = key; *c; ++c) hash = 31u * hash + static_cast<unsigned char>(*c);
        return hash & 0x7fffffffu;
    }

    float skew() const {
        return std::log(0.5f) / std::log((centre - minimum) / (maximum - minimum));
    }

    float plain(float normalized) const {
        normalized = std::clamp(normalized, 0.f, 1.f);
        if (mapping == Mapping::toggle) return normalized >= 0.5f ? 1.f : 0.f;
        if (mapping == Mapping::frequency) return minimum * std::pow(maximum / minimum, normalized);
        if (mapping == Mapping::skewed && normalized > 0.f)
            normalized = std::exp(std::log(normalized) / skew());
        const float value = minimum + (maximum - minimum) * normalized;
        return std::clamp(minimum + interval * std::floor((value - minimum) / interval + 0.5f),
                          minimum, maximum);
    }

    float normalized(float value) const {
        value = std::clamp(value, minimum, maximum);
        if (mapping == Mapping::frequency) return std::log(value / minimum) / std::log(maximum / minimum);
        const float position = (value - minimum) / (maximum - minimum);
        return mapping == Mapping::skewed ? std::pow(position, skew()) : position;
    }
};

inline constexpr std::array<ParameterSpec, parameterCount> specs{{
    {"amount", L"Amount", L"%", 0, 100, .1f, 50, 0, Mapping::linear},
    {"smooth", L"Smooth", L"%", 0, 100, .1f, 30, 0, Mapping::linear},
    {"link", L"Stereo Link", L"%", 0, 100, .1f, 0, 0, Mapping::linear},
    {"attack", L"Attack", L"ms", 1, 200, .1f, 10, 20, Mapping::skewed},
    {"release", L"Release", L"ms", 10, 1000, 1, 150, 150, Mapping::skewed},
    {"lowcut", L"Low Cut", L"Hz", 20, 20000, 0, 20, 0, Mapping::frequency},
    {"highcut", L"High Cut", L"Hz", 20, 20000, 0, 20000, 0, Mapping::frequency},
    {"freeze", L"Freeze", L"", 0, 1, 1, 0, 0, Mapping::toggle},
    {"power", L"Power", L"", 0, 1, 1, 1, 0, Mapping::toggle},
    {"pan", L"Pan", L"%", -100, 100, .1f, 0, 0, Mapping::linear},
    {"ms", L"Mid/Side", L"", 0, 1, 1, 0, 0, Mapping::toggle},
    {"liveMode", L"Live Tracking", L"", 0, 1, 1, 1, 0, Mapping::toggle},
}};

inline constexpr std::uint32_t bypassId = 0x62797073u;
inline constexpr ParameterSpec bypassSpec{"byps", L"Bypass", L"", 0, 1, 1, 0, 0, Mapping::toggle};
static_assert(bypassSpec.id() != bypassId); // JUCE assigns a reserved ID, independent of its hash.
inline constexpr char productId[] = "audio.skell.spacer.native.dev";
using Values = std::array<float, parameterCount + 1>;

inline Values defaults() {
    Values result{};
    for (std::size_t i = 0; i < specs.size(); ++i)
        result[i] = specs[i].normalized(specs[i].initial);
    return result;
}

} // namespace Skelly::NativeSpacer
