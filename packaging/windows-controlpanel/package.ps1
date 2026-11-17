param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$DependencyRoot,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [Parameter(Mandatory = $true)][string]$FFmpegRoot,
    [Parameter(Mandatory = $true)][string]$RuntimeRoot,
    [Parameter(Mandatory = $true)][string]$DistributionMaterialsRoot
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& (Join-Path $PSScriptRoot 'Assert-DistributionMaterials.ps1') -DistributionMaterialsRoot $DistributionMaterialsRoot
$packageRoot = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $packageRoot) {
    throw "Output must be a new directory, not an existing package: $packageRoot"
}
New-Item -ItemType Directory -Path $packageRoot | Out-Null
function Invoke-Checked([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable exited $LASTEXITCODE" }
}
function Copy-Tree([string]$Source, [string]$Destination) {
    if (!(Test-Path -LiteralPath $Source)) { throw "Missing package input: $Source" }
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | Copy-Item -Destination $Destination -Recurse -Force
}
Invoke-Checked 'cmake' @('--install', $BuildDirectory, '--prefix', $packageRoot)
$packageBin = Join-Path $packageRoot 'bin'
$packageQml = Join-Path $packageRoot 'qml'
$packagePlugins = Join-Path $packageRoot 'plugins'
Copy-Tree (Join-Path $packageRoot 'lib/qml') $packageQml
Copy-Tree (Join-Path $packageRoot 'lib/plugins') $packagePlugins
# Framework plugins are loaded dynamically, not through PE imports.
foreach ($pluginType in @('kf6', 'kpeople', 'kiconthemes6', 'iconengines', 'styles')) {
    $pluginSource = Join-Path $DependencyRoot "plugins/$pluginType"
    if (Test-Path -LiteralPath $pluginSource) {
        Copy-Tree $pluginSource (Join-Path $packagePlugins $pluginType)
    }
}
Copy-Tree (Join-Path $DependencyRoot 'qml/org/kde') (Join-Path $packageQml 'org/kde')
foreach ($dataType in @('kf6', 'mime')) {
    Copy-Tree (Join-Path $DependencyRoot "bin/data/$dataType") (Join-Path $packageBin "data/$dataType")
}
$frameworkChinese = Join-Path $DependencyRoot 'bin/data/locale/zh_CN'
if (Test-Path -LiteralPath $frameworkChinese) {
    Copy-Tree $frameworkChinese (Join-Path $packageBin 'data/locale/zh_CN')
}
foreach ($helper in @('dbus-daemon.exe', 'dbus-send.exe', 'kioworker.exe', 'SnoreToast.exe')) {
    $helperSource = Join-Path $DependencyRoot "bin/$helper"
    if (Test-Path -LiteralPath $helperSource) { Copy-Item -LiteralPath $helperSource -Destination $packageBin }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'qt.conf') -Destination $packageBin
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'session.conf') -Destination (Join-Path $packageBin 'data/dbus-1/session.conf')
$deployArguments = @('--release', '--compiler-runtime',
    '--qmldir', (Join-Path $sourceRoot 'app/qml'), '--qmldir', (Join-Path $sourceRoot 'indicator'),
    '--qmldir', (Join-Path $sourceRoot 'smsapp'),
    '--qmlimport', (Join-Path $BuildDirectory 'bin'), '--qmlimport', (Join-Path $DependencyRoot 'qml'),
    '--dir', $packageBin, '--plugindir', $packagePlugins, '--qml-deploy-dir', $packageQml,
    '--translationdir', (Join-Path $packageRoot 'translations'), '--translations', 'en,zh_CN',
    '--skip-plugin-types', 'qmltooling', '--include-plugins', 'qoffscreen', '--openssl-root', $DependencyRoot)
$deployArguments += @(Get-ChildItem -LiteralPath $packageBin -Filter 'kdeconnect*.exe' | ForEach-Object FullName)
Invoke-Checked (Join-Path $DependencyRoot 'bin/windeployqt.exe') $deployArguments
$ffmpegLibraries = @('avcodec-62.dll', 'avdevice-62.dll', 'avfilter-11.dll',
    'avformat-62.dll', 'avutil-60.dll', 'swresample-6.dll', 'swscale-9.dll')
foreach ($library in $ffmpegLibraries) {
    $replacement = Join-Path $FFmpegRoot "bin/$library"
    if (!(Test-Path -LiteralPath $replacement -PathType Leaf)) { throw "Missing FFmpeg override: $replacement" }
    Copy-Item -LiteralPath $replacement -Destination (Join-Path $packageBin $library) -Force
}
$runtimeLibraries = @('concrt140.dll', 'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll',
    'msvcp140_atomic_wait.dll', 'msvcp140_codecvt_ids.dll', 'vccorlib140.dll',
    'vcruntime140.dll', 'vcruntime140_1.dll', 'vcruntime140_threads.dll', 'vcomp140.dll')
foreach ($library in $runtimeLibraries) {
    $replacement = Join-Path $RuntimeRoot "bin/$library"
    if (!(Test-Path -LiteralPath $replacement -PathType Leaf)) { throw "Missing release CRT: $replacement" }
    Copy-Item -LiteralPath $replacement -Destination (Join-Path $packageBin $library) -Force
}
Invoke-Checked 'cmake' @("-DPACKAGE_ROOT=$packageRoot", "-DDEPENDENCY_ROOT=$DependencyRoot",
    "-DFFMPEG_OVERRIDE_ROOT=$FFmpegRoot",
    "-DRUNTIME_OVERRIDE_ROOT=$RuntimeRoot",
    '-P', (Join-Path $PSScriptRoot 'runtime-dependencies.cmake'))
Copy-Tree (Join-Path $sourceRoot 'LICENSES') (Join-Path $packageRoot 'licenses/kdeconnect')
Copy-Tree (Join-Path $DependencyRoot 'licenses') (Join-Path $packageRoot 'licenses/dependencies')
Copy-Tree (Join-Path $DistributionMaterialsRoot 'licenses/third-party') (Join-Path $packageRoot 'licenses/third-party')
Copy-Item -LiteralPath (Join-Path $DistributionMaterialsRoot 'source-index.json'),
    (Join-Path $DistributionMaterialsRoot 'THIRD-PARTY-NOTICES.txt') -Destination $packageRoot
foreach ($launcher in @('Start-ControlPanel.cmd', 'Start-ControlPanel.ps1')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $launcher) -Destination $packageRoot
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.zh-CN.md') -Destination $packageRoot
Invoke-Checked 'python' @((Join-Path $PSScriptRoot 'verify-ffmpeg.py'), $packageRoot)
Write-Output "Package staged: $packageRoot"
