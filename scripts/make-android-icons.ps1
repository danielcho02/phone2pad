# Generate phone2pad Android launcher icon PNGs from the transparent base
# (pc/client/assets/phone2pad.png — produced by scripts/make-icon.ps1).
#
# Writes, under android/app/src/main/res/:
#   mipmap-<d>/ic_launcher_foreground.png   adaptive foreground (phone in safe zone)
#   mipmap-<d>/ic_launcher.png              legacy: white rounded square + phone
#   mipmap-<d>/ic_launcher_round.png        legacy: white circle + phone
# The adaptive XML (mipmap-anydpi-v26/*) and the background color are static files
# committed alongside; this script only (re)generates the raster icons.
#
# Windows PowerShell 5.1 compatible. Run after make-icon.ps1.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$basePng = Join-Path $repoRoot 'pc\client\assets\phone2pad.png'
$res = Join-Path $repoRoot 'android\app\src\main\res'
if (-not (Test-Path $basePng)) { throw "base icon not found: $basePng (run make-icon.ps1 first)" }

Add-Type -AssemblyName System.Drawing

# Density -> (legacy launcher px, adaptive foreground px).
$densities = @(
    @{ name = 'mdpi';    legacy = 48;  fg = 108 },
    @{ name = 'hdpi';    legacy = 72;  fg = 162 },
    @{ name = 'xhdpi';   legacy = 96;  fg = 216 },
    @{ name = 'xxhdpi';  legacy = 144; fg = 324 },
    @{ name = 'xxxhdpi'; legacy = 192; fg = 432 }
)

$base = [System.Drawing.Image]::FromFile($basePng)

function New-Canvas([int]$size) {
    $b = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    return $b
}
function New-Graphics([System.Drawing.Bitmap]$b) {
    $g = [System.Drawing.Graphics]::FromImage($b)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    return $g
}
# Draw the base centered at the given fraction of the canvas.
function Draw-Centered([System.Drawing.Graphics]$g, [int]$size, [double]$frac) {
    $d = [int][Math]::Round($size * $frac)
    $o = [int][Math]::Round(($size - $d) / 2)
    $g.DrawImage($base, $o, $o, $d, $d)
}
function Rounded-Rect([int]$size, [double]$radiusFrac) {
    $r = [int][Math]::Round($size * $radiusFrac)
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = 2 * $r
    $p.AddArc(0, 0, $d, $d, 180, 90)
    $p.AddArc($size - $d, 0, $d, $d, 270, 90)
    $p.AddArc($size - $d, $size - $d, $d, $d, 0, 90)
    $p.AddArc(0, $size - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

$white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)

foreach ($den in $densities) {
    $dir = Join-Path $res ("mipmap-" + $den.name)
    New-Item -ItemType Directory -Force -Path $dir | Out-Null

    # Adaptive foreground: phone within the inner safe zone (transparent elsewhere).
    $fg = New-Canvas $den.fg
    $g = New-Graphics $fg
    try { $g.Clear([System.Drawing.Color]::Transparent); Draw-Centered $g $den.fg 0.62 } finally { $g.Dispose() }
    $fg.Save((Join-Path $dir 'ic_launcher_foreground.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    $fg.Dispose()

    # Legacy square: white rounded square + phone.
    $sq = New-Canvas $den.legacy
    $g = New-Graphics $sq
    try {
        $g.Clear([System.Drawing.Color]::Transparent)
        $path = Rounded-Rect $den.legacy 0.18
        try { $g.FillPath($white, $path) } finally { $path.Dispose() }
        Draw-Centered $g $den.legacy 0.74
    } finally { $g.Dispose() }
    $sq.Save((Join-Path $dir 'ic_launcher.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    $sq.Dispose()

    # Legacy round: white circle + phone.
    $rd = New-Canvas $den.legacy
    $g = New-Graphics $rd
    try {
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.FillEllipse($white, 0, 0, $den.legacy, $den.legacy)
        Draw-Centered $g $den.legacy 0.70
    } finally { $g.Dispose() }
    $rd.Save((Join-Path $dir 'ic_launcher_round.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    $rd.Dispose()

    Write-Host ("wrote mipmap-{0}: foreground {1}px, legacy {2}px" -f $den.name, $den.fg, $den.legacy)
}

$white.Dispose()
$base.Dispose()
Write-Host "done."
