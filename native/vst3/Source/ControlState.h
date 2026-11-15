#pragma once
// Off-audio-thread state IO. Decode fully before publishing any values.
#include <SkellNative/Controls.h>
#include <SkellNative/State.h>
#include "pluginterfaces/base/ibstream.h"
#include <array>

namespace skellnative::vst3 {
template<std::size_t N>
std::optional<std::array<float, N>> readControlState(
    Steinberg::IBStream* stream, const char* product,
    const sdk::NativeControl (&controls)[N]) {
    if (!stream) return {};
    std::array<std::uint8_t, 512> chunk{};
    std::vector<std::uint8_t> bytes;
    for (;;) {
        Steinberg::int32 n = 0;
        const auto result = stream->read(chunk.data(), static_cast<Steinberg::int32>(chunk.size()), &n);
        if (n < 0 || n > static_cast<Steinberg::int32>(chunk.size()) || bytes.size() + n > 16384) return {};
        if (n == 0) break; // IBStream implementations may signal EOF with kResultFalse.
        if (result != Steinberg::kResultOk) return {};
        bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + n);
    }
    const auto state = sdk::decodeState(product, bytes);
    if (!state) return {};
    std::array<float, N> values{};
    // Newly appended parameters retain their declared defaults in older states.
    // Unknown keys fail closed instead of silently dropping future data.
    for (std::size_t i = 0; i < N; ++i) values[i] = controls[i].normalized(controls[i].initial);
    for (const auto& entry : state->parameters) {
        std::size_t i = 0;
        for (; i < N; ++i) if (entry.persistentId == controls[i].key) break;
        if (i == N) return {};
        values[i] = controls[i].canonical(entry.normalized);
    }
    return values;
}

template<std::size_t N, class Read>
Steinberg::tresult writeControlState(Steinberg::IBStream* stream, const char* product,
    const sdk::NativeControl (&controls)[N], Read read) {
    if (!stream) return Steinberg::kInvalidArgument;
    sdk::StateSnapshot snapshot;
    for (std::size_t i = 0; i < N; ++i) snapshot.parameters.push_back({controls[i].key, read(i)});
    auto bytes = sdk::encodeState(product, snapshot);
    if (!bytes) return Steinberg::kResultFalse;
    const auto size = static_cast<Steinberg::int32>(bytes->size());
    Steinberg::int32 written{};
    return stream->write(bytes->data(), size, &written) == Steinberg::kResultOk && written == size
        ? Steinberg::kResultOk : Steinberg::kResultFalse;
}
} // namespace skellnative::vst3
