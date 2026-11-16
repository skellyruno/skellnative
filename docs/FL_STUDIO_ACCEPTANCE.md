# 🩻 FL Studio acceptance gate — SkellNative

Windows CI compilation is not evidence of interactive FL Studio compatibility. Run the acceptance preflight locally against the exact experimental VST3 output.

PowerShell example (edit bundle paths to match extracted builds):

    powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Run-FLStudio-Acceptance.ps1 -Bundle "C:\Tests\SkellNativeGain.vst3","C:\Tests\Skell Mini Native DEV.vst3","C:\Tests\Skell Spacer Native DEV.vst3" -OutputRoot "$env:USERPROFILE\Documents\Skelly-Native-Test"

To open an installed FL Studio after staging, also pass -FLStudioExe "C:\Program Files\Image-Line\FL Studio 2026\FL64.exe" -LaunchFLStudio. Adjust the executable path to your own installation.

The runner verifies that each source is an existing VST3 folder with one genuine AMD64 PE32+ binary and no symlink/junction entries; it computes SHA-256, stages isolated copies, and writes acceptance.json and FL_STUDIO_CHECKLIST.md. All host gates start with NOT_RUN. The runner can launch the DAW but does not claim to automate its UI.

Add the newly staged VST3 directory as a FL Studio scan folder. Rescan, load each plugin, and complete every checklist gate against real audio and copied test projects. Keep production plugins untouched. Measure sample rates, block sizes, CPU/PDC and UI scaling, then record exact commit hashes, screenshots and failures.

Release requires genuine interactive FL Studio scan/load, state recall, parameter automation, multiple-instance/editor lifecycle, measured latency/CPU and audio quality evidence. GitHub CI validation alone is insufficient.
