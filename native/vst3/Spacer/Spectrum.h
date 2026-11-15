#pragma once
#include <array>
#include <cstdint>

namespace Skelly::NativeSpacer {
inline constexpr char spectrumRequest[] = "SkellSpacer.SpectrumRequest";
inline constexpr char spectrumResponse[] = "SkellSpacer.SpectrumResponse";
inline constexpr std::size_t bins = 256;
struct Spectrum {
    std::array<float, bins> main{}, side{}, cut{};
    double rate = 48000.0;
    std::uint32_t latency = 0;
    bool sidechainActive = false;
};
} // namespace Skelly::NativeSpacer
