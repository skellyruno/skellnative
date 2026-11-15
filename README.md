# SkellNative 🩻

Skelly's **independent, experimental JUCE-free C++17 audio and Windows VST3 foundation**.

## What exists
- `include/skellnative/audio.hpp`, `src/audio.cpp`: small checked planar-buffer API with allocation-free copy/clear; original v0.1 API preserved.
- `native/include/SkellNative/`: tested portable DSP and parameter/state contract.
- `native/skelldraw/`: reusable UI semantics, original Win32 painter.
- `native/vst3/`: **SkellNative Gain**, an actual direct-Steinberg VST3 processor/controller with a native SkellDraw HWND editor and experimental stable prototype parameter IDs; **no JUCE and no VSTGUI**.
- `tools/`: standalone PE32+ and official Steinberg validator checks (no SkellForge dependency).

Source was transferred without renaming IDs or audio algorithms from SkellForge's
[direct VST3 prototype](https://github.com/skellyruno/skellforge/pull/9)
(commit `54c938a3a85e99ff325aa1152e706f66d967f976`).

SkellNative links against the official [Steinberg VST3 SDK](https://github.com/steinbergmedia/vst3sdk)
(pinned MIT-licensed SDK commit); it does **not** replace or own the VST standard.

## Role split
- SkellNative: host/platform integration, VST3 processor/controller, Win32 and real-time audio contracts.
- SkellCore: shared DSP, plugin UI, themes, presets and plugin-to-plugin integration.
- SkellForge: plugin design/generation, builds, release verification and packaging.

See [transfer/build instructions](docs/VST3_MIGRATION.md),
[architecture](docs/ARCHITECTURE.md), and
[SkellWorld/AGENTS.md](https://github.com/skellyruno/SkellWorld/blob/main/AGENTS.md).

**Experimental.** Build and Steinberg validator CI are necessary but not
sufficient for a release: real Windows FL Studio load/unload, state recall,
automation, sound A/B, multiple-instance UI and CPU/latency tests remain release
gates. Existing production plugins and their presets have not been modified.
