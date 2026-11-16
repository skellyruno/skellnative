# Skell Mini Native DEV

A Windows x64 VST3 pitch-correction pilot using the original portable
RealtimeTuner from SkellMini commit `6a1c0267967cab696f8d3b3371e9c40a4ed75a78`.
The build uses standalone SkellLang to generate the VST3 parameter definitions
from `native/vst3/Mini/Mini.skell`.

The DEV product uses separate factory IDs, mono/stereo float32 processing,
chromatic tuning, speed, amount, wet mix, A4 reference and latency-aligned VST3
bypass. The host supplies a generic parameter interface. Its native state is
versioned and separate from legacy Mini state.

```powershell
cmake -S native/vst3 -B build-mini -A x64 -DSKELLNATIVE_BUILD_MINI=ON
cmake --build build-mini --config Release --target SkellMiniNative SpacerVst3Parity
& ./build-mini/Release/SpacerVst3Parity.exe --mini 'build-mini/VST3/Release/Skell Mini Native DEV.vst3'
```

Alternatively use `python -m skellforge build-native mini SDK BUILD` from
the corresponding Forge branch. The Windows build runs the Steinberg validator.

## Verification

- Windows x64 Release and Steinberg validator passed, 47 of 47.
- The VST3 binary smoke rendered a 430 Hz tone to 440 Hz at full wet.
  Dry output remained 430 Hz and matched the input delayed by the reported
  144 samples at 48 kHz. The smoke exercises the loaded binary and confirms
  the generated host control count.
- SkellLang tested 20 of 20. The existing Forge suite ran 84 tests with four
  skips, and the Forge CLI built and validated the actual Mini bundle.
- Real vocals, legacy Mini state, full Mini controls, a branded editor,
  double-precision host buffers, and FL Studio compatibility remain open.
