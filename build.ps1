param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [switch]$Benchmarks, [switch]$Example, [switch]$Gui, [switch]$Run, [switch]$Smoke
)
$ErrorActionPreference = 'Stop'
$project = $PSScriptRoot
$originalPath = $env:PATH
if ($Run -or $Smoke) { $Gui = $true }
try {
    $compiler = Join-Path $project '.tools\llvm-mingw-20260908-ucrt-x86_64\bin'
    $cmakeBin = Join-Path $project '.tools\cmake-4.4.3-windows-x86_64\bin'
    $extra = @()
    if (Test-Path -LiteralPath $cmakeBin) { $env:PATH = "$cmakeBin;" + $env:PATH }
    if (Test-Path -LiteralPath $compiler) {
        $env:PATH = "$compiler;$project\.tools;" + $env:PATH
        $extra = @('-G','Ninja','-DCMAKE_C_COMPILER=clang','-DCMAKE_CXX_COMPILER=clang++')
    }
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'Install CMake 3.20+ and a C++20 compiler.' }
    $pins = @{
        STB = 'stb-2c980bb59875b0d32144a71867fbdebb2f77cd20'
        IMGUI = 'imgui-f5befd2d29e66809cd1110a152e375a7f1981f06'
        GLFW = 'glfw-7b6aead9fb88b3623e3b3725ebb42670cbe4c579'
        SOL2 = 'sol2-9190880c593dfb018ccf5cc9729ab87739709862'
        LUA = 'lua-5.4.9'
    }
    foreach ($name in $pins.Keys) {
        $source = Join-Path $project ('.deps\' + $pins[$name])
        if (Test-Path -LiteralPath $source) { $extra += "-DFETCHCONTENT_SOURCE_DIR_${name}=$source" }
    }
    $directory = if ($Gui) { if ($Configuration -eq 'Release') { 'build-native' } else { "build-native-$Configuration" } } else { "build-$Configuration" }
    $buildDir = Join-Path $project $directory
    $benchmarkFlag = if ($Benchmarks) { 'ON' } else { 'OFF' }
    $guiFlag = if ($Gui) { 'ON' } else { 'OFF' }
    & cmake -S $project -B $buildDir @extra "-DCMAKE_BUILD_TYPE=$Configuration" "-DJULRETSU_BUILD_BENCHMARKS=$benchmarkFlag" "-DJULRETSU_BUILD_GUI=$guiFlag" "-DJULRETSU_ENABLE_LUA=$guiFlag"
    if ($LASTEXITCODE -ne 0) { throw 'Configure failed.' }
    & cmake --build $buildDir --config $Configuration --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    & ctest --test-dir $buildDir -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
    $binDir = if (Test-Path -LiteralPath (Join-Path $buildDir "$Configuration\julretsu_headless.exe")) { Join-Path $buildDir $Configuration } else { $buildDir }
    if ($Example) {
        & (Join-Path $binDir 'julretsu_headless.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Example failed.' }
    }
    if ($Benchmarks) {
        & (Join-Path $binDir 'julretsu_benchmarks.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Benchmarks failed.' }
    }
    if ($Smoke) {
        $artifacts = Join-Path $project 'artifacts'
        New-Item -ItemType Directory -Force -Path $artifacts | Out-Null
        $process = Start-Process -FilePath (Join-Path $binDir 'julretsu.exe') -ArgumentList @('--smoke','--capture-prefix',('"' + (Join-Path $artifacts 'julretsu') + '"')) -WorkingDirectory $project -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $artifacts 'native-smoke.txt') -RedirectStandardError (Join-Path $artifacts 'native-smoke-errors.txt')
        Get-Content -LiteralPath (Join-Path $artifacts 'native-smoke.txt')
        if ($process.ExitCode -ne 0) { throw 'Native smoke checks failed; see artifacts/native-smoke-errors.txt.' }
    }
    if ($Run) { Start-Process -FilePath (Join-Path $binDir 'julretsu.exe') -WorkingDirectory $project -WindowStyle Normal | Out-Null }
} finally { $env:PATH = $originalPath }
