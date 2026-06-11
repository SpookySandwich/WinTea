# Generates WinTea's icons from vector shapes (no ImageMagick needed).
# Outputs multi-size .ico files into assets/icon plus a preview sheet.
#
#   pwsh tools/make-icons.ps1
#
# Re-run after tweaking the drawing below. The .ico files are committed so a
# clean build does not require running this.

$ErrorActionPreference = 'Stop'
try { Add-Type -AssemblyName System.Drawing } catch { Add-Type -AssemblyName System.Drawing.Common }

$root   = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root 'assets\icon'
New-Item -ItemType Directory -Force $outDir | Out-Null

function New-RoundedRect([single]$x, [single]$y, [single]$w, [single]$h, [single]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

# Draws the teacup (+ steam "T") in 0..100 space using the given solid color.
function Draw-Teacup($g, [System.Drawing.Color]$col, [bool]$steam) {
    $brush = New-Object System.Drawing.SolidBrush($col)
    $pen   = New-Object System.Drawing.Pen($col, 7)
    $pen.StartCap = 'Round'; $pen.EndCap = 'Round'

    # Handle (drawn first so the body overlaps its inner edge).
    $g.DrawArc($pen, 60, 40, 26, 26, -70, 150)
    # Cup body.
    $body = New-Object System.Drawing.Drawing2D.GraphicsPath
    $body.AddPolygon(([System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF(26,40)),
        (New-Object System.Drawing.PointF(72,40)),
        (New-Object System.Drawing.PointF(63,74)),
        (New-Object System.Drawing.PointF(35,74)))))
    $g.FillPath($brush, $body)
    # Rim + saucer.
    $g.FillEllipse($brush, 25, 35, 48, 11)
    $g.FillEllipse($brush, 19, 77, 60, 10)

    if ($steam) {
        # The "T" of Win+T(ea), rising as steam.
        $g.FillRectangle($brush, 46, 9, 8, 22)   # vertical
        $g.FillRectangle($brush, 36, 9, 28, 7)   # top bar
    }
}

function New-Glyph([int]$S, [string]$mode) {
    $bmp = New-Object System.Drawing.Bitmap($S, $S, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.PixelOffsetMode = 'HighQuality'
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.ScaleTransform($S / 100.0, $S / 100.0)
    $steam = $S -ge 24

    switch ($mode) {
        'app' {
            $bg = New-RoundedRect 4 4 92 92 22
            $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
                (New-Object System.Drawing.Point(0,0)), (New-Object System.Drawing.Point(0,100)),
                ([System.Drawing.Color]::FromArgb(255,76,175,125)),
                ([System.Drawing.Color]::FromArgb(255,38,125,84)))
            $g.FillPath($grad, $bg)
            Draw-Teacup $g ([System.Drawing.Color]::White) $steam
        }
        'dark'  { Draw-Teacup $g ([System.Drawing.Color]::FromArgb(255,235,235,235)) $steam } # for dark taskbar
        'light' { Draw-Teacup $g ([System.Drawing.Color]::FromArgb(255,40,40,40)) $steam }    # for light taskbar
    }
    $g.Dispose()
    return $bmp
}

# Classic uncompressed ICO entry (BITMAPINFOHEADER + BGRA + AND mask).
# Sizes below 256 MUST use this format: the shell (Start menu, Explorer,
# ExtractAssociatedIcon) does not decode PNG-compressed entries reliably except
# for the 256px slot -- all-PNG icons render as the generic placeholder.
function ConvertTo-IcoDib([System.Drawing.Bitmap]$bmp) {
    $S = $bmp.Width
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $maskRow = [int][Math]::Ceiling($S / 32.0) * 4   # 1bpp AND-mask row, 32-bit padded
    $bw.Write([uint32]40)                            # BITMAPINFOHEADER
    $bw.Write([int32]$S)
    $bw.Write([int32]($S * 2))                       # height counts XOR + AND blocks
    $bw.Write([uint16]1); $bw.Write([uint16]32)
    $bw.Write([uint32]0)                             # BI_RGB
    $bw.Write([uint32]($S * $S * 4 + $maskRow * $S))
    $bw.Write([int32]0); $bw.Write([int32]0); $bw.Write([uint32]0); $bw.Write([uint32]0)
    for ($y = $S - 1; $y -ge 0; $y--) {              # XOR: BGRA, bottom-up
        for ($x = 0; $x -lt $S; $x++) {
            $c = $bmp.GetPixel($x, $y)
            $bw.Write([byte]$c.B); $bw.Write([byte]$c.G); $bw.Write([byte]$c.R); $bw.Write([byte]$c.A)
        }
    }
    for ($y = $S - 1; $y -ge 0; $y--) {              # AND mask: 1 = transparent
        $row = New-Object byte[] $maskRow
        for ($x = 0; $x -lt $S; $x++) {
            if ($bmp.GetPixel($x, $y).A -eq 0) {
                $row[$x -shr 3] = $row[$x -shr 3] -bor (0x80 -shr ($x -band 7))
            }
        }
        $bw.Write($row)
    }
    $bw.Flush()
    return [byte[]]$ms.ToArray()   # MUST be byte[]: an Object[] binds Write() to the char[] overload
}

