# Format conversion only: keep the supplied source PNGs unchanged.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$assets = Join-Path (Split-Path $PSScriptRoot) 'assets'
$source = [System.Drawing.Bitmap]::new((Join-Path $assets 'app-icon.png'))
$streams = @()
try {
    $sizes = @(16,24,32,48,64,128,256)
    foreach ($size in $sizes) {
        $bitmap = [System.Drawing.Bitmap]::new($size,$size,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $stream = [System.IO.MemoryStream]::new()
        try {
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.DrawImage($source,0,0,$size,$size)
            $bitmap.Save($stream,[System.Drawing.Imaging.ImageFormat]::Png)
            $streams += ,$stream.ToArray()
        } finally { $graphics.Dispose(); $bitmap.Dispose(); $stream.Dispose() }
    }
    $output = [System.IO.File]::Create((Join-Path $assets 'app-icon.ico'))
    $writer = [System.IO.BinaryWriter]::new($output)
    try {
        $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
        $offset = 6 + 16 * $sizes.Count
        for ($i=0; $i -lt $sizes.Count; $i++) {
            $dimension = if ($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
            $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
            $writer.Write([byte]0); $writer.Write([byte]0)
            $writer.Write([uint16]1); $writer.Write([uint16]32)
            $writer.Write([uint32]$streams[$i].Length); $writer.Write([uint32]$offset)
            $offset += $streams[$i].Length
        }
        foreach ($bytes in $streams) { $writer.Write([byte[]]$bytes) }
    } finally { $writer.Dispose(); $output.Dispose() }
} finally { $source.Dispose() }
Write-Output 'Generated a Windows ICO with 16, 24, 32, 48, 64, 128, and 256 pixel images.'
