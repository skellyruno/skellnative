#include "skellnative/audio.hpp"

#include <cstddef>
#include <cstring>
#include <limits>

namespace skellnative {
namespace {

bool byteCountFits(std::uint32_t frames) noexcept {
    return static_cast<std::uint64_t>(frames) <=
        std::numeric_limits<std::size_t>::max() / sizeof(float);
}

bool validInput(InputAudio input) noexcept {
    if (input.frameCount == 0 || input.channelCount == 0) return true;
    if (input.channels == nullptr || !byteCountFits(input.frameCount)) return false;
    for (std::uint32_t channel = 0; channel < input.channelCount; ++channel) {
        if (input.channels[channel] == nullptr) return false;
    }
    return true;
}

bool validOutput(OutputAudio output) noexcept {
    if (output.frameCount == 0 || output.channelCount == 0) return true;
    if (output.channels == nullptr || !byteCountFits(output.frameCount)) return false;
    for (std::uint32_t channel = 0; channel < output.channelCount; ++channel) {
        if (output.channels[channel] == nullptr) return false;
    }
    return true;
}

} // namespace

AudioStatus copyAudio(InputAudio input, OutputAudio output) noexcept {
    if (input.channelCount != output.channelCount ||
        input.frameCount != output.frameCount) {
        return AudioStatus::incompatibleLayout;
    }
    if (!validInput(input) || !validOutput(output)) {
        return AudioStatus::invalidBuffer;
    }
    if (input.channelCount == 0 || input.frameCount == 0) return AudioStatus::ok;
    const auto bytes = static_cast<std::size_t>(input.frameCount) * sizeof(float);
    for (std::uint32_t channel = 0; channel < input.channelCount; ++channel) {
        if (input.channels[channel] != output.channels[channel]) {
            std::memmove(output.channels[channel], input.channels[channel], bytes);
        }
    }
    return AudioStatus::ok;
}

AudioStatus clearAudio(OutputAudio output) noexcept {
    if (!validOutput(output)) return AudioStatus::invalidBuffer;
    if (output.channelCount == 0 || output.frameCount == 0) return AudioStatus::ok;
    const auto bytes = static_cast<std::size_t>(output.frameCount) * sizeof(float);
    for (std::uint32_t channel = 0; channel < output.channelCount; ++channel) {
        std::memset(output.channels[channel], 0, bytes);
    }
    return AudioStatus::ok;
}

} // namespace skellnative
