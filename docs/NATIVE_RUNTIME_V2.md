# Native runtime and SkellLang v2 verification — 2026-11-15

## Delivered

- Shared host-neutral `Controls.h` maps bounded finite values, skew and discrete steps.
- Shared VST3 control metadata, text conversion, state IO and sample-offset automation.
- Atomic validation of each automation batch before state/audio mutation. Queue storage
  is fixed-size; no allocation or sorting is introduced into this callback helper.
  Automation is piecewise constant at each event; this is not linear interpolation
  between VST3 points. Engines remain responsible for audible smoothing.
- Mini uses the shared runtime and SkellLang schema v2. Existing five parameter IDs,
  ranges, defaults, title strings, class IDs and native state product key are retained.
  Its bypass metadata is now declared in source rather than special-cased in C++.
- Invalid, wrong-product and unknown-key states fail closed. Appended controls
  default when reading older states. Existing SKST v1 encoding is unchanged.
- Mini rejects sample rates outside its actual engine range, 8–192 kHz, instead
  of silently clamping the host rate. Float64 buffers remain unsupported.
- Mini metadata uses strict JSON after checking the expected DEV product and IDs.
- Compiler pin is `93871860d9d2b5b70ecfa6a9f5ddcd444e463431`; an explicit local
  `SKELLNATIVE_SKELLLANG_SOURCE` override remains available for development.
- Windows CI builds Mini and runs its CTest gates in addition to the Gain checks.

## Local evidence

Windows x64 MSVC Release, official Steinberg SDK 3.8.1:

- PASS — shared runtime metadata, finite/bounded mapping, sample offsets, flush,
  invalid batches, read-only controls, state roundtrip, corruption, wrong identity,
  and defaults for appended parameters.
- PASS — official Mini validator, 47 tests, zero failed.
- PASS — loaded Mini VST3 automation gives 48,128 sample-exact samples versus
  separate host blocks at the same event boundaries.
- PASS — 430 Hz dry / 440 Hz corrected; dry signal matches 144-sample reported
  delay at 48 kHz.
- PASS — ten Python metadata tests.

The official SDK validator prints diagnostics for rejected unsupported sample
rates even when its sample-rate test passes. Mini validation runs through CTest
instead of MSBuild post-build console parsing. Full output is retained in
`Testing/Temporary/LastTest.log`; rejection is intentional, not a hidden failure.

## Scope and next steps

This is DEV infrastructure, not a production release. No Master or Preset plugin
port is included. SkellCore retains an optional, separately named Master DSP
snapshot at the user's request; it is not wired into a native plugin here.

1. Test Mini and Spacer automation, preset/project recall, multiple instances,
   editor reopen and unloading in FL Studio. These remain UNVERIFIED.
2. Give Mini a genuine SkellDraw editor and verify rendered states and gestures.
3. Finish Spacer visual/legacy-state parity before any replacement release.
   Spacer's compatibility-sensitive block automation policy is unchanged.
4. Broaden Mini audio checks to real vocals, supported rates and host layouts.
5. Add explicit retired-parameter migration policies and a general Forge native
   project exporter. Current Forge commands build named pilots.
6. Independently review SkellCipher security and user-device recovery before
   protecting irreplaceable source. Encrypted-native checks do not emit private
   source into CMake caches; automated encrypted VST3 builds are not implemented.
