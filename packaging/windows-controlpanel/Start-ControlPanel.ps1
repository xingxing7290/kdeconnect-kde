param(
    [string]$DataDirectory = '',
    [string]$Screenshot = '',
    [switch]$Offscreen,
    [switch]$SelfTest
)
$ErrorActionPreference = 'Stop'
$packageRoot = $PSScriptRoot
$packageBin = Join-Path $packageRoot 'bin'
if (!$DataDirectory) { $DataDirectory = Join-Path $packageRoot 'profile' }
$profileRoot = [IO.Path]::GetFullPath($DataDirectory)
$profileDrive = [IO.Path]::GetPathRoot($profileRoot)
if ($profileDrive -notmatch '^[A-Za-z]:\\$' -or $profileRoot.TrimEnd('\') -eq $profileDrive.TrimEnd('\')) {
    throw 'Use a dedicated profile directory on a local drive, not a drive root'
}
$profileMarker = Join-Path $profileRoot '.kde-controlpanel-profile'
if (!(Test-Path -LiteralPath $profileMarker)) {
    if ((Test-Path -LiteralPath $profileRoot) -and @(Get-ChildItem -LiteralPath $profileRoot -Force).Count) {
        throw 'Use a new or empty data directory; unrelated directories are not modified'
    }
    New-Item -ItemType Directory -Force -Path $profileRoot | Out-Null
    $profileAcl = New-Object Security.AccessControl.DirectorySecurity
    $profileAcl.SetAccessRuleProtection($true, $false)
    $currentSid = [Security.Principal.WindowsIdentity]::GetCurrent().User
    $profileAcl.SetOwner($currentSid)
    foreach ($sid in @($currentSid, (New-Object Security.Principal.SecurityIdentifier('S-1-5-18')),
        (New-Object Security.Principal.SecurityIdentifier('S-1-5-32-544')))) {
        $rule = New-Object Security.AccessControl.FileSystemAccessRule($sid, 'FullControl',
            'ContainerInherit,ObjectInherit', 'None', 'Allow')
        $profileAcl.AddAccessRule($rule)
    }
    Set-Acl -LiteralPath $profileRoot -AclObject $profileAcl
    [IO.File]::WriteAllText($profileMarker, 'KDE Control Panel private profile v1')
}
foreach ($directory in @('temp', 'logs', 'kdeconnect', 'panel', 'config', 'cache/qml', 'cache/shaders')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $profileRoot $directory) | Out-Null
}
$env:PATH = "$packageBin;$env:SystemRoot\System32;$env:SystemRoot"
$env:TEMP = Join-Path $profileRoot 'temp'
$env:TMP = $env:TEMP
# D-Bus 1.14's Windows cookie keyring uses these two variables, not APPDATA.
$env:HOMEDRIVE = $profileDrive.TrimEnd('\')
$env:HOMEPATH = $profileRoot.Substring($env:HOMEDRIVE.Length)
$env:KDECONNECT_CONFIG_DIR = Join-Path $profileRoot 'kdeconnect'
$env:KDECONNECT_PANEL_CONFIG_DIR = Join-Path $profileRoot 'panel'
$env:XDG_CONFIG_HOME = Join-Path $profileRoot 'config'
$env:XDG_CACHE_HOME = Join-Path $profileRoot 'cache'
$env:QML_DISK_CACHE_PATH = Join-Path $profileRoot 'cache/qml'
$env:QT_SHADER_CACHE_PATH = Join-Path $profileRoot 'cache/shaders'
$env:QT_PLUGIN_PATH = Join-Path $packageRoot 'plugins'
$env:QML_IMPORT_PATH = Join-Path $packageRoot 'qml'
$env:QML2_IMPORT_PATH = $env:QML_IMPORT_PATH
$env:QT_QUICK_CONTROLS_STYLE = 'Basic'
$env:QT_QPA_PLATFORM = if ($Offscreen) { 'offscreen' } else { 'windows' }
$env:QT_QPA_FONTDIR = Join-Path $env:SystemRoot 'Fonts'
Remove-Item Env:DBUS_SESSION_BUS_ADDRESS -ErrorAction SilentlyContinue
Remove-Item Env:QT_LOGGING_RULES -ErrorAction SilentlyContinue
$pathHash = [Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($profileRoot.ToLowerInvariant()))
$mutexName = 'Local\KDEControlPanel-' + ([BitConverter]::ToString($pathHash) -replace '-', '')
$profileMutex = New-Object Threading.Mutex($false, $mutexName)
$mutexHeld = $false
$ownedProcesses = New-Object Collections.Generic.List[Diagnostics.Process]
$startedAt = Get-Date
function Stop-PackageChildren {
    $allProcesses = @(Get-CimInstance Win32_Process)
    $ownedIds = New-Object 'Collections.Generic.HashSet[int]'
    foreach ($owned in $ownedProcesses) { [void]$ownedIds.Add($owned.Id) }
    do {
        $added = $false
        foreach ($candidate in $allProcesses) {
            if ($ownedIds.Contains([int]$candidate.ParentProcessId) -and
                $candidate.ExecutablePath -and
                $candidate.ExecutablePath.StartsWith($packageBin + '\', [StringComparison]::OrdinalIgnoreCase) -and
                $candidate.CreationDate -ge $startedAt) {
                if ($ownedIds.Add([int]$candidate.ProcessId)) { $added = $true }
            }
        }
    } while ($added)
    foreach ($ownedId in $ownedIds) {
        $candidateProcess = Get-Process -Id $ownedId -ErrorAction SilentlyContinue
        if ($candidateProcess -and $candidateProcess.StartTime -ge $startedAt -and
            $candidateProcess.Path.StartsWith($packageBin + '\', [StringComparison]::OrdinalIgnoreCase)) {
            Stop-Process -Id $ownedId -Force -ErrorAction SilentlyContinue
        }
    }
}
function Start-PackageProcess([string]$Name, [string[]]$Arguments = @()) {
    $parameters = @{
        FilePath = Join-Path $packageBin "$Name.exe"
        WorkingDirectory = $packageBin
        PassThru = $true
        WindowStyle = 'Hidden'
        RedirectStandardOutput = Join-Path $profileRoot "logs/$Name-output.log"
        RedirectStandardError = Join-Path $profileRoot "logs/$Name-error.log"
    }
    if ($Arguments.Count) { $parameters.ArgumentList = $Arguments }
    $process = Start-Process @parameters
    # Keep the native handle alive so Windows PowerShell retains exit codes.
    [void]$process.Handle
    $ownedProcesses.Add($process)
    return $process
}
try {
    try { $mutexHeld = $profileMutex.WaitOne(0) }
    catch [Threading.AbandonedMutexException] { $mutexHeld = $true }
    if (!$mutexHeld) { throw "This profile is already running: $profileRoot" }
    $bus = Start-PackageProcess 'dbus-daemon' @('--nofork', '--print-address', '--config-file',
        ('"' + (Join-Path $packageBin 'data/dbus-1/session.conf') + '"'))
    $addressFile = Join-Path $profileRoot 'logs/dbus-daemon-output.log'
    [string]$address = ''
    $busDeadline = (Get-Date).AddSeconds(15)
    while ((Get-Date) -lt $busDeadline -and !$bus.HasExited) {
        $address = [string](Get-Content -LiteralPath $addressFile -ErrorAction SilentlyContinue | Select-Object -First 1)
        if ($address -and $address.StartsWith('nonce-tcp:')) { break }
        Start-Sleep -Milliseconds 100
    }
    if (!$address -or !$address.StartsWith('nonce-tcp:')) { throw "Private D-Bus did not start; inspect $profileRoot/logs" }
    $env:DBUS_SESSION_BUS_ADDRESS = $address.Trim()
    $daemon = Start-PackageProcess 'kdeconnectd'
    $ready = $false
    $daemonDeadline = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $daemonDeadline -and !$daemon.HasExited) {
        $probe = Start-PackageProcess 'dbus-send' @('--session', '--print-reply', '--reply-timeout=1000',
            '--dest=org.freedesktop.DBus', '/', 'org.freedesktop.DBus.NameHasOwner', 'string:org.kde.kdeconnect')
        if (!$probe.WaitForExit(2000)) { $probe.Kill(); throw 'D-Bus probe timed out' }
        if ((Get-Content (Join-Path $profileRoot 'logs/dbus-send-output.log') -Raw) -match 'boolean true') {
            $ready = $true
            break
        }
        Start-Sleep -Milliseconds 200
    }
    if (!$ready) { throw "KDE Connect daemon did not become ready; inspect $profileRoot/logs" }
    if ($SelfTest) {
        $allowedSids = @([Security.Principal.WindowsIdentity]::GetCurrent().User.Value, 'S-1-5-18', 'S-1-5-32-544')
        foreach ($secretName in @('privateKey.pem', 'certificate.pem')) {
            $secretPath = Join-Path $profileRoot "kdeconnect/$secretName"
            if (!(Test-Path -LiteralPath $secretPath)) { throw "Missing identity file: $secretName" }
            foreach ($access in (Get-Acl -LiteralPath $secretPath).Access) {
                $sidValue = $access.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value
                if ($access.AccessControlType -eq 'Allow' -and $sidValue -notin $allowedSids) {
                    throw "Unexpected access to identity file: $sidValue"
                }
            }
        }
        $cli = Start-PackageProcess 'kdeconnect-cli' @('--list-devices')
        if (!$cli.WaitForExit(10000)) { throw 'CLI self-test timed out' }
        if ($cli.ExitCode -ne 0) { throw "CLI self-test failed: $($cli.ExitCode)" }
    }
    $appArguments = @()
    if ($Screenshot) { $appArguments = @('--screenshot', ('"' + [IO.Path]::GetFullPath($Screenshot) + '"')) }
    $app = Start-PackageProcess 'kdeconnect-app' $appArguments
    if ($SelfTest) {
        if ($app.WaitForExit(1000)) { throw 'Application exited before module inspection' }
        $loadedModules = @()
        foreach ($target in @($daemon, $app)) {
            $target.Refresh()
            foreach ($module in $target.Modules) {
                $loadedModules += [ordered]@{process=$target.ProcessName; module=$module.ModuleName; path=$module.FileName}
                $isSystemModule = $module.FileName.StartsWith($env:SystemRoot + '\', [StringComparison]::OrdinalIgnoreCase)
                $isPackageModule = $module.FileName.StartsWith($packageRoot + '\', [StringComparison]::OrdinalIgnoreCase)
                if (!$isSystemModule -and !$isPackageModule) {
                    throw "Module loaded from outside package/system: $($module.FileName)"
                }
            }
        }
        $loadedModules | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $profileRoot 'logs/loaded-modules.json') -Encoding UTF8
    }
    if ($Screenshot) {
        if (!$app.WaitForExit(45000)) { throw 'Application screenshot timed out' }
        if ($app.ExitCode -ne 0 -or !(Test-Path -LiteralPath $Screenshot)) { throw 'Application screenshot failed' }
    } else {
        $app.WaitForExit()
    }
    if ($daemon.HasExited) { throw 'KDE Connect daemon exited unexpectedly' }
    Write-Output "Session completed. Data and logs: $profileRoot"
} finally {
    Stop-PackageChildren
    if ($mutexHeld) { $profileMutex.ReleaseMutex() }
    $profileMutex.Dispose()
}
