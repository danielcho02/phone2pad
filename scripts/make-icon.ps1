# Regenerate the phone2pad Windows icon from the source logo (logo.png).
#
# Pipeline: remove the white background (border flood-fill, so the INTERIOR white
# cursor is preserved), add a subtle light edge so the black phone stays visible on
# dark taskbars, crop to a centered square with transparent padding, then write:
#   pc/client/assets/phone2pad.png   transparent base (also the Android foreground src)
#   pc/client/assets/phone2pad.ico   16/32/48/256, alpha preserved (PNG-in-ICO)
#
# Windows PowerShell 5.1 compatible. Run from anywhere. Re-run when logo.png changes;
# then run scripts/make-android-icons.ps1 to refresh the Android launcher icons.
[CmdletBinding()]
param(
    [int]$Work = 512,        # working resolution (>= 256 icon; keeps flood-fill fast)
    [int]$WhiteThreshold = 240,
    [int]$HaloRadius = 3     # light-edge thickness at the working resolution
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$srcLogo = Join-Path $repoRoot 'logo.png'
$assets = Join-Path $repoRoot 'pc\client\assets'
$outPng = Join-Path $assets 'phone2pad.png'
$outIco = Join-Path $assets 'phone2pad.ico'

if (-not (Test-Path $srcLogo)) { throw "source logo not found: $srcLogo" }
New-Item -ItemType Directory -Force -Path $assets | Out-Null

Add-Type -AssemblyName System.Drawing

function Save-Png([System.Drawing.Bitmap]$bmp) {
    $ms = New-Object System.IO.MemoryStream
    try { $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png); return $ms.ToArray() }
    finally { $ms.Dispose() }
}

# --- Load + downscale source to the working size (32bpp ARGB) -------------------
$src = [System.Drawing.Image]::FromFile($srcLogo)
$workBmp = New-Object System.Drawing.Bitmap($Work, $Work,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    $g = [System.Drawing.Graphics]::FromImage($workBmp)
    try {
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.DrawImage($src, 0, 0, $Work, $Work)
    } finally { $g.Dispose() }
} finally { $src.Dispose() }

$W = $Work; $H = $Work
$rect = New-Object System.Drawing.Rectangle(0, 0, $W, $H)

# --- Read pixels into a byte buffer (BGRA) -------------------------------------
$data = $workBmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadWrite,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $data.Stride
$buf = New-Object byte[] ($stride * $H)
[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $buf, 0, $buf.Length)

function Idx([int]$x, [int]$y) { return ($y * $stride + $x * 4) }
function IsNearWhite([int]$i) {
    return ($buf[$i] -ge $WhiteThreshold) -and ($buf[$i + 1] -ge $WhiteThreshold) -and
           ($buf[$i + 2] -ge $WhiteThreshold)  # B,G,R
}

# --- Border flood-fill: near-white reachable from the edge -> transparent -------
$visited = New-Object bool[] ($W * $H)
$q = New-Object System.Collections.Generic.Queue[int]
function Enqueue-IfBg([int]$x, [int]$y) {
    if ($x -lt 0 -or $y -lt 0 -or $x -ge $W -or $y -ge $H) { return }
    $p = $y * $W + $x
    if ($visited[$p]) { return }
    if (IsNearWhite (Idx $x $y)) { $visited[$p] = $true; $q.Enqueue($p) }
}
for ($x = 0; $x -lt $W; $x++) { Enqueue-IfBg $x 0; Enqueue-IfBg $x ($H - 1) }
for ($y = 0; $y -lt $H; $y++) { Enqueue-IfBg 0 $y; Enqueue-IfBg ($W - 1) $y }
while ($q.Count -gt 0) {
    $p = $q.Dequeue()
    $x = $p % $W; $y = [int][math]::Floor($p / $W)
    $i = Idx $x $y
    $buf[$i] = 0; $buf[$i + 1] = 0; $buf[$i + 2] = 0; $buf[$i + 3] = 0  # transparent black
    Enqueue-IfBg ($x - 1) $y; Enqueue-IfBg ($x + 1) $y
    Enqueue-IfBg $x ($y - 1); Enqueue-IfBg $x ($y + 1)
}

# Opaque bounding box (the phone shape) for cropping later.
$minX = $W; $minY = $H; $maxX = -1; $maxY = -1
for ($y = 0; $y -lt $H; $y++) {
    for ($x = 0; $x -lt $W; $x++) {
        if ($buf[(Idx $x $y) + 3] -gt 0) {
            if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
            if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
        }
    }
}

