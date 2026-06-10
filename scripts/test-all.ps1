# Phase 0 local test runner. Regenerates the shared test vectors, then builds and
# runs the C++ and Kotlin protocol tests. Each stage degrades gracefully: a
# missing toolchain is reported as SKIPPED (with the reason) rather than aborting
# the whole run. Exit code is non-zero if any stage FAILED.
#
# Windows PowerShell 5.1 compatible (no '&&'/'||'; uses 'if ($?)' patterns).
# Run from anywhere: paths are resolved relative to the repo root.

$ErrorActionPreference = 'Continue'
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$results = [ordered]@{}

function Find-Cmake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $default = Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'
    if (Test-Path $default) { return $default }
    return $null
}

function Find-Python {
    foreach ($name in @('python', 'py')) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    return $null
}

# --- Stage 1: regenerate / verify test vectors --------------------------------
Write-Host "`n=== Stage 1: test vectors ===" -ForegroundColor Cyan
$python = Find-Python
if (-not $python) {
    Write-Warning "python not found; using committed vectors as-is."
    $results['vectors'] = 'SKIPPED (no python)'
} else {
    & $python proto/tools/gen_test_vectors.py
    if ($?) { $results['vectors'] = 'PASS' } else { $results['vectors'] = 'FAIL' }
}

# --- Stage 2: C++ proto tests -------------------------------------------------
Write-Host "`n=== Stage 2: C++ proto tests ===" -ForegroundColor Cyan
$cmake = Find-Cmake
if (-not $cmake) {
    Write-Warning "cmake not found in PATH or default install dir."
    Write-Host  "  Open a fresh PowerShell (so a freshly-installed CMake is on PATH) and re-run, or" -ForegroundColor Yellow
    Write-Host  "  use the 'Developer PowerShell' for your installed Visual Studio." -ForegroundColor Yellow
    $results['cpp'] = 'FAILED (cmake not found)'
} else {
    & $cmake -B pc/build -S pc
    if ($?) {
        & $cmake --build pc/build --config Debug
        if ($?) {
            # Run each test exe; all must exist and pass (proto = L1, client = L2/L3).
            $exes = @(
                'pc/build/client/proto/tests/Debug/proto_tests.exe',
                'pc/build/client/tests/Debug/client_tests.exe'
            )
            $cppOk = $true
            foreach ($exe in $exes) {
                if (Test-Path $exe) {
                    & $exe
                    if (-not $?) { $cppOk = $false }
                } else {
                    Write-Warning "test exe not found at $exe"
                    $cppOk = $false
                }
            }
            if ($cppOk) { $results['cpp'] = 'PASS' } else { $results['cpp'] = 'FAIL (tests)' }
        } else { $results['cpp'] = 'FAIL (build)' }
    } else { $results['cpp'] = 'FAIL (configure)' }
}

# --- Stage 3: Kotlin proto tests (Gradle wrapper) -----------------------------
Write-Host "`n=== Stage 3: Kotlin proto tests ===" -ForegroundColor Cyan
$wrapper = Join-Path $repoRoot 'android\gradlew.bat'
$wrapperJar = Join-Path $repoRoot 'android\gradle\wrapper\gradle-wrapper.jar'
if (-not (Test-Path $wrapperJar)) {
    Write-Warning "gradle-wrapper.jar missing; cannot run Android tests."
    Write-Host  "  Generate it once with a local Gradle/Android Studio: 'gradle wrapper --gradle-version 8.10.2'" -ForegroundColor Yellow
    $results['kotlin'] = 'SKIPPED (no wrapper jar)'
} else {
    Push-Location (Join-Path $repoRoot 'android')
    try {
        & .\gradlew.bat :proto:test :app:assembleDebug --console=plain --no-daemon
        if ($?) { $results['kotlin'] = 'PASS' } else { $results['kotlin'] = 'FAIL' }
    } finally { Pop-Location }
}

# --- Summary ------------------------------------------------------------------
Write-Host "`n=== Summary ===" -ForegroundColor Cyan
$failed = $false
foreach ($stage in $results.Keys) {
    $status = $results[$stage]
    $color = 'Green'
    if ($status -like 'FAIL*') { $color = 'Red'; $failed = $true }
    elseif ($status -like 'SKIPPED*') { $color = 'Yellow' }
    Write-Host ("  {0,-8} {1}" -f $stage, $status) -ForegroundColor $color
}

if ($failed) {
    Write-Host "`nOne or more stages FAILED." -ForegroundColor Red
    exit 1
}
Write-Host "`nAll runnable stages passed." -ForegroundColor Green
exit 0