function Save-Ico([int[]]$sizes, [string]$mode, [string]$path) {
    $imgs = New-Object 'System.Collections.Generic.List[byte[]]'
    foreach ($s in $sizes) {
        $bmp = New-Glyph $s $mode
        if ($s -ge 256) {  # only the 256 slot may be PNG-compressed
            $msp = New-Object System.IO.MemoryStream
            $bmp.Save($msp, [System.Drawing.Imaging.ImageFormat]::Png)
            $imgs.Add([byte[]]$msp.ToArray())
        } else {
            $imgs.Add([byte[]](ConvertTo-IcoDib $bmp))
        }
        $bmp.Dispose()
    }
    $fs = [System.IO.File]::Create($path)
    $bw = New-Object System.IO.BinaryWriter($fs)
    $bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $b = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
        $bw.Write([byte]$b); $bw.Write([byte]$b); $bw.Write([byte]0); $bw.Write([byte]0)
        $bw.Write([uint16]1); $bw.Write([uint16]32)
        $bw.Write([uint32]$imgs[$i].Length); $bw.Write([uint32]$offset)
        $offset += $imgs[$i].Length
    }
    foreach ($img in $imgs) { $bw.Write([byte[]]$img) }
    $bw.Flush(); $fs.Close()
    Write-Host "wrote $path ($($sizes.Count) sizes)"
}

Save-Ico @(16,20,24,32,48,64,128,256) 'app'   (Join-Path $outDir 'wintea.ico')
Save-Ico @(16,20,24,32)               'light' (Join-Path $outDir 'tray-for-light-taskbar.ico')
Save-Ico @(16,20,24,32)               'dark'  (Join-Path $outDir 'tray-for-dark-taskbar.ico')

# Preview sheet on a split light/dark background for eyeballing.
$pw = 360; $ph = 140
$prev = New-Object System.Drawing.Bitmap($pw, $ph)
$pg = [System.Drawing.Graphics]::FromImage($prev)
$pg.SmoothingMode = 'AntiAlias'
$pg.FillRectangle((New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,240,240,240))), 0, 0, $pw/2, $ph)
$pg.FillRectangle((New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,32,32,32))), $pw/2, 0, $pw/2, $ph)
$app = New-Glyph 96 'app';   $pg.DrawImage($app, 20, 22, 96, 96)
$gl  = New-Glyph 32 'light'; $pg.DrawImage($gl, 140, 54, 32, 32)
$gd  = New-Glyph 32 'dark';  $pg.DrawImage($gd, 300, 54, 32, 32)
$pg.Dispose()
$previewPath = Join-Path $outDir 'preview.png'
$prev.Save($previewPath, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "wrote $previewPath"

# --- README logo (256 app icon on transparent) ----------------------------
$assets = Join-Path $root 'assets'
$logo = New-Glyph 256 'app'
$logoPath = Join-Path $assets 'wintea-logo.png'
$logo.Save($logoPath, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "wrote $logoPath"

# --- GitHub social preview (1280x640) --------------------------------------
$sw = 1280; $sh = 640
$soc = New-Object System.Drawing.Bitmap($sw, $sh)
$sg = [System.Drawing.Graphics]::FromImage($soc)
$sg.SmoothingMode = 'AntiAlias'
$sg.TextRenderingHint = 'ClearTypeGridFit'
$grad2 = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Point(0,0)), (New-Object System.Drawing.Point(0,$sh)),
    ([System.Drawing.Color]::FromArgb(255,16,32,24)),
    ([System.Drawing.Color]::FromArgb(255,10,20,15)))
$sg.FillRectangle($grad2, 0, 0, $sw, $sh)
$bigIcon = New-Glyph 320 'app'
$sg.DrawImage($bigIcon, 130, 160, 320, 320)

$white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
$green = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,120,220,170))
$grey  = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,180,195,188))
$titleFont = New-Object System.Drawing.Font('Segoe UI', 92, [System.Drawing.FontStyle]::Bold)
$tagFont   = New-Object System.Drawing.Font('Segoe UI', 34, [System.Drawing.FontStyle]::Regular)
$subFont   = New-Object System.Drawing.Font('Segoe UI', 26, [System.Drawing.FontStyle]::Regular)
$sg.DrawString('WinTea', $titleFont, $white, 520, 175)
$sg.DrawString("Win+T  ->  terminal", $tagFont, $green, 528, 320)
$sg.DrawString("Win+Alt+T  ->  admin", $tagFont, $green, 528, 372)
$sg.DrawString("one ~200 KB exe  -  zero dependencies", $subFont, $grey, 528, 440)
$sg.Dispose()
$socPath = Join-Path $assets 'social-preview.png'
$soc.Save($socPath, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "wrote $socPath"
