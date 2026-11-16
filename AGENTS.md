# SkellNative development rules 🩻

Follow the [SkellWorld master AGENTS.md](https://github.com/skellyruno/SkellWorld/blob/main/AGENTS.md) for build/test, DSP safety, GitHub, compatibility, and reporting standards.

Repository-specific rules:
- **No JUCE dependency** in this native foundation. Do not claim VST3 support until a compiled Steinberg SDK adapter and real Windows DAW tests exist.
- Build the host/platform boundary here; reusable DSP, UI, themes and presets belong in SkellCore.
- Keep the processing path free of allocation, locking, logging, exceptions and file I/O.
- New public APIs should preserve ABI/parameter compatibility once shipped; initial APIs are experimental.
- Keep current Skelly JUCE plugins unchanged until a separately verified opt-in migration exists.
- Use branches/draft PRs, run builds and tests, and never merge unverified changes.
