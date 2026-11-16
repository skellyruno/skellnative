#pragma once

#include "Parameters.h"
#include <SkellNative/PluginApi.h>
#include "ZeroLatencyEngine.h"

namespace Skelly::NativeSpacer {

class SpacerProcessor final : public skellnative::sdk::Processor {
public:
    void configure(const Values& values) noexcept {
        if (specs[smooth].plain(values[smooth]) != specs[smooth].initial)
            smoothAtConstructionDefault_ = false;
        values_ = values;
    }
    float& value(std::size_t index) noexcept { return values_[index]; }

    bool prepare(double sampleRate, std::uint32_t) noexcept override {
        if (!std::isfinite(sampleRate) || sampleRate <= 1000.0)
            return false;
        try {
            studio_.prepare(sampleRate);
            live_.prepare(sampleRate);
        } catch (...) {
            return false;
        }
        latchMode();
        return true;
    }

    void latchMode() noexcept { liveActive_ = values_[live] >= 0.5f; }
    void reset() noexcept override { live_.reset(); studio_.reset(); }

    std::uint32_t latencySamples() const noexcept override {
        return liveActive_ ? 0u : static_cast<std::uint32_t>(studio_.latencySamples());
    }

    void readSpectrum(bool isLive, float* main, float* side, float* cut) const noexcept {
        if (isLive) live_.getDisplay(main, side, cut);
        else studio_.getDisplay(main, side, cut);
    }

    bool process(const skellnative::sdk::ProcessContext& context) noexcept override {
        if (!context.valid() || context.changeCount != 0) return false;
        auto plain = [this](Index index) { return specs[index].plain(values_[index]); };
        skell::SpacerParams parameters;
        parameters.amount = plain(amount) * 0.01f;
        // JUCE APVTS starts with an unsnapped raw default. After the first
        // real value change it uses snapped values, including a return to 30%.
        constexpr float rawSmoothDefault = (30.f / 100.f) * 100.f;
        parameters.smooth = (smoothAtConstructionDefault_ ? rawSmoothDefault : plain(smooth)) * 0.01f;
        parameters.link = plain(link) * 0.01f;
        parameters.attackMs = plain(attack);
        parameters.releaseMs = plain(release);
        parameters.lowHz = plain(low);
        parameters.highHz = plain(high);
        parameters.freeze = plain(freeze) > 0.f;
        parameters.power = plain(power) > 0.f && values_[parameterCount] < 0.5f;
        // Pass through legacy plain Pan. The engines apply the original clamp.
        parameters.pan = plain(pan);
        parameters.ms = plain(ms) > 0.f;
        const auto frames = static_cast<int>(context.audio.frames);
        const auto channels = static_cast<int>(context.audio.channelCount);
        const auto sideChannels = static_cast<int>(context.sidechain.channelCount);
        const auto* main = context.audio.channel.data();
        const auto* sidechain = context.sidechain.channel.data();
        if (liveActive_) {
            live_.setParams(parameters);
            live_.process(main, sidechain, channels, sideChannels, frames);
        } else {
            studio_.setParams(parameters);
            studio_.process(main, sidechain, channels, sideChannels, frames);
        }
        return true;
    }

private:
    Values values_ = defaults();
    skell::SpacerEngine studio_;
    skell::ZeroLatencyEngine live_;
    bool liveActive_ = true;
    bool smoothAtConstructionDefault_ = true;
};

} // namespace Skelly::NativeSpacer
