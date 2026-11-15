# SkellNative architecture (experimental)

## Responsibilities
- **SkellNative**: low-level planar audio buffers, runtime contracts, future Windows native UI and Steinberg VST3 SDK boundary.
- **SkellCore**: shared DSP engines, plugin UI widgets, presets, themes and cross-plugin utilities.
- **SkellForge**: generators, validation, build orchestration, packaging and release gates.

## Current scope
`SkellNative::Native` is a JUCE-free C++17 static library with checked planar float buffer copy/clear primitives and no external dependencies. It does **not** create a VST3 binary, implement a plugin processor interface, provide a GUI, or run audio from a DAW. Compatibility with FL Studio and CPU/latency behavior is **not yet tested**.

## Porting milestones (not yet implemented)
1. Establish owned audio process/parameter/state contracts with real-time-safety tests.
2. Create a pinned, license-reviewed Steinberg VST3 SDK adapter (VST3 **plugin**, not VST3 **host**); add manufacturer/product ID and state-preservation tests.
3. Windows native editor and SkellCore UI bridge, screenshot/interaction testing.
4. Native SkellForge generation path and verified Windows VST3/FL Studio smoke tests.
5. Optional side-by-side port of a low-risk plugin with preset, automation-ID, state and audio A/B regression checks.

Never replace existing JUCE plugins automatically. Build one native plugin end-to-end and verify before migrating any production project.
