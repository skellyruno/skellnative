#pragma once

#include <cstdint>

namespace skellnative {

// Non-owning planar float audio buffers. Channel arrays and samples are owned
// by the caller (typically a host adapter). Audio is never allocated here.
struct InputAudio {
    const float* const* channels = nullptr;
    std::uint32_t channelCount = 0;
    std::uint32_t frameCount = 0;
};

struct OutputAudio {
    float* const* channels = nullptr;
    std::uint32_t channelCount = 0;
    std::uint32_t frameCount = 0;
};

enum class AudioStatus : std::uint8_t {
    ok,
    invalidBuffer,
    incompatibleLayout
};

// Copy an equal-channel, equal-frame block. Input and corresponding output
// channels may alias (in-place processing); cross-channel overlapping regions
// are not supported. No allocation, locks, logging, exceptions or host calls.
// An error leaves all output channels unchanged.
AudioStatus copyAudio(InputAudio input, OutputAudio output) noexcept;

// Zero a writable output block without allocating.
// An error leaves all output channels unchanged.
AudioStatus clearAudio(OutputAudio output) noexcept;

} // namespace skellnative
