#include "StateBridge.h"
#include "Editor.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include <cwchar>
#include <cwctype>

namespace Skelly::NativeSpacer {
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
void copyText(String128 dest, const wchar_t* source) {
    std::size_t i = 0;
    for (; i < 127 && source[i]; ++i) dest[i] = static_cast<TChar>(source[i]);
    dest[i] = 0;
}

class SpacerParameter final : public Parameter {
public:
    explicit SpacerParameter(const ParameterSpec& spec, bool bypass = false) : spec_(spec) {
        info.id = bypass ? bypassId : spec.id();
        copyText(info.title, spec.title);
        wchar_t shortTitle[9]{};
        std::wcsncpy(shortTitle, spec.title, 8);
        copyText(info.shortTitle, shortTitle);
        copyText(info.units, spec.unit);
        info.stepCount = spec.mapping == Mapping::toggle ? 1 : 0;
        info.defaultNormalizedValue = spec.normalized(spec.initial);
        info.unitId = kRootUnitId;
        info.flags = ParameterInfo::kCanAutomate | (bypass ? ParameterInfo::kIsBypass : 0);
        setNormalized(info.defaultNormalizedValue);
    }

    // JUCE's VST3 controller exposes identity conversions here. Physical
    // units are applied when formatting text and in the processor.
    ParamValue toPlain(ParamValue value) const override { return value; }
    ParamValue toNormalized(ParamValue value) const override { return value; }

    void toString(ParamValue normalized, String128 dest) const override {
        wchar_t text[128]{};
        const auto plain = spec_.plain(static_cast<float>(normalized));
        if (spec_.mapping == Mapping::toggle)
            copyText(dest, plain > 0 ? L"On" : L"Off");
        else if (spec_.mapping == Mapping::frequency) {
            std::swprintf(text, 128, plain >= 1000 ? L"%.1f kHz" : L"%.1f Hz",
                          plain >= 1000 ? plain / 1000 : plain);
            copyText(dest, text);
        } else if (spec_.id() == specs[pan].id()) {
            const auto rounded = static_cast<long>(std::nearbyint(plain));
            if (rounded == 0) copyText(dest, L"C");
            else {
                std::swprintf(text, 128, L"%ld %lc", std::abs(rounded), rounded < 0 ? L'L' : L'R');
                copyText(dest, text);
            }
        } else {
            std::swprintf(text, 128, L"%ld%ls%ls", static_cast<long>(std::nearbyint(plain)),
                          spec_.unit[0] == L'%' ? L"" : L" ", spec_.unit);
            copyText(dest, text);
        }
    }

    bool fromString(const TChar* source, ParamValue& result) const override {
        wchar_t text[128]{};
        for (std::size_t i = 0; i < 127 && source[i]; ++i)
            text[i] = static_cast<wchar_t>(std::towlower(source[i]));
        if (spec_.mapping == Mapping::toggle) {
            if (std::wcscmp(text, L"on") == 0 || std::wcscmp(text, L"1") == 0
                || std::wcscmp(text, L"true") == 0) { result = 1; return true; }
            if (std::wcscmp(text, L"off") == 0 || std::wcscmp(text, L"0") == 0
                || std::wcscmp(text, L"false") == 0) { result = 0; return true; }
            return false;
        }
        const bool isPan = spec_.id() == specs[pan].id();
        if (isPan && std::wcscmp(text, L"c") == 0) { result = 0.5; return true; }
        wchar_t* end = nullptr;
        auto value = std::wcstof(text, &end);
        if (end == text || !std::isfinite(value)) return false;
        if (spec_.mapping == Mapping::frequency && std::wcschr(end, L'k')) value *= 1000.f;
        if (isPan && std::wcschr(end, L'l')) value = -std::abs(value);
        result = spec_.normalized(value);
        return true;
    }
private:
    const ParameterSpec& spec_;
};
}

class Controller final : public EditController, private EditorModel {
public:
    tresult PLUGIN_API initialize(FUnknown* context) override {
        if (const auto result = EditController::initialize(context); result != kResultOk) return result;
        for (const auto& spec : specs) parameters.addParameter(new SpacerParameter(spec));
        parameters.addParameter(new SpacerParameter(bypassSpec, true));
        spectrum_.main.fill(-120.f);
        spectrum_.side.fill(-120.f);
        return kResultOk;
    }

    tresult PLUGIN_API setComponentState(IBStream* stream) override {
        const auto state = readState(stream);
        if (!state) return kResultFalse;
        for (std::size_t i = 0; i < specs.size(); ++i)
            setParamNormalized(specs[i].id(), (*state)[i]);
        return setParamNormalized(bypassId, (*state)[parameterCount]);
    }

    IPlugView* PLUGIN_API createView(FIDString name) override {
        return name && std::strcmp(name, ViewType::kEditor) == 0
            ? new Editor(*this, static_cast<IEditController*>(this)) : nullptr;
    }

    tresult PLUGIN_API getState(IBStream* stream) override {
        std::array<std::uint8_t, 5> state{'S', 'K', 'T', '1', static_cast<std::uint8_t>(theme_)};
        int32 written = 0;
        return stream && stream->write(state.data(), 5, &written) == kResultOk && written == 5
            ? kResultOk : kResultFalse;
    }

    tresult PLUGIN_API setState(IBStream* stream) override {
        std::array<std::uint8_t, 5> state{};
        int32 read = 0;
        if (!stream || stream->read(state.data(), 5, &read) != kResultOk || read != 5
            || std::memcmp(state.data(), "SKT1", 4) || state[4] > 3) return kResultFalse;
        theme_ = state[4];
        return kResultOk;
    }

    tresult PLUGIN_API notify(IMessage* message) override {
        if (!message || !message->getMessageID()
            || std::strcmp(message->getMessageID(), spectrumResponse) != 0)
            return EditController::notify(message);
        const void* data = nullptr;
        uint32 size = 0;
        if (!message->getAttributes()
            || message->getAttributes()->getBinary("spectrum", data, size) != kResultOk
            || !data || size != sizeof(Spectrum)) return kResultFalse;
        std::memcpy(&spectrum_, data, size);
        return kResultOk;
    }

private:
    float get(Index index) override { return static_cast<float>(getParamNormalized(specs[index].id())); }
    void begin(Index index) override { beginEdit(specs[index].id()); }
    void edit(Index index, float value) override {
        const auto id = specs[index].id();
        setParamNormalized(id, value);
        performEdit(id, value);
    }
    void end(Index index) override { endEdit(specs[index].id()); }
    void text(Index index, String128 dest) override {
        const auto id = specs[index].id();
        getParamStringByValue(id, getParamNormalized(id), dest);
    }
    int theme() const override { return theme_; }
    void cycleTheme() override {
        theme_ = (theme_ + 1) % 4;
        if (componentHandler)
            if (FUnknownPtr<IComponentHandler2> handler{componentHandler})
                handler->setDirty(true);
    }
    Spectrum spectrum() override {
        if (getPeer()) sendMessageID(spectrumRequest);
        return spectrum_;
    }

    Spectrum spectrum_{};
    int theme_ = 0;
};

FUnknown* createController(void*) { return static_cast<IEditController*>(new Controller); }
} // namespace Skelly::NativeSpacer
