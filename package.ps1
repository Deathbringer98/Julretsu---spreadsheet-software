param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$project = $PSScriptRoot
if (-not $SkipBuild) { & (Join-Path $project 'build.ps1') -Gui }
$buildDir = Join-Path $project 'build-native'
$binary = Join-Path $buildDir 'julretsu.exe'
if (-not (Test-Path -LiteralPath $binary)) { $binary = Join-Path $buildDir 'Release\julretsu.exe' }
if (-not (Test-Path -LiteralPath $binary)) { throw 'Build the Release GUI first.' }
$version = ([regex]::Match((Get-Content -Raw (Join-Path $project 'CMakeLists.txt')),'project\(Julretsu VERSION ([0-9.]+)')).Groups[1].Value
$destination = Join-Path $project "release\Julretsu-$version"
$notices = Join-Path $destination 'licenses'
New-Item -ItemType Directory -Force -Path $notices | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $destination 'Julretsu.exe') -Force
Copy-Item -LiteralPath (Join-Path $project 'assets') -Destination $destination -Recurse -Force
$runtime = Join-Path $project '.tools\llvm-mingw-20260908-ucrt-x86_64'
if (Test-Path -LiteralPath $runtime) {
    foreach ($name in @('libc++.dll','libunwind.dll')) {
        Copy-Item -LiteralPath (Join-Path $runtime "bin\$name") -Destination $destination -Force
    }
    Copy-Item -LiteralPath (Join-Path $runtime 'LICENSE.TXT') -Destination (Join-Path $notices 'LLVM.txt') -Force
    Copy-Item -Path (Join-Path $runtime 'x86_64-w64-mingw32\share\mingw32\COPYING*') -Destination $notices -Force
}
function DependencySource([string]$name,[string]$pin) {
    $local = Join-Path $project ".deps\$pin"
    if (Test-Path -LiteralPath $local) { return $local }
    $fetched = Join-Path $buildDir "_deps\$name-src"
    if (Test-Path -LiteralPath $fetched) { return $fetched }
    throw "Dependency license source unavailable: $name"
}
Copy-Item -LiteralPath (Join-Path (DependencySource 'imgui' 'imgui-f5befd2d29e66809cd1110a152e375a7f1981f06') 'LICENSE.txt') -Destination (Join-Path $notices 'ImGui.txt') -Force
Copy-Item -LiteralPath (Join-Path (DependencySource 'glfw' 'glfw-7b6aead9fb88b3623e3b3725ebb42670cbe4c579') 'LICENSE.md') -Destination (Join-Path $notices 'GLFW.txt') -Force
Copy-Item -LiteralPath (Join-Path (DependencySource 'sol2' 'sol2-9190880c593dfb018ccf5cc9729ab87739709862') 'LICENSE.txt') -Destination (Join-Path $notices 'sol2.txt') -Force
Copy-Item -LiteralPath (Join-Path (DependencySource 'stb' 'stb-2c980bb59875b0d32144a71867fbdebb2f77cd20') 'LICENSE') -Destination (Join-Path $notices 'stb.txt') -Force
Copy-Item -LiteralPath (Join-Path (DependencySource 'miniz' 'miniz-3.1.2') 'LICENSE') -Destination (Join-Path $notices 'miniz.txt') -Force
Copy-Item -LiteralPath (Join-Path (DependencySource 'pugixml' 'pugixml-1.15') 'LICENSE.md') -Destination (Join-Path $notices 'pugixml.txt') -Force
# lua.h contains Lua's exact copyright and full MIT notice.
Copy-Item -LiteralPath (Join-Path (DependencySource 'lua' 'lua-5.4.9') 'src\lua.h') -Destination (Join-Path $notices 'Lua-copyright-and-license.h') -Force
Copy-Item -LiteralPath (Join-Path $project 'LICENSE') -Destination $destination -Force
Copy-Item -LiteralPath (Join-Path $project 'THIRD_PARTY_NOTICES.md') -Destination $destination -Force
Copy-Item -LiteralPath (Join-Path $project 'docs\QUICKSTART.md') -Destination (Join-Path $destination 'START-HERE.md') -Force
$manual = Join-Path $project 'docs\Julretsu-User-Manual.pdf'
if (Test-Path -LiteralPath $manual) { Copy-Item -LiteralPath $manual -Destination $destination -Force }
Write-Output "Portable Windows app: $destination\Julretsu.exe"

Copy-Item -LiteralPath (Join-Path $project 'tools\Register-Julretsu.ps1') -Destination $destination -Force

# Update only an existing per-user Julretsu registration.
if (Test-Path -LiteralPath 'Registry::HKEY_CURRENT_USER\Software\Classes\Julretsu.Workbook') {
    & (Join-Path $destination 'Register-Julretsu.ps1')
}

# One-file Windows installer: the setup program followed by the zipped app and a small footer.
$stub = Join-Path $buildDir 'julretsu_setup.exe'
if (-not (Test-Path -LiteralPath $stub)) { throw 'Build the Julretsu setup program first (build.ps1 -Gui).' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$payload = Join-Path $project "release\Julretsu-$version-payload.zip"
if (Test-Path -LiteralPath $payload) { Remove-Item -LiteralPath $payload -Force }
[System.IO.Compression.ZipFile]::CreateFromDirectory($destination, $payload, [System.IO.Compression.CompressionLevel]::Optimal, $false)
$installer = Join-Path $project "release\Julretsu-$version-Windows-Setup.exe"
$output = [System.IO.File]::Create($installer)
try {
    foreach ($part in @($stub, $payload)) { $bytes = [System.IO.File]::ReadAllBytes($part); $output.Write($bytes, 0, $bytes.Length) }
    $size = [BitConverter]::GetBytes([UInt64](Get-Item -LiteralPath $payload).Length); $output.Write($size, 0, 8)
    $magic = [System.Text.Encoding]::ASCII.GetBytes('JULSETUP'); $output.Write($magic, 0, 8)
} finally { $output.Dispose() }
Remove-Item -LiteralPath $payload -Force
Write-Output "Windows installer: $installer"
