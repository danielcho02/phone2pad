# phone2pad release packager. Builds the Windows client (Release, static CRT) and,
# when a release keystore is configured, the signed Android APK/AAB, then stages a
# self-contained Windows zip and writes SHA256SUMS.txt under dist/.
#
# Mirrors scripts/test-all.ps1: Windows PowerShell 5.1 compatible (no '&&'/'||',
# uses 'if ($?)'); each stage degrades gracefully (missing toolchain / keystore is
# reported as SKIPPED rather than aborting). Run from anywhere — paths resolve to
# the repo root.
#
# Usage:
#   ./scripts/package-release.ps1                # version from this script's default
#   ./scripts/package-release.ps1 -Version 0.2.0
#   ./scripts/package-release.ps1 -SkipAndroid   # Windows zip only

[CmdletBinding()]
param(
    [string]$Version = '0.3.0',
    [switch]$SkipAndroid
)

$ErrorActionPreference = 'Continue'
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$results = [ordered]@{}
$producedAssets = New-Object System.Collections.Generic.List[string]

function Find-Cmake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $default = Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'
    if (Test-Path $default) { return $default }
    return $null
}

$distDir = Join-Path $repoRoot 'dist'
$buildDir = Join-Path $repoRoot 'pc\build-release'
$winName = "phone2pad-windows-x64-v$Version"
$stageDir = Join-Path $distDir $winName

# --- Stage 0: clean dist ------------------------------------------------------
Write-Host "`n=== phone2pad release packager — v$Version ===" -ForegroundColor Cyan
if (Test-Path $distDir) { Remove-Item -Recurse -Force $distDir }
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

# --- Stage 1: build PC client (Release, static CRT) ---------------------------
Write-Host "`n=== Stage 1: PC client (Release) ===" -ForegroundColor Cyan
$cmake = Find-Cmake
$winZipPath = $null
if (-not $cmake) {
    Write-Warning "cmake not found in PATH or default install dir; cannot build the Windows client."
    $results['pc'] = 'FAILED (cmake not found)'
} else {
    & $cmake -B $buildDir -S (Join-Path $repoRoot 'pc') -DPHONE2PAD_BUILD_TESTS=OFF
    if ($?) {
        & $cmake --build $buildDir --config Release
        if ($?) { $results['pc'] = 'PASS' } else { $results['pc'] = 'FAIL (build)' }
    } else { $results['pc'] = 'FAIL (configure)' }
}

# --- Stage 2: stage + zip the Windows client ----------------------------------
Write-Host "`n=== Stage 2: Windows zip ===" -ForegroundColor Cyan
if ($results['pc'] -eq 'PASS') {
    $exes = @{
        'phone2pad_tray.exe'   = Join-Path $buildDir 'client\Release\phone2pad_tray.exe'
        'phone2pad_client.exe' = Join-Path $buildDir 'client\Release\phone2pad_client.exe'
        'recorder.exe'         = Join-Path $buildDir 'tools\recorder\Release\recorder.exe'
        'replay.exe'           = Join-Path $buildDir 'tools\replay\Release\replay.exe'
    }
    New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
    $stageOk = $true
    foreach ($name in $exes.Keys) {
        $src = $exes[$name]
        if (Test-Path $src) {
            Copy-Item $src (Join-Path $stageDir $name) -Force
        } else {
            Write-Warning "missing built exe: $src"
            $stageOk = $false
        }
    }

    # README.txt (from template) + LICENSE (from repo root, if present).
    $readmeTemplate = Join-Path $repoRoot 'scripts\assets\windows-README.txt'
    if (Test-Path $readmeTemplate) {
        Copy-Item $readmeTemplate (Join-Path $stageDir 'README.txt') -Force
    } else {
        Write-Warning "windows-README.txt template missing."
    }
    $license = Join-Path $repoRoot 'LICENSE'
    if (Test-Path $license) {
        Copy-Item $license (Join-Path $stageDir 'LICENSE') -Force
    } else {
        Write-Warning "LICENSE not found at repo root; zip will omit it."
    }

    # Ship QUICKSTART.md next to the exes so the tray's "Open setup guide" resolves
    # locally (it falls back to the GitHub copy when absent).
    $quickstart = Join-Path $repoRoot 'QUICKSTART.md'
    if (Test-Path $quickstart) {
        Copy-Item $quickstart (Join-Path $stageDir 'QUICKSTART.md') -Force
    } else {
        Write-Warning "QUICKSTART.md not found at repo root; zip will omit it."
    }

    # Ship the adb setup guide flat in the zip root (source lives in docs/) so the
    # tray's "ADB 설치 안내 열기" opens it locally; it falls back to the GitHub copy.
    $adbSetup = Join-Path $repoRoot 'docs\ADB-SETUP.md'
    if (Test-Path $adbSetup) {
        Copy-Item $adbSetup (Join-Path $stageDir 'ADB-SETUP.md') -Force
    } else {
        Write-Warning "docs\ADB-SETUP.md not found; zip will omit the adb setup guide."
    }

    if ($stageOk) {
        $winZipPath = Join-Path $distDir "$winName.zip"
        if (Test-Path $winZipPath) { Remove-Item -Force $winZipPath }
        Compress-Archive -Path $stageDir -DestinationPath $winZipPath -Force
        $producedAssets.Add($winZipPath)
        $results['zip'] = 'PASS'
    } else {
        $results['zip'] = 'FAIL (missing exe)'
    }
} else {
    $results['zip'] = 'SKIPPED (no PC build)'
}

