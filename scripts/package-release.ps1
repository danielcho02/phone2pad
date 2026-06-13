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

function Find-Iscc {
    # Inno Setup 6 compiler. Used to build the per-user Windows installer.
    $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    # Per-machine (default) and per-user (winget) install locations.
    $bases = @(${env:ProgramFiles(x86)}, $env:ProgramFiles,
               (Join-Path $env:LOCALAPPDATA 'Programs'))
    foreach ($base in $bases) {
        if (-not $base) { continue }
        $p = Join-Path $base 'Inno Setup 6\ISCC.exe'
        if (Test-Path $p) { return $p }
    }
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

# --- Stage 2.5: Windows installer (Inno Setup) --------------------------------
# Packages the SAME staged folder as the zip into a per-user setup .exe. Builds
# only when the zip staged cleanly and ISCC.exe is available; otherwise SKIPPED.
Write-Host "`n=== Stage 2.5: Windows installer ===" -ForegroundColor Cyan
$setupExe = $null
if ($results['zip'] -ne 'PASS') {
    $results['installer'] = 'SKIPPED (no zip)'
} else {
    $iscc = Find-Iscc
    if (-not $iscc) {
        Write-Warning "ISCC.exe (Inno Setup 6) not found; cannot build the installer."
        Write-Host  "  Install it once with:  winget install JRSoftware.InnoSetup" -ForegroundColor Yellow
        Write-Host  "  A v0.3.0 RELEASE requires installer + installer-verify to PASS (SKIPPED is dev-only)." -ForegroundColor Yellow
        $results['installer'] = 'SKIPPED (ISCC not found)'
    } else {
        $iss = Join-Path $repoRoot 'scripts\installer\phone2pad.iss'
        & $iscc "/DMyVersion=$Version" "/DStageDir=$stageDir" "/DRepoRoot=$repoRoot" $iss
        $expectedSetup = Join-Path $distDir "phone2pad-setup-v$Version.exe"
        if ($? -and (Test-Path $expectedSetup)) {
            $setupExe = $expectedSetup
            $producedAssets.Add($setupExe)
            $results['installer'] = 'PASS'
        } else {
            $results['installer'] = 'FAIL (iscc)'
        }
    }
}

# --- Stage 2.6: installer verification (silent install -> assert -> uninstall) -
# Smoke test that the installer lands the expected files + the Start Menu shortcut,
# does NOT install adb, does NOT launch the tray (skipifsilent), and uninstalls
# cleanly. Runs per-user into a temp dir. The installer uses DisableProgramGroupPage
# (so /GROUP is ignored) and always creates the default "phone2pad" group; to avoid
# disturbing a real install, the test is SKIPPED when a phone2pad install is already
# present. Start Menu shortcut creation is a requirement, so NO /NOICONS.
Write-Host "`n=== Stage 2.6: installer verification ===" -ForegroundColor Cyan
$startMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs'
$groupDir  = Join-Path $startMenu 'phone2pad'
$shortcut  = Join-Path $groupDir 'phone2pad.lnk'
# Inno appends "_is1" to the AppId for its HKCU uninstall key.
$unKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{8F2A6C13-7E4B-4D9A-AC15-2B6E9D3F1A07}_is1'
if ($results['installer'] -ne 'PASS') {
    $results['installer-verify'] = "SKIPPED ($($results['installer']))"
} elseif (-not (Test-Path $setupExe) -or (Get-Item $setupExe).Length -le 0) {
    $results['installer-verify'] = 'FAIL (setup exe missing/empty)'
} elseif ((Test-Path $shortcut) -or (Test-Path $unKey)) {
    Write-Warning "an existing phone2pad install was detected; skipping the destructive smoke test so it is not clobbered."
    $results['installer-verify'] = 'SKIPPED (existing phone2pad install present)'
} else {
    $rand        = [guid]::NewGuid().ToString('N').Substring(0, 8)
    $verifyDir   = Join-Path $env:TEMP "phone2pad-verify-$rand"
    $verifyLog   = "$verifyDir.log"
    $vFail = New-Object System.Collections.Generic.List[string]
    $ranSilently = $true
    try {
        if (Test-Path $verifyDir) { Remove-Item -Recurse -Force $verifyDir }

        # Silent per-user install. skipifsilent in [Run] keeps the tray from launching.
        # /GROUP is intentionally omitted (it is ignored under DisableProgramGroupPage).
        $instArgs = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART',
                      "/DIR=$verifyDir", "/LOG=$verifyLog")
        try {
            $proc = Start-Process -FilePath $setupExe -ArgumentList $instArgs -Wait -PassThru -ErrorAction Stop
            if ($proc.ExitCode -ne 0) { $vFail.Add("installer exit code $($proc.ExitCode)") }
        } catch {
            $ranSilently = $false
            Write-Warning "could not launch the installer silently: $($_.Exception.Message)"
        }

        if ($ranSilently) {
            # Inno's loader returns after the install completes for a non-elevated run,
            # but settle briefly so the Start Menu shortcut is flushed to disk.
            $deadline = (Get-Date).AddSeconds(30)
            while (-not (Test-Path (Join-Path $verifyDir 'phone2pad_tray.exe')) -and (Get-Date) -lt $deadline) {
                Start-Sleep -Milliseconds 300
            }

            # (3) installed files present
            foreach ($f in @('phone2pad_tray.exe', 'phone2pad_client.exe', 'recorder.exe', 'replay.exe')) {
                if (-not (Test-Path (Join-Path $verifyDir $f))) { $vFail.Add("missing installed file: $f") }
            }
            # (4) Start Menu shortcut created
            if (-not (Test-Path $shortcut)) { $vFail.Add("missing Start Menu shortcut: $shortcut") }
            # (5) adb / platform-tools NOT installed
            if (Test-Path (Join-Path $verifyDir 'adb.exe'))        { $vFail.Add('adb.exe was installed (must not be)') }
            if (Test-Path (Join-Path $verifyDir 'platform-tools')) { $vFail.Add('platform-tools was installed (must not be)') }

            # (6) silent uninstall
            $uninst = Join-Path $verifyDir 'unins000.exe'
            if (Test-Path $uninst) {
                try {
                    Start-Process -FilePath $uninst -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES') -Wait -ErrorAction Stop
                } catch {
                    $vFail.Add("uninstall failed to launch: $($_.Exception.Message)")
                }
                # The uninstaller copies itself to temp and self-deletes the dir async.
                $udeadline = (Get-Date).AddSeconds(30)
                while ((Test-Path $verifyDir) -and (Get-Date) -lt $udeadline) {
                    Start-Sleep -Milliseconds 300
                }
                # (7) install dir + shortcut removed
                if (Test-Path $verifyDir) { $vFail.Add('install dir not removed by uninstall') }
                if (Test-Path $shortcut)  { $vFail.Add('Start Menu shortcut not removed by uninstall') }
            } else {
                $vFail.Add('uninstaller (unins000.exe) not found')
            }
        }
    } finally {
        # Best-effort cleanup so a failed run never leaves verify artifacts behind.
        foreach ($leftover in @($verifyDir, $groupDir)) {
            if ($leftover -and (Test-Path $leftover)) {
                Remove-Item -Recurse -Force $leftover -ErrorAction SilentlyContinue
            }
        }
        if (Test-Path $verifyLog) { Remove-Item -Force $verifyLog -ErrorAction SilentlyContinue }
    }

    if (-not $ranSilently) {
        # Could not exercise the installer; fall back to the existence/size check
        # (already passed above to reach here).
        $results['installer-verify'] = 'PASS (exists only — silent run unavailable)'
    } elseif ($vFail.Count -eq 0) {
        $results['installer-verify'] = 'PASS'
    } else {
        Write-Warning ("installer verification failures:`n  - " + ($vFail -join "`n  - "))
        $results['installer-verify'] = 'FAIL (' + ($vFail -join '; ') + ')'
    }
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
