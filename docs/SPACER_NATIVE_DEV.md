# Skell Spacer Native DEV

This opt-in Windows x64 pilot keeps the two engines from Spacer main
`05373baa6fb87ed870cb572d1fff888b1747daf7`. It configures that repository's
portable headers as an include directory and does not evaluate its JUCE build.
VST3 uses the pinned Steinberg SDK already used by SkellNative Gain. The editor
uses SkellDraw's GDI renderer.

| Product | Processor CID | Controller CID |
| --- | --- | --- |
| Legacy Spacer 1.0.0, local Windows build | ABCDEF019182FAEB536B4175536B3032 | ABCDEF011234ABCD536B4175536B3032 |
| Native DEV 0.1.0 | 0C6C714DBC9E4528A05AF80A2C219C48 | D692F31E5F754933A3B1B7F582C49CB2 |

Legacy vendor and product codes are `SkAu` and `Sk02`. DEV uses a separate
state identity and bundle. The controller uses the observed legacy numeric
parameter IDs. The audio adapter uses the last point per queue for the whole
block, like JUCE 8.0.12. Studio latency is the actual FFT size. DEV latches the
requested latency mode at setup or component activation. Host double buffers
are converted to the original float engine using preallocated scratch buffers.

## Local build and comparison

```powershell
cmake -S native/vst3 -B build-spacer -A x64 -DSKELLNATIVE_BUILD_SPACER=ON
cmake --build build-spacer --config Release --target SkellSpacerNative SpacerVst3Parity
& ./build-spacer/Release/SpacerVst3Parity.exe <legacy-bundle> <native-bundle>
& ./build-spacer/Release/SpacerVst3Parity.exe <legacy-bundle> <native-bundle> --capture <output-directory>
```

The build runs the official validator. The comparison harness loads both
compiled binaries through the VST3 SDK, compares parameter metadata and text,
then checks the two modes at five sample rates, seven block sizes, and six bus
combinations. Capture mode opens the actual editor views in a Windows test host.

## Updated ecosystem references

- SkellNative foundation `09be49ea11574d252f08f10dfc672676c1f8ce05` contains
  the upstream transfer of the Forge Gain prototype, strict Gain metadata checks,
  and separate portable buffer API. Spacer host integration lives downstream of it.
- Spacer V16 draft `e5c26811ecaef09eab6f789e299677f1b61377bb` is the newer
  candidate. It adds sidechain filters and retains the Link/LiveMode IDs as
  placeholders. It retires the historical Studio engine. A V16 port requires a
  separate baseline and an explicit migration decision for old Studio/Link sessions.
- Forge package-hardening draft `95fe566` improves the JUCE generator and
  packaging path. A native exporter remains pending.
- Core main `ad6fac2` contains the portable DSP target. Its DSP v0.7 draft
  adds limiter/FDN changes, and the Movement draft has reusable models and
  JUCE UI wrappers. Neither is required by the preserved Spacer DSP.
- SkellLang extraction `adc6334` tests the standalone v0.1 compiler. Its
  export is separate from the hand-ported compatibility pilot.

## Open gates

Legacy component-state import, input safety stress, allocation instrumentation,
FL Studio loading/PDC/automation/project recall, pixel parity, DPI matrix and
Forge export require further verification or implementation. The Win32 editor
is a first implementation of the reference layout and all four palettes.
Production Spacer and the V16 draft remain separate.

## November 15 local verification

| Gate | Status | Evidence |
| --- | --- | --- |
| Windows x64 Release, direct VST3 and SkellDraw | PASS | MSVC 19.51.36260, SDK 3.8.1. No MSVC warnings in the Spacer build log. |
| JUCE-free link graph | PASS | Spacer headers only, SDK and SkellNative/SkellDraw CMake targets. PE imports Windows system DLLs and the MSVC runtime. |
| Steinberg validator | PASS | 47 passed, 0 failed. Its sample-accuracy check reports unread intermediate points, expected from legacy block queue behavior. |
| Numeric parameter IDs, flags, defaults and sampled display strings | PASS | All 13 entries including host Bypass matched the compiled legacy controller. |
| Engine latency and Studio waveform | PASS | 420 case latency comparisons matched. Studio matched sample-for-sample in all 210 Studio cases. |
| Live bit-exact waveform | FAIL | 2,339,485 sample differences out of 14,751,450 combined samples. Overall peak error 2.38418579e-7 and RMS 1.51009558e-8. No tolerance was substituted for the exact gate. |
| CPU improvement | UNVERIFIED | Two runs' aggregate callback wall times varied. Standalone run totals were about 7.89 s legacy and 7.85 s native. Concurrent run totals were about 15.37 s legacy and 15.50 s native. |
| Native editor lifecycle smoke | PASS | The binary capture harness attached, sized and removed the actual editors repeatedly, at 495x405, 396x324 and 990x810. |
| Exact UI parity | FAIL | Inspected real 100% matched-size captures. Typography, antialiasing, resize affordance and finer spectrum rendering differ. Active processing, menus and 125/150/200% DPI remain unverified. |
| Legacy state and FL Studio | UNVERIFIED | Native state round-trip received validator coverage. Legacy state bytes were captured. Import and DAW session recall remain open. |
| Forge native exporter | UNVERIFIED | Forge hardening draft ran 84 tests with four skips. CMake is the current independent build entry point. |
| Shared SDK and optional World integration | PASS within pilot scope | No World runtime dependency. SkellNative portable audio suite and 9 metadata tests passed. Core Movement/FDN draft passed 16 portable CTests and SkellLang extraction passed 18 tests. |
| V16 compatibility decision and CPU | FAIL / UNVERIFIED | Published-ID guard passed. V16 MSVC DSP and stress signals passed, but the 48 kHz Live timing and the 192 kHz stress timing gates failed under concurrent local load. V16 drops Studio/Link semantics. |

The Windows test bundle is experimental. ZIP SHA-256
`90CA4B13223A994697C0A56234446491315056659D2AC37181AE58D1E3EE0896`.
Binary SHA-256 `CD466A22577EF8D8D52B6157D2ED3BEDF3B40A04C0DBC486BAD93903FFF92EA3`.

To investigate the remaining exact Live difference, use the binary harness
to find the first differing sample within one Live fixture, capture its
effective parameter values in both processors, then compare the first
differing 60-band control update. Continue keeping V16 measurements separate.

Spacer V16 `e5c2681` also built locally as Windows x64 Release with the cached
JUCE 8.0.12 source. Its compiled factory reports the same production CIDs as
Spacer 1.0.0. The independent SDK validator returned **46 passed, 1 failed**.
The Scan Programs check reports an unnamed program in the Factory Presets
list. V16 declares one program while PluginProcessor.h returns an empty name.
The V16 bundle was tested directly from its build directory.

## Follow-up: exact Live and Studio parity

Commit `016953a` preserves the legacy APVTS Smooth default before the first
effective edit. JUCE initializes the raw default to 30.000001907348633 and
uses snapped values after an edit. The prior native adapter snapped the
initial value to 30.0. This accounted for the Live waveform mismatch.

The reconstructed Windows x64 Release bundle passed all 47 validator checks.
Binary A/B passed two 420-case matrices with zero differing samples across
29,502,900 samples. The second matrix includes automation away from and back
to the Smooth default. The revised UI fills the track spectrum and the
editor capture host completed all 12 captures. Pixel parity, legacy state
import, and FL Studio verification remain open.

Updated DEV ZIP SHA-256:
`F1EB07C82B417D45FC6DAE2D783B869002C33CF91E58C6E9FC29BD1702CBCF53`.
