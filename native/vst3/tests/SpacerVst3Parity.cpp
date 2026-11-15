#include "../Spacer/Parameters.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace spacer = Skelly::NativeSpacer;

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

class PluginInstance {
public:
    explicit PluginInstance(const char* path) {
        std::string error;
        module_ = VST3::Hosting::Module::create(path, error);
        if (!module_) throw std::runtime_error(error);
        const auto& factory = module_->getFactory();
        for (const auto& info : factory.classInfos()) {
            std::cout << info.category() << " " << info.ID().toString() << " " << info.name() << '\n';
            if (info.category() == kVstAudioEffectClass) {
                provider_ = owned(new PlugProvider(factory, info));
                require(provider_->initialize(), "PlugProvider initialize");
            }
        }
        require(provider_ != nullptr, "Missing audio effect");
        component = provider_->getComponentPtr();
        controller = provider_->getControllerPtr();
        processor = FUnknownPtr<IAudioProcessor>(component);
        require(component && controller && processor, "Missing plugin interface");
    }

    void prepare(double rate, int block, bool live, int main, int side, int32 format) {
        SpeakerArrangement input[]{main == 1 ? SpeakerArr::kMono : SpeakerArr::kStereo,
                                   side == 1 ? SpeakerArr::kMono : SpeakerArr::kStereo};
        SpeakerArrangement output[]{input[0]};
        require(processor->setBusArrangements(input, 2, output, 1) == kResultOk, "Bus arrangement");
        require(component->activateBus(kAudio, kInput, 0, true) == kResultOk, "Main input");
        require(component->activateBus(kAudio, kInput, 1, side > 0) == kResultOk, "Sidechain");
        require(component->activateBus(kAudio, kOutput, 0, true) == kResultOk, "Main output");
        ProcessSetup setup{kOffline, format, block, rate};
        require(processor->setupProcessing(setup) == kResultOk, "Setup");
        ParameterChanges changes(1);
        add(changes, spacer::live, 0, live ? 1.f : 0.f);
        ProcessData flush{};
        flush.symbolicSampleSize = format;
        flush.inputParameterChanges = &changes;
        require(processor->process(flush) == kResultOk, "Mode flush");
        require(component->setActive(true) == kResultOk, "Set active");
        require(processor->setProcessing(true) == kResultOk, "Set processing");
    }

    void stop() {
        require(processor->setProcessing(false) == kResultOk, "Stop processing");
        require(component->setActive(false) == kResultOk, "Deactivate");
    }

    static void add(ParameterChanges& changes, spacer::Index index, int offset, float normalized) {
        int32 entry{};
        auto* queue = changes.addParameterData(spacer::specs[index].id(), entry);
        require(queue && queue->addPoint(offset, normalized, entry) == kResultOk, "Add automation");
    }

private:
    VST3::Hosting::Module::Ptr module_;
    IPtr<PlugProvider> provider_;
public:
    IPtr<IComponent> component;
    IPtr<IEditController> controller;
    IPtr<IAudioProcessor> processor;
};

void compareParameters(PluginInstance& legacy, PluginInstance& native) {
    require(legacy.controller->getParameterCount() == native.controller->getParameterCount(),
            "Parameter count mismatch");
    std::cout << std::setprecision(9);
    for (int i = 0; i < legacy.controller->getParameterCount(); ++i) {
        ParameterInfo ref{}, actual{};
        require(legacy.controller->getParameterInfo(i, ref) == kResultOk
                    && native.controller->getParameterInfo(i, actual) == kResultOk, "Parameter info");
        std::cout << "index=" << i << " id=" << ref.id << " default=" << ref.defaultNormalizedValue
                  << " steps=" << ref.stepCount << " flags=" << ref.flags << '\n';
        require(ref.id == actual.id && ref.defaultNormalizedValue == actual.defaultNormalizedValue
                    && ref.stepCount == actual.stepCount && ref.flags == actual.flags, "Parameter identity");
        for (const double value : {0.0, 0.01, 0.2, 0.5, 0.825, 1.0}) {
            String128 expected{}, observed{};
            require(legacy.controller->getParamStringByValue(ref.id, value, expected) == kResultOk
                        && native.controller->getParamStringByValue(ref.id, value, observed) == kResultOk,
                    "Parameter text");
            std::u16string lhs(expected), rhs(observed);
            if (lhs != rhs) {
                std::cerr << "Text at " << value << ": [" << std::string(lhs.begin(), lhs.end())
                          << "] vs [" << std::string(rhs.begin(), rhs.end()) << "]\n";
                std::cerr << "Parameter text mismatch for ID " << ref.id << '\n';
                throw std::runtime_error("Parameter text parity");
            }
        }
    }
}

