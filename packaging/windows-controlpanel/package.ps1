param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$DependencyRoot,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
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
Invoke-Checked 'cmake' @("-DPACKAGE_ROOT=$packageRoot", "-DDEPENDENCY_ROOT=$DependencyRoot",
    '-P', (Join-Path $PSScriptRoot 'runtime-dependencies.cmake'))
Copy-Tree (Join-Path $sourceRoot 'LICENSES') (Join-Path $packageRoot 'licenses/kdeconnect')
Copy-Tree (Join-Path $DependencyRoot 'licenses') (Join-Path $packageRoot 'licenses/dependencies')
foreach ($launcher in @('Start-ControlPanel.cmd', 'Start-ControlPanel.ps1')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $launcher) -Destination $packageRoot
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.zh-CN.md') -Destination $packageRoot
Write-Output "Package staged: $packageRoot"
