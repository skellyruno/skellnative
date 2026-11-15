#pragma once
#include <SkellNative/Controls.h>
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <array>

namespace skellnative::vst3 {
// Piecewise-constant sample-offset automation. Continuous DSP smoothing remains
// the engine's responsibility. Validate the entire batch before touching audio
// or state. No allocation and no sorting on the audio thread.
template<std::size_t N, class Apply, class Render>
bool processControlAutomation(Steinberg::Vst::IParameterChanges* changes,
    Steinberg::int32 frames, const sdk::NativeControl (&controls)[N], Apply apply, Render render) {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    if (frames < 0) return false;
    std::array<IParamValueQueue*, N> queues{};
    std::array<int32, N> cursor{}, count{};
    std::array<int32, N> offsets{};
    std::array<ParamValue, N> values{};
    int32 total = 0;
    if (changes) {
        const int32 queueCount = changes->getParameterCount();
        if (queueCount < 0 || queueCount > 1024) return false;
        for (int32 q = 0; q < queueCount; ++q) {
            auto* queue = changes->getParameterData(q);
            if (!queue) return false;
            std::size_t i = 0;
            for (; i < N; ++i) if (controls[i].id == queue->getParameterId()) break;
            if (i == N || controls[i].flags == sdk::NativeControlFlags::readonly) continue;
            if (queues[i]) return false;
            queues[i] = queue;
            count[i] = queue->getPointCount();
            if (count[i] < 0 || count[i] > 8192 - total) return false;
            total += count[i];
            int32 previous = -1;
            for (int32 p = 0; p < count[i]; ++p) {
                int32 offset{};
                ParamValue value{};
                if (queue->getPoint(p, offset, value) != kResultOk || !std::isfinite(value)
                    || offset < 0 || offset < previous || (frames == 0 ? offset != 0 : offset >= frames)) return false;
                previous = offset;
            }
            if (count[i]) queue->getPoint(0, offsets[i], values[i]);
        }
    }
    int32 position = 0;
    for (;;) {
        int32 next = frames;
        bool pending = false;
        for (std::size_t i = 0; i < N; ++i) {
            if (cursor[i] < count[i]) { next = std::min(next, offsets[i]); pending = true; }
        }
        if (next > position) render(position, next - position);
        if (!pending) break;
        for (std::size_t i = 0; i < N; ++i) {
            while (cursor[i] < count[i] && offsets[i] == next) {
                apply(i, controls[i].canonical(static_cast<float>(values[i])));
                if (++cursor[i] < count[i]) queues[i]->getPoint(cursor[i], offsets[i], values[i]);
            }
        }
        position = next;
    }
    return true;
}
} // namespace skellnative::vst3
