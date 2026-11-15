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
