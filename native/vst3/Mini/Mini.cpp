#include "RealtimeTuner.hpp"
#include "MiniEditor.h"
#include <cstring>
#include "../Source/ControlState.h"
#include "../Source/ControlParameter.h"
#include "../Source/ControlAutomation.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/main/pluginfactory_constexpr.h"
#include <array>
#include <atomic>
#include <immintrin.h>

namespace Skelly::NativeMini {
using namespace Steinberg;
using namespace Steinberg::Vst;

static DECLARE_UID(kProcessor, 0x781C7C4C, 0xF278447A, 0xA4279A90, 0x1761A4E2);
static DECLARE_UID(kController, 0xF987AB25, 0xF94C4646, 0xBE7E6D3C, 0x4DA5CC75);
constexpr auto product = "audio.skell.mini.native.dev";

using skellnative::sdk::NativeControl;
using skellnative::sdk::NativeControlFlags;
using skellnative::vst3::ControlParameter;

#include "MiniControls.inc"

using Values = std::array<float, controlCount>;
static_assert(std::size(controls) == controlCount);
static_assert(std::atomic<float>::is_always_lock_free);

std::optional<Values> readState(IBStream* stream) {
    return skellnative::vst3::readControlState(stream, product, controls);
}

class Processor final : public AudioEffect {
public:
    Processor() {
        setControllerClass(FUID::fromTUID(kController));
        for (std::size_t i = 0; i < controlCount; ++i)
            values_[i].store(controls[i].normalized(controls[i].initial));
    }

