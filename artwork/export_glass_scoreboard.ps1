# Code-native rounded UI geometry, corresponding to glass_scoreboard.svg.
# No game installation is performed by this asset exporter.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$assetRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$bitmap = New-Object System.Drawing.Bitmap(1040,680,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$graphics.Clear([System.Drawing.Color]::Transparent)
function New-RoundRect([single]$x,[single]$y,[single]$w,[single]$h,[single]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r*2
    $p.AddArc($x,$y,$d,$d,180,90)
    $p.AddArc($x+$w-$d,$y,$d,$d,270,90)
    $p.AddArc($x+$w-$d,$y+$h-$d,$d,$d,0,90)
    $p.AddArc($x,$y+$h-$d,$d,$d,90,90)
    $p.CloseFigure()
    return ,$p
}
$path = New-RoundRect 2 2 1036 676 52
$rectangle = New-Object System.Drawing.RectangleF(0,0,1040,680)
$fill = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rectangle,[System.Drawing.Color]::Black,[System.Drawing.Color]::Black,90)
$blend = New-Object System.Drawing.Drawing2D.ColorBlend(4)
$blend.Positions = [single[]]@(0,.16,.8,1)
$blend.Colors = [System.Drawing.Color[]]@([System.Drawing.Color]::FromArgb(156,115,121,126),[System.Drawing.Color]::FromArgb(138,52,56,62),[System.Drawing.Color]::FromArgb(125,32,37,44),[System.Drawing.Color]::FromArgb(145,83,89,97))
$fill.InterpolationColors = $blend
$graphics.FillPath($fill,$path)
$edge = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rectangle,[System.Drawing.Color]::White,[System.Drawing.Color]::White,90)
$edgeBlend = New-Object System.Drawing.Drawing2D.ColorBlend(3)
$edgeBlend.Positions = [single[]]@(0,.36,1)
$edgeBlend.Colors = [System.Drawing.Color[]]@([System.Drawing.Color]::FromArgb(224,255,255,255),[System.Drawing.Color]::FromArgb(122,220,228,235),[System.Drawing.Color]::FromArgb(184,244,248,255))
$edge.InterpolationColors = $edgeBlend
$pen = New-Object System.Drawing.Pen($edge,2)
$graphics.DrawPath($pen,$path)
$inner = New-RoundRect 5 5 1030 670 49
$innerPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(33,255,255,255),1)
$graphics.DrawPath($innerPen,$inner)
$bitmap.Save((Join-Path $assetRoot 'glass_scoreboard.png'),[System.Drawing.Imaging.ImageFormat]::Png)
$innerPen.Dispose();$inner.Dispose();$pen.Dispose();$edge.Dispose();$fill.Dispose();$path.Dispose();$graphics.Dispose();$bitmap.Dispose()
