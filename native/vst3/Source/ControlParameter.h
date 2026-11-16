#pragma once
#include <SkellNative/Controls.h>
#include "public.sdk/source/vst/vstparameters.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace skellnative::vst3 {
class ControlParameter final : public Steinberg::Vst::Parameter {
public:
    explicit ControlParameter(const sdk::NativeControl& spec) : spec_(spec) {
        std::copy_n(spec.title, std::min(std::char_traits<char16_t>::length(spec.title), std::size_t{127}), info.title);
        std::copy_n(spec.units, std::min(std::char_traits<char16_t>::length(spec.units), std::size_t{127}), info.units);
        info.id = spec.id;
        using Info = Steinberg::Vst::ParameterInfo;
        info.flags = spec.flags == sdk::NativeControlFlags::readonly ? Info::kIsReadOnly : Info::kCanAutomate;
        if (spec.flags == sdk::NativeControlFlags::bypass) info.flags |= Info::kIsBypass;
        info.stepCount = spec.steps;
        info.defaultNormalizedValue = spec.normalized(spec.initial);
        setNormalized(info.defaultNormalizedValue);
    }
    Steinberg::Vst::ParamValue toPlain(Steinberg::Vst::ParamValue value) const override { return spec_.plain(static_cast<float>(value)); }
    Steinberg::Vst::ParamValue toNormalized(Steinberg::Vst::ParamValue value) const override { return spec_.normalized(static_cast<float>(value)); }
    void toString(Steinberg::Vst::ParamValue value, Steinberg::Vst::String128 text) const override {
        std::fill_n(text, 128, char16_t{});
        char ascii[64]{};
        std::snprintf(ascii, sizeof ascii, "%.2f", toPlain(value));
        for (std::size_t i = 0; ascii[i]; ++i) text[i] = static_cast<char16_t>(ascii[i]);
    }
    bool fromString(const Steinberg::Vst::TChar* text, Steinberg::Vst::ParamValue& normalized) const override {
        if (!text) return false;
        std::array<char, 128> ascii{};
        std::size_t i = 0;
        for (; i < ascii.size() - 1 && text[i]; ++i) {
            if (text[i] > 127) return false;
            ascii[i] = static_cast<char>(text[i]);
        }
        if (text[i]) return false;
        char* end = nullptr;
        const float value = std::strtof(ascii.data(), &end);
        if (end == ascii.data() || *end != 0 || !std::isfinite(value)) return false;
        normalized = spec_.normalized(value);
        return true;
    }
private:
    sdk::NativeControl spec_;
};
} // namespace skellnative::vst3