# Build a silhouette buffer (opaque -> light gray) for the halo.
$sil = New-Object byte[] ($stride * $H)
for ($y = 0; $y -lt $H; $y++) {
    for ($x = 0; $x -lt $W; $x++) {
        $i = Idx $x $y
        if ($buf[$i + 3] -gt 0) { $sil[$i] = 238; $sil[$i + 1] = 238; $sil[$i + 2] = 238; $sil[$i + 3] = 255 }
    }
}

[System.Runtime.InteropServices.Marshal]::Copy($buf, 0, $data.Scan0, $buf.Length)
$workBmp.UnlockBits($data)

# Silhouette bitmap from $sil.
$silBmp = New-Object System.Drawing.Bitmap($W, $H, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$sd = $silBmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
[System.Runtime.InteropServices.Marshal]::Copy($sil, 0, $sd.Scan0, $sil.Length)
$silBmp.UnlockBits($sd)

# --- Compose: light-gray halo (8-direction offset stamp) under the phone --------
$composed = New-Object System.Drawing.Bitmap($W, $H, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$cg = [System.Drawing.Graphics]::FromImage($composed)
try {
    $cg.Clear([System.Drawing.Color]::Transparent)
    $r = $HaloRadius
    $offsets = @(@($r, 0), @(-$r, 0), @(0, $r), @(0, -$r), @($r, $r), @($r, -$r), @(-$r, $r), @(-$r, -$r))
    foreach ($o in $offsets) { $cg.DrawImage($silBmp, [int]$o[0], [int]$o[1]) }
    $cg.DrawImage($workBmp, 0, 0)  # real phone (black body + white cursor) on top
} finally { $cg.Dispose() }
$silBmp.Dispose(); $workBmp.Dispose()

# --- Crop to a centered square with transparent padding ------------------------
$bw = $maxX - $minX + 1; $bh = $maxY - $minY + 1
$side = [Math]::Max($bw, $bh)
$pad = [int][Math]::Round($side * 0.12) + $HaloRadius
$canvas = $side + 2 * $pad
$base = New-Object System.Drawing.Bitmap($canvas, $canvas, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$bg = [System.Drawing.Graphics]::FromImage($base)
try {
    $bg.Clear([System.Drawing.Color]::Transparent)
    $dx = $pad + [int][Math]::Round(($side - $bw) / 2)
    $dy = $pad + [int][Math]::Round(($side - $bh) / 2)
    $dest = New-Object System.Drawing.Rectangle($dx, $dy, $bw, $bh)
    $srcRect = New-Object System.Drawing.Rectangle($minX, $minY, $bw, $bh)
    $bg.DrawImage($composed, $dest, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
} finally { $bg.Dispose() }
$composed.Dispose()

$base.Save($outPng, [System.Drawing.Imaging.ImageFormat]::Png)

# --- Build the multi-size .ico (alpha preserved) -------------------------------
$sizes = @(16, 32, 48, 256)
$pngImages = New-Object System.Collections.Generic.List[byte[]]
foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($s, $s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.DrawImage($base, 0, 0, $s, $s)
    } finally { $g.Dispose() }
    $pngImages.Add((Save-Png $bmp))
    $bmp.Dispose()
}
$base.Dispose()

$out = New-Object System.IO.MemoryStream
$bw2 = New-Object System.IO.BinaryWriter($out)
try {
    $n = $sizes.Count
    $bw2.Write([UInt16]0); $bw2.Write([UInt16]1); $bw2.Write([UInt16]$n)
    $offset = 6 + (16 * $n)
    for ($i = 0; $i -lt $n; $i++) {
        $s = $sizes[$i]; $bytes = $pngImages[$i]
        $dim = if ($s -ge 256) { 0 } else { $s }
        $bw2.Write([Byte]$dim); $bw2.Write([Byte]$dim); $bw2.Write([Byte]0); $bw2.Write([Byte]0)
        $bw2.Write([UInt16]1); $bw2.Write([UInt16]32)
        $bw2.Write([UInt32]$bytes.Length); $bw2.Write([UInt32]$offset)
        $offset += $bytes.Length
    }
    foreach ($bytes in $pngImages) { $bw2.Write($bytes) }
    $bw2.Flush()
    [System.IO.File]::WriteAllBytes($outIco, $out.ToArray())
} finally { $bw2.Dispose(); $out.Dispose() }

Write-Host "wrote $outPng (transparent, $canvas x $canvas)"
Write-Host "wrote $outIco ($($sizes -join '/') px, $((Get-Item $outIco).Length) bytes)"
