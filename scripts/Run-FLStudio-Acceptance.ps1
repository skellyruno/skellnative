# 🩻 SkellNative local FL Studio acceptance preflight.
# Isolated copies only; never touches production VST3 installs.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string[]]$Bundle,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [string]$FLStudioExe = "",
    [switch]$LaunchFLStudio
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if ($LaunchFLStudio -and (-not $FLStudioExe -or -not (Test-Path -LiteralPath $FLStudioExe -PathType Leaf))) {
    throw "LaunchFLStudio requires an existing FLStudioExe path."
}
$root = [System.IO.Path]::GetFullPath($OutputRoot)
if (-not (Test-Path -LiteralPath $root)) { [void](New-Item -ItemType Directory -Path $root -Force) }
$runId = [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssZ") + "-" + [Guid]::NewGuid().ToString("N").Substring(0,8)
$run = Join-Path $root $runId
[void](New-Item -ItemType Directory -Path $run)
$staging = Join-Path $run "VST3"
[void](New-Item -ItemType Directory -Path $staging)
$records = @()
foreach ($source in $Bundle) {
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "Missing VST3 bundle: $source" }
    $canonical = (Resolve-Path -LiteralPath $source).Path
    if (-not $canonical.EndsWith(".vst3", [StringComparison]::OrdinalIgnoreCase)) {
        throw "Bundle must end in .vst3: $canonical"
    }
    if ($run.StartsWith($canonical + [IO.Path]::DirectorySeparatorChar,
                        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Output cannot be nested inside a source bundle."
    }
    $entries = @(Get-Item -LiteralPath $canonical -Force) +
        @(Get-ChildItem -LiteralPath $canonical -Recurse -Force)
    foreach ($entry in $entries) {
        if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Symlink/junction not allowed in VST3 bundle: $($entry.FullName)"
        }
    }
    $binary = @(Get-ChildItem -LiteralPath $canonical -Recurse -File |
        Where-Object { $_.Name.EndsWith(".vst3", [StringComparison]::OrdinalIgnoreCase) })
    if ($binary.Count -ne 1) { throw "Expected one Windows binary per bundle: $canonical" }
    $bytes = [IO.File]::ReadAllBytes($binary[0].FullName)
    if ($bytes.Length -lt 256 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
        throw "Missing Windows MZ header: $($binary[0].FullName)"
    }
    $offset = [BitConverter]::ToInt32($bytes, 0x3c)
    $peSignature = "PE" + [char]0 + [char]0
    if ($offset -lt 64 -or $offset + 26 -gt $bytes.Length -or
        [Text.Encoding]::ASCII.GetString($bytes, $offset, 4) -ne $peSignature -or
        [BitConverter]::ToUInt16($bytes, $offset + 4) -ne 0x8664 -or
        [BitConverter]::ToUInt16($bytes, $offset + 24) -ne 0x20b) {
        throw "Expected valid AMD64 PE32+ VST3: $($binary[0].FullName)"
    }
    $destination = Join-Path $staging (Split-Path $canonical -Leaf)
    if (Test-Path -LiteralPath $destination) { throw "Duplicate staged name: $destination" }
    Copy-Item -LiteralPath $canonical -Destination $destination -Recurse
    $hash = (Get-FileHash -LiteralPath $binary[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $records += [pscustomobject]@{
        name = (Split-Path $canonical -Leaf)
        source = $canonical
        stage = $destination
        binary_sha256 = $hash
        status = "UNVERIFIED_IN_FL_STUDIO"
    }
}
$report = [ordered]@{
    schema = "skellnative/flstudio-acceptance/1"
    created_utc = [DateTime]::UtcNow.ToString("o")
    windows_version = [Environment]::OSVersion.VersionString
    studio_executable = $FLStudioExe
    checks = [ordered]@{
        scan_and_load = "NOT_RUN"
        audio_ab = "NOT_RUN"
        automation = "NOT_RUN"
        save_reopen = "NOT_RUN"
        editor_and_scaling = "NOT_RUN"
        multiple_instances_and_unload = "NOT_RUN"
        latency_and_cpu = "NOT_RUN"
    }
    bundles = @($records)
}
$reportPath = Join-Path $run "acceptance.json"
$report | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $reportPath -Encoding UTF8
$checklist = @'
# SkellNative FL Studio acceptance 🩻

Status: NOT RUN / needs actual interactive Windows DAW testing.
Add this run's VST3 directory as an extra scan path in FL Studio.
Do not replace production installations or edit existing projects.

- [ ] Rescan and load each exact staged VST3; record FL Studio version and scan errors.
- [ ] Play real audio; record Gain unity/mute, Mini dry/wet pitch, Spacer Live/Studio A/B.
- [ ] Automate each writable parameter, including rapid moves and multiple changes per block.
- [ ] Save a new FL Studio project, close, reopen, compare audio, IDs and plugin state.
- [ ] Open and close each editor 20x; test two instances and deletion while stopped.
- [ ] Inspect real screenshots at 80%, 100%, 125%, 150%, 200% scale if available.
- [ ] Measure CPU and PDC at 44.1, 48 and 96 kHz, blocks 64/256/1024.
- [ ] Check for glitches, NaNs, denormals, clipping and audible artifacts.
- [ ] Include exact source commit, screenshots, FL Studio logs and audio fixtures.

Write observed results into acceptance.json; never mark untested gates passed.
'@
$checklist | Set-Content -LiteralPath (Join-Path $run "FL_STUDIO_CHECKLIST.md") -Encoding UTF8
Write-Host "PASS: isolated PE32+ x64 bundle preflight; actual FL Studio checks remain NOT RUN."
Write-Host "Staged: $staging"
Write-Host "Report: $reportPath"
if ($LaunchFLStudio) {
    $process = Start-Process -FilePath $FLStudioExe -PassThru
    Write-Host "FL Studio started (PID $($process.Id)); host tests remain unverified."
}
