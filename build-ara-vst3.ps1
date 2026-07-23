param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root "build-vs"
$ara = Resolve-Path -LiteralPath (Join-Path $root "external\ARA_SDK")

$vsRoot = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
$vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
$cmake = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "Visual Studio vcvars64.bat not found: $vcvars"
}

if (-not (Test-Path -LiteralPath $cmake)) {
    throw "Visual Studio bundled CMake not found: $cmake"
}

$configure = @(
    "`"$vcvars`"",
    "&&",
    "`"$cmake`"",
    "-S", "`"$root`"",
    "-B", "`"$buildDir`"",
    "-G", "`"Visual Studio 17 2022`"",
    "-A", "x64",
    "-DQQDEBREATH_FETCH_JUCE=ON",
    "-DQQDEBREATH_COPY_AFTER_BUILD=ON",
    "-DQQDEBREATH_ARA_SDK_PATH=`"$ara`""
) -join " "

$build = @(
    "`"$vcvars`"",
    "&&",
    "`"$cmake`"",
    "--build", "`"$buildDir`"",
    "--config", $Configuration,
    "--target", "QQEasyTool_VST3"
) -join " "

Write-Host "Plugin root: $root"
Write-Host "ARA SDK: $ara"
Write-Host "Build dir: $buildDir"
Write-Host "Configuring QQEasyTool 0.99 ARA-capable VST3..."
cmd /c $configure
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Building QQEasyTool_VST3..."
cmd /c $build
exit $LASTEXITCODE