# --- Stage 3: Android release APK/AAB (only with a keystore) -------------------
Write-Host "`n=== Stage 3: Android release ===" -ForegroundColor Cyan
$keystoreProps = Join-Path $repoRoot 'android\keystore.properties'
if ($SkipAndroid) {
    $results['android'] = 'SKIPPED (-SkipAndroid)'
} elseif (-not (Test-Path $keystoreProps)) {
    Write-Warning "android/keystore.properties not found -> skipping Android release APK/AAB."
    Write-Host  "  Provide a keystore (see RELEASE.md) to publish a release-signed APK/AAB." -ForegroundColor Yellow
    Write-Host  "  Debug APKs are dev-only and are NOT attached as release assets." -ForegroundColor Yellow
    $results['android'] = 'SKIPPED (no keystore)'
} else {
    $wrapper = Join-Path $repoRoot 'android\gradlew.bat'
    if (-not (Test-Path $wrapper)) {
        Write-Warning "android/gradlew.bat missing; cannot build Android release."
        $results['android'] = 'FAILED (no gradlew)'
    } else {
        Push-Location (Join-Path $repoRoot 'android')
        try {
            & .\gradlew.bat :app:assembleRelease :app:bundleRelease --console=plain --no-daemon
            if ($?) {
                $apk = Get-ChildItem -Path 'app\build\outputs\apk\release' -Filter '*.apk' -ErrorAction SilentlyContinue |
                    Where-Object { $_.Name -notlike '*unsigned*' } | Select-Object -First 1
                $aab = Get-ChildItem -Path 'app\build\outputs\bundle\release' -Filter '*.aab' -ErrorAction SilentlyContinue |
                    Select-Object -First 1
                if ($apk) {
                    $apkDest = Join-Path $distDir "phone2pad-android-v$Version.apk"
                    Copy-Item $apk.FullName $apkDest -Force
                    $producedAssets.Add($apkDest)
                }
                if ($aab) {
                    $aabDest = Join-Path $distDir "phone2pad-android-v$Version.aab"
                    Copy-Item $aab.FullName $aabDest -Force
                    $producedAssets.Add($aabDest)
                }
                if ($apk) { $results['android'] = 'PASS' }
                else { $results['android'] = 'FAIL (no signed apk — check keystore)' }
            } else { $results['android'] = 'FAIL (gradle build)' }
        } finally { Pop-Location }
    }
}

# --- Stage 4: SHA256SUMS ------------------------------------------------------
Write-Host "`n=== Stage 4: checksums ===" -ForegroundColor Cyan
if ($producedAssets.Count -gt 0) {
    $sumsPath = Join-Path $distDir 'SHA256SUMS.txt'
    $lines = foreach ($asset in $producedAssets) {
        $hash = (Get-FileHash -Algorithm SHA256 -Path $asset).Hash.ToLower()
        "{0}  {1}" -f $hash, (Split-Path -Leaf $asset)
    }
    Set-Content -Path $sumsPath -Value $lines -Encoding ASCII
    $results['checksums'] = 'PASS'
} else {
    Write-Warning "no assets produced; skipping checksums."
    $results['checksums'] = 'SKIPPED (no assets)'
}

# --- Summary ------------------------------------------------------------------
Write-Host "`n=== Summary ===" -ForegroundColor Cyan
$failed = $false
foreach ($stage in $results.Keys) {
    $status = $results[$stage]
    $color = 'Green'
    if ($status -like 'FAIL*') { $color = 'Red'; $failed = $true }
    elseif ($status -like 'SKIPPED*') { $color = 'Yellow' }
    Write-Host ("  {0,-10} {1}" -f $stage, $status) -ForegroundColor $color
}

Write-Host "`nAssets in dist/:" -ForegroundColor Cyan
if ($producedAssets.Count -gt 0) {
    foreach ($asset in $producedAssets) { Write-Host "  $(Split-Path -Leaf $asset)" }
    Write-Host "  SHA256SUMS.txt"
} else {
    Write-Host "  (none)"
}

if ($failed) {
    Write-Host "`nOne or more stages FAILED." -ForegroundColor Red
    exit 1
}
Write-Host "`nPackaging complete." -ForegroundColor Green
exit 0
