#include "IDs.h"
#include "SpacerProcessor.h"
#include "StateBridge.h"
#include "Spectrum.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include <immintrin.h>
#include <type_traits>

namespace Skelly::NativeSpacer {
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
class ScopedNoDenormals {
public:
    ScopedNoDenormals() noexcept : previous_(_mm_getcsr()) { _mm_setcsr(previous_ | 0x8040u); }
    ~ScopedNoDenormals() { _mm_setcsr(previous_); }
private:
    unsigned previous_;
};

constexpr bool monoOrStereo(SpeakerArrangement layout) {
    return layout == SpeakerArr::kMono || layout == SpeakerArr::kStereo;
}
}

class ProcessorAdapter final : public AudioEffect {
public:
    ProcessorAdapter() {
        setControllerClass(FUID::fromTUID(kControllerUID));
        const auto initial = defaults();
        for (std::size_t i = 0; i < initial.size(); ++i) values_[i].store(initial[i]);
    }

    tresult PLUGIN_API initialize(FUnknown* context) override {
        if (const auto result = AudioEffect::initialize(context); result != kResultOk) return result;
        addAudioInput(STR16("Input"), SpeakerArr::kStereo);
        addAudioInput(STR16("Sidechain"), SpeakerArr::kStereo, kAux, 0);
        addAudioOutput(STR16("Output"), SpeakerArr::kStereo);
        return kResultOk;
    }

    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* ins, int32 numIn,
                                           SpeakerArrangement* outs, int32 numOut) override {
        if (!ins || !outs || (numIn != 1 && numIn != 2) || numOut != 1
            || ins[0] != outs[0] || !monoOrStereo(ins[0])
            || (numIn == 2 && ins[1] != SpeakerArr::kEmpty && !monoOrStereo(ins[1])))
            return kResultFalse;
        getAudioInput(0)->setArrangement(ins[0]);
        getAudioOutput(0)->setArrangement(outs[0]);
        if (numIn == 2) getAudioInput(1)->setArrangement(ins[1]);
        return kResultOk;
    }

    tresult PLUGIN_API canProcessSampleSize(int32 size) override {
        return size == kSample32 || size == kSample64 ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override {
        if (setup.maxSamplesPerBlock <= 0 || canProcessSampleSize(setup.symbolicSampleSize) != kResultTrue)
            return kInvalidArgument;
        try {
            const auto maxFrames = static_cast<std::size_t>(setup.maxSamplesPerBlock);
            for (auto& channel : mainScratch_) channel.resize(maxFrames);
            for (auto& channel : sideScratch_) channel.resize(maxFrames);
        } catch (...) {
            return kOutOfMemory;
        }
        processor_.configure(getValues());
        if (!processor_.prepare(setup.sampleRate, static_cast<std::uint32_t>(setup.maxSamplesPerBlock)))
            return kResultFalse;
        sampleRate_.store(setup.sampleRate, std::memory_order_relaxed);
        latency_.store(processor_.latencySamples(), std::memory_order_relaxed);
        return AudioEffect::setupProcessing(setup);
    }

    tresult PLUGIN_API setActive(TBool active) override {
        if (active) {
            processor_.configure(getValues());
            processor_.latchMode();
            processor_.reset();
            sidechainHold_.store(0, std::memory_order_relaxed);
            latency_.store(processor_.latencySamples(), std::memory_order_relaxed);
        }
        return AudioEffect::setActive(active);
    }

    uint32 PLUGIN_API getLatencySamples() override { return latency_.load(std::memory_order_relaxed); }
    uint32 PLUGIN_API getTailSamples() override { return getLatencySamples(); }
    tresult PLUGIN_API setProcessing(TBool) override { return kResultOk; }

    tresult PLUGIN_API notify(IMessage* message) override {
        if (!message || !message->getMessageID()
            || std::strcmp(message->getMessageID(), spectrumRequest) != 0)
            return AudioEffect::notify(message);
        // Only the controller's UI timer asks for a snapshot. The callback
        // publishes atomic values and never sends a message.
        Spectrum spectrum{};
        spectrum.latency = getLatencySamples();
        spectrum.rate = sampleRate_.load(std::memory_order_relaxed);
        spectrum.sidechainActive = sidechainHold_.load(std::memory_order_relaxed) > 0;
        processor_.readSpectrum(spectrum.latency == 0, spectrum.main.data(),
                                spectrum.side.data(), spectrum.cut.data());
        auto reply = owned(allocateMessage());
        if (!reply || !reply->getAttributes()) return kResultFalse;
        reply->setMessageID(spectrumResponse);
        if (reply->getAttributes()->setBinary("spectrum", &spectrum, sizeof spectrum) != kResultOk)
            return kResultFalse;
        return sendMessage(reply);
    }

    tresult PLUGIN_API process(ProcessData& data) override {
        if (data.numSamples < 0 || data.numSamples > processSetup.maxSamplesPerBlock)
            return kInvalidArgument;
        applyBlockParameters(data.inputParameterChanges);
        // Some hosts pass a bufferless process call to flush automation.
        if (!data.numSamples || (data.numInputs == 0 && data.numOutputs == 0)) return kResultOk;
        if (!data.inputs || !data.outputs || data.numInputs < 1 || data.numOutputs != 1)
            return kInvalidArgument;
        auto& input = data.inputs[0];
        auto& output = data.outputs[0];
        if (input.numChannels == 0 && output.numChannels == 0) return kResultOk;
        if (input.numChannels != output.numChannels || input.numChannels < 1 || input.numChannels > 2)
            return kInvalidArgument;
        const auto* side = data.numInputs >= 2 && getAudioInput(1)->isActive() ? &data.inputs[1] : nullptr;
        const ScopedNoDenormals guard;
        if (data.symbolicSampleSize == kSample32)
            return processAudio<float>(data, side);
        if (data.symbolicSampleSize == kSample64)
            return processAudio<double>(data, side);
        return kInvalidArgument;
    }

    tresult PLUGIN_API setState(IBStream* stream) override {
        const auto state = readState(stream);
        if (!state) return kResultFalse;
        for (std::size_t i = 0; i < state->size(); ++i)
            values_[i].store((*state)[i], std::memory_order_relaxed);
        return kResultOk;
    }

    tresult PLUGIN_API getState(IBStream* stream) override {
        return writeState(stream, getValues()) ? kResultOk : kResultFalse;
    }

private:
    Values getValues() const noexcept {
        Values values;
        for (std::size_t i = 0; i < values.size(); ++i)
            values[i] = values_[i].load(std::memory_order_relaxed);
        return values;
    }

    void applyBlockParameters(IParameterChanges* changes) noexcept {
        if (!changes) return;
        const auto count = changes->getParameterCount();
        for (int32 i = 0; i < count; ++i) {
            auto* queue = changes->getParameterData(i);
            if (!queue || queue->getPointCount() <= 0) continue;
            const auto id = queue->getParameterId();
            std::size_t index = 0;
            while (index < specs.size() && specs[index].id() != id) ++index;
            if (index == specs.size() && id != bypassId) continue;
            // Match the JUCE reference: the last point applies to the whole block.
            int32 offset = 0;
            ParamValue normalized = 0;
            if (queue->getPoint(queue->getPointCount() - 1, offset, normalized) == kResultOk
                && std::isfinite(normalized)) {
                values_[index].store(static_cast<float>(std::clamp(normalized, 0.0, 1.0)),
                                     std::memory_order_relaxed);
            }
        }
    }

    template<class Sample>
    static Sample** channels(const AudioBusBuffers& bus) noexcept {
        if constexpr (std::is_same_v<Sample, float>) return bus.channelBuffers32;
        else return bus.channelBuffers64;
    }

    template<class Sample>
    static bool validBus(const AudioBusBuffers& bus, bool output) noexcept {
        if (bus.numChannels < 0 || bus.numChannels > 2) return false;
        for (int32 i = 0; i < bus.numChannels; ++i) {
            if (!output && (bus.silenceFlags & (uint64{1} << i))) continue;
            if (!channels<Sample>(bus) || !channels<Sample>(bus)[i]) return false;
        }
        return true;
    }

    template<class Sample>
    static void readChannel(const AudioBusBuffers& bus, int32 channel, int32 frames, float* dest) noexcept {
        if (bus.silenceFlags & (uint64{1} << channel)) {
            std::fill_n(dest, frames, 0.f);
            return;
        }
        const auto* source = channels<Sample>(bus)[channel];
        for (int32 i = 0; i < frames; ++i) {
            const auto sample = static_cast<float>(source[i]);
            dest[i] = std::isfinite(sample) ? sample : 0.f;
        }
    }

    template<class Sample>
    tresult processAudio(ProcessData& data, const AudioBusBuffers* side) noexcept {
        auto& input = data.inputs[0];
        auto& output = data.outputs[0];
        if (!validBus<Sample>(input, false) || !validBus<Sample>(output, true)
            || (side && !validBus<Sample>(*side, false))) return kInvalidArgument;

        skellnative::sdk::ProcessContext context{};
        // The engines were prepared with the actual host rate. The portable
        // context's nominal rate is bounded to the SDK contract.
        context.sampleRate = std::min(processSetup.sampleRate, 384000.0);
        context.audio.frames = static_cast<std::size_t>(data.numSamples);
        context.audio.channelCount = static_cast<std::size_t>(input.numChannels);
        bool hasSignal = false;
        if (side) {
            context.sidechain.channelCount = static_cast<std::size_t>(side->numChannels);
            for (int32 i = 0; i < side->numChannels; ++i) {
                auto* scratch = sideScratch_[i].data();
                readChannel<Sample>(*side, i, data.numSamples, scratch);
                context.sidechain.channel[i] = scratch;
                if (!hasSignal)
                    hasSignal = std::any_of(scratch, scratch + data.numSamples,
                                            [](float x) { return std::abs(x) > 1.e-4f; });
            }
        }
        const auto countdown = sidechainHold_.load(std::memory_order_relaxed);
        sidechainHold_.store(hasSignal ? static_cast<int>(std::min(processSetup.sampleRate, 384000.0))
                                       : std::max(0, countdown - data.numSamples),
                            std::memory_order_relaxed);
        for (int32 i = 0; i < input.numChannels; ++i) {
            float* dest = mainScratch_[i].data();
            if constexpr (std::is_same_v<Sample, float>) dest = channels<Sample>(output)[i];
            readChannel<Sample>(input, i, data.numSamples, dest);
            context.audio.channel[i] = dest;
        }

        processor_.configure(getValues());
        if (!processor_.process(context)) return kResultFalse;

        // Studio's delay line can produce audio when the new input is silent.
        output.silenceFlags = 0;
        for (int32 i = 0; i < output.numChannels; ++i) {
            const float* processed = context.audio.channel[i];
            auto* dest = channels<Sample>(output)[i];
            bool silent = true;
            for (int32 n = 0; n < data.numSamples; ++n) {
                const float value = std::isfinite(processed[n]) ? processed[n] : 0.f;
                dest[n] = static_cast<Sample>(value);
                silent &= value == 0.f;
            }
            if (silent) output.silenceFlags |= uint64{1} << i;
        }
        return kResultOk;
    }

    static_assert(std::atomic<float>::is_always_lock_free);
    std::array<std::atomic<float>, parameterCount + 1> values_{};
    std::atomic<uint32> latency_{0};
    std::atomic<double> sampleRate_{48000.0};
    std::atomic<int> sidechainHold_{0};
    SpacerProcessor processor_;
    std::array<std::vector<float>, 2> mainScratch_, sideScratch_;
};

FUnknown* createProcessor(void*) {
    try {
        return static_cast<IAudioProcessor*>(new ProcessorAdapter);
    } catch (...) {
        return nullptr;
    }
}
} // namespace Skelly::NativeSpacer
