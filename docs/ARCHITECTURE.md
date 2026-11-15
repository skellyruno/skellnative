# SkellNative architecture (experimental) 🩻

## Responsibilities
- **SkellNative**: independent native processing contracts, low-level buffer operations, Win32 rendering boundary, and direct Steinberg VST3 plugin adapter.
- **SkellCore**: reusable DSP, plugin UI widgets, presets, themes and cross-plugin utilities.
- **SkellForge**: project generation, validation, orchestration, Windows builds, packaging and release gates.

## Implemented today
- `SkellNative::Native` is a JUCE-free C++17 static library with checked planar buffer copy/clear operations and no external dependencies.
- `native/include/SkellNative/` contains the original portable SkellNative DSP, plugin-state and parameter prototypes transferred from SkellForge.
- `native/vst3/` contains a real *experimental* Windows x64 direct-Steinberg VST3 gain effect with separate processor/controller, genuine host automation, native SkellDraw Win32 editor and pinned official Steinberg SDK.
- The native VST3 output is built by Windows GitHub Actions and checked with Steinberg's official validator. No JUCE/VSTGUI is required for this native prototype.
- The Steinberg-generated `moduleinfo.json` is automatically validated and normalized to strict JSON to avoid invalid trailing commas.

The plugin is **not production validated**: FL Studio load/unload, automation/state recall, UI scaling, CPU/latency and sound verification remain separate mandatory gates.

## Next milestones
1. Real FL Studio acceptance tests on user Windows hardware.
2. Stable public native SDK contracts and backward-compatible bridge to SkellCore.
3. Native project generation backend in SkellForge, preserving existing JUCE templates.
4. Opt-in native port of an existing low-risk Skelly plugin after reference audio A/B and preset/state compatibility checks.

The original source and immutable import commit are documented in [VST3 transfer details](VST3_MIGRATION.md). Do not automatically migrate or rewrite shipped JUCE plugins.
