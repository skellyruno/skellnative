# SkellNative Gain — direct-Steinberg VST3 transfer 🩻

## Provenance
Original SkellForge source: `skellyruno/skellforge`, commit
`54c938a3a85e99ff325aa1152e706f66d967f976`, branch
`feature/skellnative-direct-vst3-gain` (experimental PR #9).

This milestone transfers the original JUCE-free native implementation:
- portable C++17 DSP, state and parameter contracts;
- a real Steinberg processor/controller/factory and native SkellDraw Win32 HWND editor;
- an SDK pinned to official `vst3sdk` commit `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`;
- portable DSP tests, UI interaction smoke, 72-case actual VST3 adapter regression;
- Steinberg official validator and PE32+ x64 bundle checks.

Native CMake sources remain in `native/` (rather than shifting relative paths)
to keep the known-good prototype build layout unchanged. The original standalone
buffer API in `include/skellnative/` remains intact for backward compatibility.

Only the standalone verification utilities were made independent of SkellForge.
No SkellForge, SkellCore, shipped Skelly product IDs, presets or FL Studio files
are modified. This is NOT a production migration.

## Build and verify
On Linux / Windows:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake -S native -B native/build -DSKELLNATIVE_BUILD_TESTS=ON
cmake --build native/build --config Release --parallel
ctest --test-dir native/build -C Release --output-on-failure
```
On Windows x64 with CMake, MSVC, Python and Git:
```powershell
cmake -S native/vst3 -B native/vst3/build -A x64
cmake --build native/vst3/build --target SkellNativeGain --config Release --parallel 2
python -m tools.check_skellnative_vst3 native/vst3/build
cmake --build native/vst3/build --target SkellDrawUiSmoke SkellNativeVst3AdapterBench validator --config Release --parallel 2
python -m tools.validate_skellnative_vst3 native/vst3/build
```
First setup fetches the pinned official Steinberg SDK with submodules.
The SDK is **MIT licensed**, with attribution requirements. The VST standard is
not Skelly-owned, and this native plugin uses the official Steinberg API.

## Verification gates
- Linux portable tests: independently test current commit.
- Windows MSVC VST3, native Win32 editor, adapter, Steinberg validator: CI must
  pass on this repository's transferred commit (prior SkellForge success is
  provenance, not verification of the transfer).
- Real FL Studio host-load, saved state, parameter automation, unload/reload,
  UI scaling, CPU/latency and audio A/B: still requires hands-on DAW verification.

Never publish the experimental artifact as a stable release or merge the draft
PR before required checks.