template<class Sample>
struct Buffers {
    std::array<std::vector<Sample>, 2> inputs, sidechain, outputs;
    std::array<Sample*, 2> inputPtrs{}, sidePtrs{}, outputPtrs{};
    AudioBusBuffers in[2]{}, out{};
    ProcessData data{};

    Buffers(int block, int main, int side) {
        for (int i = 0; i < 2; ++i) {
            inputs[i].resize(block);
            sidechain[i].resize(block);
            outputs[i].resize(block);
            inputPtrs[i] = inputs[i].data();
            sidePtrs[i] = sidechain[i].data();
            outputPtrs[i] = outputs[i].data();
        }
        in[0].numChannels = out.numChannels = main;
        in[1].numChannels = side;
        if constexpr (std::is_same_v<Sample, float>) {
            in[0].channelBuffers32 = inputPtrs.data();
            in[1].channelBuffers32 = sidePtrs.data();
            out.channelBuffers32 = outputPtrs.data();
            data.symbolicSampleSize = kSample32;
        } else {
            in[0].channelBuffers64 = inputPtrs.data();
            in[1].channelBuffers64 = sidePtrs.data();
            out.channelBuffers64 = outputPtrs.data();
            data.symbolicSampleSize = kSample64;
        }
        data.processMode = kOffline;
        data.numInputs = 2;
        data.numOutputs = 1;
        data.inputs = in;
        data.outputs = &out;
    }

    void fill(int start, int count, double rate) {
        data.numSamples = count;
        for (int i = 0; i < count; ++i) {
            const double t = (start + i) / rate;
            constexpr double tau = 6.283185307179586;
            const auto sweep = std::sin(tau * (200.0 * t + 1600.0 * t * t));
            const auto tone = std::sin(tau * 1000.0 * t);
            const auto transient = start + i == 11 ? 0.3 : 0.0;
            inputs[0][i] = static_cast<Sample>(t < 0.19 ? transient + 0.17 * tone + 0.13 * sweep : 0.0);
            inputs[1][i] = static_cast<Sample>(t < 0.18 ? 0.21 * tone - 0.10 * sweep : 0.0);
            sidechain[0][i] = static_cast<Sample>(t > 0.025 && t < 0.12 ? 0.3 * tone : 0.0);
            sidechain[1][i] = static_cast<Sample>(t > 0.04 && t < 0.15 ? 0.2 * sweep : 0.0);
        }
    }
};