    tresult PLUGIN_API initialize(FUnknown* context) override {
        if (!skellnative::sdk::validControlTable(controls, controlCount))
            return kInvalidArgument;
        const auto result = AudioEffect::initialize(context);
        if (result != kResultOk) return result;
        addAudioInput(STR16("Input"), SpeakerArr::kStereo);
        addAudioOutput(STR16("Output"), SpeakerArr::kStereo);
        return kResultOk;
    }

    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* in, int32 nIn,
                                          SpeakerArrangement* out, int32 nOut) override {
        if (!in || !out || nIn != 1 || nOut != 1 || *in != *out
            || (*in != SpeakerArr::kMono && *in != SpeakerArr::kStereo)) return kResultFalse;
        return AudioEffect::setBusArrangements(in, nIn, out, nOut);
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override {
        if (setup.sampleRate < 8000. || setup.sampleRate > 192000. || !std::isfinite(setup.sampleRate) || setup.maxSamplesPerBlock <= 0
            || setup.symbolicSampleSize != kSample32) return kInvalidArgument;
        prepared_ = false;
        try {
            engine_.prepare(setup.sampleRate, 2);
            for (auto& channel : scratch_) channel.resize(setup.maxSamplesPerBlock);
        } catch (const std::bad_alloc&) {
            return kOutOfMemory;
        }
        const auto result = AudioEffect::setupProcessing(setup);
        prepared_ = result == kResultOk;
        return result;
    }

    uint32 PLUGIN_API getLatencySamples() override {
        return static_cast<uint32>(engine_.outputDelayContract(false).reportedSamples);
    }

    tresult PLUGIN_API setActive(TBool active) override {
        if (active && processSetup.maxSamplesPerBlock > 0) engine_.reset();
        return AudioEffect::setActive(active);
    }

    tresult PLUGIN_API setProcessing(TBool) override { return kResultOk; }

    tresult PLUGIN_API process(ProcessData& data) override {
        if (data.numSamples < 0 || data.symbolicSampleSize != kSample32) return kInvalidArgument;
        const auto apply = [this](std::size_t i, float value) {
            values_[i].store(value, std::memory_order_relaxed);
        };
        if (data.numSamples == 0)
            return skellnative::vst3::processControlAutomation(data.inputParameterChanges, 0, controls,
                apply, [](int32, int32) {}) ? kResultOk : kInvalidArgument;
        if (data.numSamples > processSetup.maxSamplesPerBlock || !prepared_ || !data.inputs || !data.outputs
            || data.numInputs != 1 || data.numOutputs != 1) return kInvalidArgument;
        const auto& input = data.inputs[0];
        auto& output = data.outputs[0];
        const int channels = input.numChannels;
        if (channels == 0 && output.numChannels == 0) return kResultOk;
        if (channels < 1 || channels > 2 || output.numChannels != channels || !output.channelBuffers32)
            return kInvalidArgument;

        std::array<const float*, 2> in{};
        for (int i = 0; i < channels; ++i) {
            if (!output.channelBuffers32[i]) return kInvalidArgument;
            const auto flag = uint64{1} << i;
            if (input.silenceFlags & flag) {
                std::fill_n(scratch_[i].data(), data.numSamples, 0.f);
                in[i] = scratch_[i].data();
            } else {
                if (!input.channelBuffers32 || !input.channelBuffers32[i]) return kInvalidArgument;
                in[i] = input.channelBuffers32[i];
            }
        }

        const auto control = _mm_getcsr();
        _mm_setcsr(control | 0x8040u);
        const bool valid = skellnative::vst3::processControlAutomation(
            data.inputParameterChanges, data.numSamples, controls, apply, [&](int32 start, int32 count) {
                const auto plain = [this](ControlIndex i) { return controls[i].plain(values_[i].load(std::memory_order_relaxed)); };
                skelly::Parameters params;
                params.speedMs = plain(control_speed);
                params.amount = plain(control_amount) / 100.f;
                params.mix = plain(control_bypass) >= 0.5f ? 0.f : plain(control_mix) / 100.f;
                params.referenceA = plain(control_a4);
                engine_.setParameters(params);
                std::array<const float*, 2> source{};
                std::array<float*, 2> destination{};
                for (int i = 0; i < channels; ++i) {
                    source[i] = in[i] + start;
                    destination[i] = output.channelBuffers32[i] + start;
                }
                engine_.process(source.data(), destination.data(), count, channels);
            });
        _mm_setcsr(control);
        if (!valid) return kInvalidArgument;
        output.silenceFlags = 0;
        for (int i = 0; i < channels; ++i) {
            const auto* first = output.channelBuffers32[i];
            if (std::all_of(first, first + data.numSamples, [](float x) { return x == 0.f; }))
                output.silenceFlags |= uint64{1} << i;
        }
        return kResultOk;
    }

    tresult PLUGIN_API setState(IBStream* state) override {
        const auto restored = readState(state);
        if (!restored) return kResultFalse;
        for (std::size_t i = 0; i < controlCount; ++i)
            values_[i].store((*restored)[i], std::memory_order_relaxed);
        return kResultOk;
    }

    tresult PLUGIN_API getState(IBStream* stream) override {
        return skellnative::vst3::writeControlState(stream, product, controls,
            [this](std::size_t i) { return values_[i].load(std::memory_order_relaxed); });
    }

private:
    bool prepared_ = false;
    std::array<std::atomic<float>, controlCount> values_{};
    std::array<std::vector<float>, 2> scratch_;
    skelly::RealtimeTuner engine_;
};

class Controller final : public EditController {
public:
    IPlugView* PLUGIN_API createView(FIDString name) override {
        if (!name || std::strcmp(name, ViewType::kEditor) != 0) return nullptr;
        return new MiniEditor(*this, controls, controlCount);
    }
    tresult PLUGIN_API initialize(FUnknown* context) override {
        if (!skellnative::sdk::validControlTable(controls, controlCount))
            return kInvalidArgument;
        const auto result = EditController::initialize(context);
        if (result != kResultOk) return result;
        for (const auto& spec : controls) parameters.addParameter(new ControlParameter(spec));
        return kResultOk;
    }
    tresult PLUGIN_API setComponentState(IBStream* state) override {
        const auto values = readState(state);
        if (!values) return kResultFalse;
        for (std::size_t i = 0; i < controlCount; ++i)
            setParamNormalized(controls[i].id, (*values)[i]);
        return kResultOk;
    }
};

FUnknown* createProcessor(void*) { return static_cast<IAudioProcessor*>(new Processor); }
FUnknown* createController(void*) { return static_cast<IEditController*>(new Controller); }
} // namespace Skelly::NativeMini

using namespace Steinberg;
using namespace Steinberg::Vst;
BEGIN_FACTORY_DEF("Skell Audio", "https://skellyr.uno", "info@skellyr.uno", 2)
DEF_CLASS(Skelly::NativeMini::kProcessor, PClassInfo::kManyInstances, kVstAudioEffectClass,
          "Skell Mini Native DEV", Vst::kDistributable, "Fx|Pitch Shift", "0.1.0",
          kVstVersionString, Skelly::NativeMini::createProcessor, nullptr)
DEF_CLASS(Skelly::NativeMini::kController, PClassInfo::kManyInstances, kVstComponentControllerClass,
          "Skell Mini Native DEV Edit", 0, "", "0.1.0", kVstVersionString,
          Skelly::NativeMini::createController, nullptr)
END_FACTORY
