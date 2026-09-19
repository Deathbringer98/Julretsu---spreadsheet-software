param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$project = $PSScriptRoot
if (-not $SkipBuild) { & (Join-Path $project 'build.ps1') -Gui }
$buildDir = Join-Path $project 'build-native'
$binary = Join-Path $buildDir 'julretsu.exe'
if (-not (Test-Path -LiteralPath $binary)) { $binary = Join-Path $buildDir 'Release\julretsu.exe' }
if (-not (Test-Path -LiteralPath $binary)) { throw 'Build the Release GUI first.' }
$destination = Join-Path $project 'release\Julretsu-0.3.0'
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
# lua.h contains Lua's exact copyright and full MIT notice.
Copy-Item -LiteralPath (Join-Path (DependencySource 'lua' 'lua-5.4.9') 'src\lua.h') -Destination (Join-Path $notices 'Lua-copyright-and-license.h') -Force
Copy-Item -LiteralPath (Join-Path $project 'LICENSE') -Destination $destination -Force
Copy-Item -LiteralPath (Join-Path $project 'THIRD_PARTY_NOTICES.md') -Destination $destination -Force
Copy-Item -LiteralPath (Join-Path $project 'docs\QUICKSTART.md') -Destination (Join-Path $destination 'START-HERE.md') -Force
Write-Output "Portable Windows app: $destination\Julretsu.exe"
