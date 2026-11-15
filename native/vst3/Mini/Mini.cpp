#include "RealtimeTuner.hpp"
#include <SkellNative/State.h>
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

struct NativeControl {
    ParamID id;
    const TChar* title;
    const char* key;
    float low, high, initial, skew;

    float plain(float n) const { return low + (high - low) * std::pow(n, 1.f / skew); }
    float normalized(float p) const { return std::pow(std::clamp((p - low) / (high - low), 0.f, 1.f), skew); }
};

#include "MiniControls.inc"

using Values = std::array<float, controlCount>;
static_assert(std::size(controls) == controlCount);
static_assert(std::atomic<float>::is_always_lock_free);

std::optional<Values> readState(IBStream* stream) {
    if (!stream) return {};
    std::array<std::uint8_t, 512> chunk{};
    std::vector<std::uint8_t> bytes;
    for (;;) {
        int32 n = 0;
        const auto result = stream->read(chunk.data(), static_cast<int32>(chunk.size()), &n);
        if (n == 0) break;
        if (result != kResultOk || n < 0 || n > static_cast<int32>(chunk.size())
            || bytes.size() + static_cast<std::size_t>(n) > 4096) return {};
        bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + n);
    }
    const auto state = skellnative::sdk::decodeState(product, bytes);
    if (!state || state->parameters.size() != controlCount) return {};
    Values values{};
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto found = std::find_if(state->parameters.begin(), state->parameters.end(),
            [i](const auto& entry) { return entry.persistentId == controls[i].key; });
        if (found == state->parameters.end()) return {};
        values[i] = found->normalized;
    }
    return values;
}

class Processor final : public AudioEffect {
public:
    Processor() {
        setControllerClass(FUID::fromTUID(kController));
        for (std::size_t i = 0; i < controlCount; ++i)
            values_[i].store(controls[i].normalized(controls[i].initial));
    }

    tresult PLUGIN_API initialize(FUnknown* context) override {
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
        if (!(setup.sampleRate > 0) || !std::isfinite(setup.sampleRate) || setup.maxSamplesPerBlock <= 0
            || setup.symbolicSampleSize != kSample32) return kInvalidArgument;
        try {
            engine_.prepare(std::clamp(setup.sampleRate, 8000., 384000.), 2);
            for (auto& channel : scratch_) channel.resize(setup.maxSamplesPerBlock);
        } catch (const std::bad_alloc&) {
            return kOutOfMemory;
        }
        return AudioEffect::setupProcessing(setup);
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
        if (auto* changes = data.inputParameterChanges) {
            for (int32 q = 0; q < changes->getParameterCount(); ++q) {
                auto* queue = changes->getParameterData(q);
                if (!queue || queue->getPointCount() <= 0) continue;
                int32 offset{};
                ParamValue value{};
                if (queue->getPoint(queue->getPointCount() - 1, offset, value) == kResultOk && std::isfinite(value))
                    for (std::size_t i = 0; i < controlCount; ++i)
                        if (queue->getParameterId() == controls[i].id)
                            values_[i].store(static_cast<float>(std::clamp(value, 0., 1.)), std::memory_order_relaxed);
            }
        }
        if (data.numSamples == 0 || (data.numInputs == 0 && data.numOutputs == 0)) return kResultOk;
        if (data.symbolicSampleSize != kSample32 || data.numSamples < 0
            || data.numSamples > processSetup.maxSamplesPerBlock || !data.inputs || !data.outputs
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

        const auto plain = [this](ControlIndex i) { return controls[i].plain(values_[i].load(std::memory_order_relaxed)); };
        skelly::Parameters params;
        params.speedMs = plain(control_speed);
        params.amount = plain(control_amount) / 100.f;
        params.mix = plain(control_mix) / 100.f;
        if (plain(control_bypass) >= 0.5f) params.mix = 0.f;
        params.referenceA = plain(control_a4);
        engine_.setParameters(params);
        const auto control = _mm_getcsr();
        _mm_setcsr(control | 0x8040u);
        engine_.process(in.data(), output.channelBuffers32, data.numSamples, channels);
        _mm_setcsr(control);
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
        if (!stream) return kInvalidArgument;
        skellnative::sdk::StateSnapshot snapshot;
        for (std::size_t i = 0; i < controlCount; ++i)
            snapshot.parameters.push_back({controls[i].key, values_[i].load(std::memory_order_relaxed)});
        auto bytes = skellnative::sdk::encodeState(product, snapshot);
        if (!bytes) return kResultFalse;
        const auto size = static_cast<int32>(bytes->size());
        int32 written{};
        return stream->write(bytes->data(), size, &written) == kResultOk && written == size
            ? kResultOk : kResultFalse;
    }

private:
    std::array<std::atomic<float>, controlCount> values_{};
    std::array<std::vector<float>, 2> scratch_;
    skelly::RealtimeTuner engine_;
};

class ControlParameter final : public Parameter {
public:
    explicit ControlParameter(const NativeControl& spec) : spec_(spec) {
        auto title = spec.title;
        std::copy_n(title, std::char_traits<TChar>::length(title) + 1, info.title);
        info.id = spec.id;
        info.flags = ParameterInfo::kCanAutomate;
        if (spec.id == controls[control_bypass].id) {
            info.flags |= ParameterInfo::kIsBypass;
            info.stepCount = 1;
        }
        info.defaultNormalizedValue = spec.normalized(spec.initial);
        setNormalized(info.defaultNormalizedValue);
    }
    ParamValue toPlain(ParamValue value) const override { return spec_.plain(static_cast<float>(value)); }
    ParamValue toNormalized(ParamValue value) const override { return spec_.normalized(static_cast<float>(value)); }
    void toString(ParamValue value, String128 text) const override {
        TChar buffer[128]{};
        std::copy_n(buffer, 128, text);
        char ascii[64]{};
        std::snprintf(ascii, sizeof ascii, "%.2f", toPlain(value));
        for (std::size_t i = 0; ascii[i]; ++i) text[i] = static_cast<TChar>(ascii[i]);
    }
    bool fromString(const TChar* text, ParamValue& normalized) const override {
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
    const NativeControl& spec_;
};

class Controller final : public EditController {
public:
    tresult PLUGIN_API initialize(FUnknown* context) override {
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