void compareAudio(PluginInstance& legacy, PluginInstance& native) {
    double totalSquared = 0.0, peak = 0.0;
    std::size_t count = 0, cases = 0, mismatches = 0;
    std::chrono::steady_clock::duration legacyTime{}, nativeTime{};
    for (const bool live : {false, true}) {
        const auto mismatchesBeforeMode = mismatches;
        for (const double rate : {44100., 48000., 88200., 96000., 192000.}) {
            for (const int block : {16, 32, 64, 128, 256, 512, 1024}) {
                for (const int main : {1, 2}) {
                    for (const int side : {0, 1, 2}) {
                        legacy.prepare(rate, block, live, main, side, kSample32);
                        native.prepare(rate, block, live, main, side, kSample32);
                        require(legacy.processor->getLatencySamples() == native.processor->getLatencySamples(),
                                "Latency mismatch");
                        Buffers<float> a(block, main, side), b(block, main, side);
                        const auto frames = static_cast<int>(rate / 4);
                        ParameterChanges changes(spacer::parameterCount);
                        int stage = -1;
                        for (int start = 0; start < frames; start += block) {
                            changes.clearQueue();
                            const int next = std::min(5, 6 * start / frames);
                            if (next != stage) {
                                stage = next;
                                for (std::size_t i = 0; i < spacer::specs.size(); ++i) {
                                    if (i == spacer::live) continue;
                                    const auto& spec = spacer::specs[i];
                                    const auto initial = spec.normalized(spec.initial);
                                    PluginInstance::add(changes, static_cast<spacer::Index>(i), 0, initial);
                                }
                                PluginInstance::add(changes, spacer::amount, 1, stage == 0 ? 0.95f : 0.7f);
                                PluginInstance::add(changes, spacer::attack, 1, 0.61f);
                                PluginInstance::add(changes, spacer::release, 1, 0.29f);
                                PluginInstance::add(changes, spacer::low, 1, 0.11f);
                                PluginInstance::add(changes, spacer::high, 1, 0.9f);
                                PluginInstance::add(changes, spacer::link, 1, stage >= 2 ? 0.53f : 0.f);
                                PluginInstance::add(changes, spacer::pan, 1, stage == 3 ? 0.505f : 0.5f);
                                PluginInstance::add(changes, spacer::ms, 1, stage == 3 ? 1.f : 0.f);
                                PluginInstance::add(changes, spacer::freeze, 1, stage == 3 ? 1.f : 0.f);
                                PluginInstance::add(changes, spacer::power, 1, stage >= 4 ? 0.f : 1.f);
                            }
                            const auto n = std::min(block, frames - start);
                            a.fill(start, n, rate);
                            b.fill(start, n, rate);
                            a.data.inputParameterChanges = b.data.inputParameterChanges = &changes;
                            auto tick = std::chrono::steady_clock::now();
                            require(legacy.processor->process(a.data) == kResultOk, "Legacy process");
                            legacyTime += std::chrono::steady_clock::now() - tick;
                            tick = std::chrono::steady_clock::now();
                            require(native.processor->process(b.data) == kResultOk, "Native process");
                            nativeTime += std::chrono::steady_clock::now() - tick;
                            for (int ch = 0; ch < main; ++ch) {
                                for (int i = 0; i < n; ++i) {
                                    require(std::isfinite(a.outputs[ch][i]) && std::isfinite(b.outputs[ch][i]),
                                            "Nonfinite output");
                                    const auto error = static_cast<double>(a.outputs[ch][i]) - b.outputs[ch][i];
                                    peak = std::max(peak, std::abs(error));
                                    totalSquared += error * error;
                                    mismatches += error != 0.0;
                                    ++count;
                                }
                            }
                        }
                        legacy.stop();
                        native.stop();
                        ++cases;
                    }
                }
            }
        }
        std::cout << (live ? "Live" : "Studio") << " mismatched samples "
                  << mismatches - mismatchesBeforeMode << '\n';
    }
    std::cout << "Cases " << cases << ", samples " << count << ", mismatches " << mismatches
              << ", peak error " << peak << ", RMS error " << std::sqrt(totalSquared / count)
              << ", legacy callback ms " << std::chrono::duration<double, std::milli>(legacyTime).count()
              << ", native callback ms " << std::chrono::duration<double, std::milli>(nativeTime).count() << '\n';
    require(mismatches == 0, "Waveform parity");
}

#include "SpacerEditorCapture.h"

int main(int argc, char** argv) {
    if (argc < 3 || argc > 5 || (argc == 5 && std::string(argv[3]) != "--capture")) {
        std::cerr << "Usage: SpacerVst3Parity <legacy.vst3> <native.vst3> [state.bin | --capture dir]\n";
        return EXIT_FAILURE;
    }
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        HostApplication context;
        PluginContextFactory::instance().setPluginContext(&context);
        PluginInstance legacy(argv[1]), native(argv[2]);
        if (argc == 5) {
            const std::filesystem::path out(argv[4]);
            legacy.prepare(48000., 512, true, 2, 0, kSample32);
            native.prepare(48000., 512, true, 2, 0, kSample32);
            spacer_capture::capture(*legacy.controller, out / "legacy-default", true);
            spacer_capture::capture(*native.controller, out / "native-default", true);
            spacer_capture::capture(*legacy.controller, out / "legacy-min", false, 396, 324);
            spacer_capture::capture(*native.controller, out / "native-min", false, 396, 324);
            spacer_capture::capture(*legacy.controller, out / "legacy-max", false, 990, 810);
            spacer_capture::capture(*native.controller, out / "native-max", false, 990, 810);
            legacy.stop();
            native.stop();
            return EXIT_SUCCESS;
        }
        if (argc == 4) {
            MemoryStream state;
            require(legacy.component->getState(&state) == kResultOk, "Read legacy state");
            std::ofstream file(argv[3], std::ios::binary);
            file.write(state.getData(), state.getSize());
            require(file.good(), "Write state evidence");
        }
        compareParameters(legacy, native);
        compareAudio(legacy, native);
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
