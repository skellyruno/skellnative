#pragma once
#include "Parameters.h"
#include <SkellNative/State.h>
#include "pluginterfaces/base/ibstream.h"

namespace Skelly::NativeSpacer {

inline const char* stateKey(std::size_t i) {
    return i < specs.size() ? specs[i].key : "hostBypass";
}

inline std::optional<Values> readState(Steinberg::IBStream* stream) {
    if (!stream) return std::nullopt;
    std::vector<std::uint8_t> bytes;
    std::array<std::uint8_t, 512> chunk{};
    for (;;) {
        Steinberg::int32 count = 0;
        const auto result = stream->read(chunk.data(), static_cast<Steinberg::int32>(chunk.size()), &count);
        if (count < 0 || count > static_cast<Steinberg::int32>(chunk.size())
            || bytes.size() + static_cast<std::size_t>(count) > 16384) return std::nullopt;
        if (count == 0) break;
        if (result != Steinberg::kResultOk) return std::nullopt;
        bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + count);
    }
    const auto snapshot = skellnative::sdk::decodeState(productId, bytes);
    Values result{};
    if (!snapshot || snapshot->parameters.size() != result.size()) return std::nullopt;
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto it = std::find_if(snapshot->parameters.begin(), snapshot->parameters.end(),
            [i](const auto& entry) { return entry.persistentId == stateKey(i); });
        if (it == snapshot->parameters.end()) return std::nullopt;
        result[i] = it->normalized;
    }
    return result;
}

inline bool writeState(Steinberg::IBStream* stream, const Values& values) {
    if (!stream) return false;
    skellnative::sdk::StateSnapshot state;
    for (std::size_t i = 0; i < values.size(); ++i)
        state.parameters.push_back({stateKey(i), values[i]});
    const auto encoded = skellnative::sdk::encodeState(productId, state);
    if (!encoded) return false;
    auto bytes = *encoded;
    Steinberg::int32 written = 0;
    const auto size = static_cast<Steinberg::int32>(bytes.size());
    return stream->write(bytes.data(), size, &written) == Steinberg::kResultOk && written == size;
}

} // namespace Skelly::NativeSpacer
