# Windows native VST3 QA handoff

November 16, 2026. Keep the follow-up on development branches and leave both PRs in draft while release gates are open.

- [SkellNative PR #6](https://github.com/skellyruno/skellnative/pull/6), tested code `bc56f8338811cb05195c5df697317bb9bd7d3c4f`. Mini's Win32 editor now receives pointer input. The native capture host renders directly into its own bitmap, which fixed desktop occlusion in the 200% Spacer screenshot. [Windows CI](https://github.com/skellyruno/skellnative/actions/runs/43809729898)
- [SkellForge PR #27](https://github.com/skellyruno/skellforge/pull/27), tested code `22c8018b592c19a09f1bc6f0bcfa0c6957576972`. Generated Gain accepts the complete pinned local Steinberg SDK. Pilot builds reject outputs inside or above the source checkout. [Windows CI](https://github.com/skellyruno/skellforge/actions/runs/43809729094)

## Evidence

The local workspace is `C:\Users\J\Documents\ChatGPT\skellnative 2\validation`. Read `TEST-REPORT.md` for commands, DLL hashes, logs, WAVs, and the disposable FLP. `output\SKELLY-Native-Windows-x64-QA.zip` contains the exact DLLs tested in FL Studio. Its SHA-256 is `be88b106d2397210638bcfc233c16f5611e5712986377c3dd14f00c1777fef5d`.

The handoff-named v2 archive was unavailable. Gain and Spacer were verified against the SHA-256 values from [Native PR #5 CI build 43790033302](https://github.com/skellyruno/skellnative/actions/runs/43790033302). The package uses the locally rebuilt Mini from the tested PR #6 commit above. Each packaged DLL passed 47 Steinberg validator checks.

FL Studio 2025 25.2.5.5319 loaded all three exact DLLs and processed audio through two instances of each at 44.1 kHz with 256- and 1024-sample buffers. Basic automation and save/reopen passed. All five Mini editor controls responded after the fix. The standalone host passed 144 DSP configurations and 20 paired lifecycle cycles per DLL. Spacer matched the pinned dual-engine JUCE reference sample-for-sample. The report links all 15 native captures and the measured A/B renders.

## Resume here

1. Open the disposable QA FLP. The test bundles were staged in `C:\Program Files\Common Files\VST3\Skelly Native QA 43fb80c`. FL Studio listed the staged Spacer as `Skell Spacer Native DEV_2` because a separate copy was already installed. Verify loaded DLL paths and hashes before collecting new evidence.
2. Complete 20 delete/reinstantiate cycles per plugin in FL Studio, rapid automation across writable parameters, FL Studio sidechain routing, and the remaining host rate/buffer combinations. Record exact failures and retest fixes against the same fixture.
3. Finish representative-music and subjective level-matched listening, audio-thread allocation/lock instrumentation, and the remaining DPI/focus checks. Verify the named v2 ZIP if it becomes available.
