#include "../Source/ControlAutomation.h"
#include "../Source/ControlState.h"
#include "../Source/ControlParameter.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace skellnative::sdk;
using namespace skellnative::vst3;
constexpr NativeControl controls[]{
    {10, u"Gain", "gain", 0, 2, 1, 1, 0, u"x", NativeControlFlags::automate},
    {11, u"Bypass", "bypass", 0, 1, 0, 1, 1, u"", NativeControlFlags::bypass},
    {12, u"Meter", "meter", 0, 1, 0, 1, 0, u"", NativeControlFlags::readonly}
};
void require(bool ok) { if (!ok) throw std::runtime_error("Control runtime regression"); }
void point(ParameterChanges& changes, ParamID id, int32 offset, double value) {
    int32 index{};
    auto* queue = changes.addParameterData(id, index);
    require(queue && queue->addPoint(offset, value, index) == kResultOk);
}
int main() {
    try {
        require(controls[0].plain(-1) == 0 && controls[0].plain(2) == 2);
        require(controls[0].plain(std::numeric_limits<float>::quiet_NaN()) == 1);
        require(controls[1].canonical(.49f) == 0 && controls[1].canonical(.5f) == 1);
        ControlParameter bypass(controls[1]), meter(controls[2]);
        require(bypass.getInfo().stepCount == 1 && (bypass.getInfo().flags & ParameterInfo::kIsBypass));
        require(meter.getInfo().flags == ParameterInfo::kIsReadOnly);
        std::array<float, 3> state{.5f, 0, 0};
        std::array<float, 16> output{};
        auto apply = [&](std::size_t i, float value) { state[i] = value; };
        auto render = [&](int32 start, int32 count) { std::fill_n(output.data()+start, count, state[0]); };
        ParameterChanges changes(3);
        point(changes, 10, 0, .25); point(changes, 10, 7, .75); point(changes, 10, 15, 1);
        point(changes, 11, 8, .6); point(changes, 12, 0, 1);
        require(processControlAutomation(&changes, 16, controls, apply, render));
        for (int i=0; i<16; ++i) require(output[i] == (i<7 ? .25f : i<15 ? .75f : 1.f));
        require(state[1] == 1 && state[2] == 0);
        changes.clearQueue(); point(changes, 10, 16, 0);
        require(!processControlAutomation(&changes, 16, controls, apply, render));
        require(state[0] == 1 && output[0] == .25f);
        changes.clearQueue(); point(changes, 10, 0, std::numeric_limits<double>::quiet_NaN());
        require(!processControlAutomation(&changes, 16, controls, apply, render));
        changes.clearQueue(); point(changes, 10, 0, .2);
        require(processControlAutomation(&changes, 0, controls, apply, render));
        require(state[0] == .2f);
        MemoryStream stream;
        require(writeControlState(&stream, "test.controls", controls, [&](std::size_t i) {return state[i];}) == kResultOk);
        stream.seek(0, IBStream::kIBSeekSet, nullptr);
        const auto restored = readControlState(&stream, "test.controls", controls);
        require(restored && *restored == state);
        stream.seek(0, IBStream::kIBSeekSet, nullptr);
        require(!readControlState(&stream, "wrong.product", controls));
        stream.getData()[stream.getSize()-1] ^= 1;
        stream.seek(0, IBStream::kIBSeekSet, nullptr);
        require(!readControlState(&stream, "test.controls", controls));
        auto older = encodeState("test.controls", {{{"gain", .75f}}});
        MemoryStream oldStream(older->data(), static_cast<TSize>(older->size()));
        const auto upgraded = readControlState(&oldStream, "test.controls", controls);
        require(upgraded && (*upgraded)[0] == .75f && (*upgraded)[1] == 0);
        auto unknown = encodeState("test.controls", {{{"future", .5f}}});
        MemoryStream futureStream(unknown->data(), static_cast<TSize>(unknown->size()));
        require(!readControlState(&futureStream, "test.controls", controls));
        require(validControlTable(controls, std::size(controls)));
        std::array<NativeControl, std::size(controls)> bad{};
        std::copy(std::begin(controls), std::end(controls), bad.begin());
        bad[1].id = bad[0].id;
        require(!validControlTable(bad.data(), bad.size()));
        bad[1] = controls[1];
        bad[1].key = controls[0].key;
        require(!validControlTable(bad.data(), bad.size()));
        bad[1] = controls[1];
        bad[0].skew = 0;
        require(!validControlTable(bad.data(), bad.size()));
        bad[0] = controls[0];
        bad[0].initial = 4;
        require(!validControlTable(bad.data(), bad.size()));
        bad[0] = controls[0];
        bad[0].low = bad[0].high;
        require(!validControlTable(bad.data(), bad.size()));
        bad[0] = controls[0];
        bad[1].steps = 0;
        require(!validControlTable(bad.data(), bad.size()));
        bad[1] = controls[1];
        bad[2].steps = 1;
        require(!validControlTable(bad.data(), bad.size()));
        bad[2] = controls[2];
        require(validControlTable(bad.data(), bad.size()));
        // Invalid automation must not touch previously published state/output.
        const auto priorState = state;
        const auto priorOutput = output;
        changes.clearQueue();
        point(changes, 10, 17, .5);
        require(!processControlAutomation(&changes, 16, controls, apply, render));
        require(state == priorState && output == priorOutput);
        changes.clearQueue();
        point(changes, 10, 0, std::numeric_limits<double>::infinity());
        require(!processControlAutomation(&changes, 16, controls, apply, render));
        require(state == priorState && output == priorOutput);
        std::cout << "PASS control metadata, boundaries, sample offsets, flush, state recall, corruption and append migration\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
