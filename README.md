# SkellNative 🩻

Skelly's **experimental, JUCE-free native audio foundation**.

Development takes place in feature branches and draft pull requests. This initial commit only establishes the repository; no VST3 host compatibility, DSP performance, or distributable plugin is claimed yet.

- **SkellNative:** native audio/process contracts and eventual VST3/Windows adapters
- **SkellCore:** reusable DSP, UI, themes, presets, and cross-plugin utilities
- **SkellForge:** project generation, builds, verification, and packaging

Existing JUCE plugins will not be migrated automatically. The shared engineering rules live in [SkellWorld/AGENTS.md](https://github.com/skellyruno/SkellWorld/blob/main/AGENTS.md).
