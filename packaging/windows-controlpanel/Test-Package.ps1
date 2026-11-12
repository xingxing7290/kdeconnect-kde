param(
    [Parameter(Mandatory = $true)][string]$PackageDirectory,
    [Parameter(Mandatory = $true)][string]$EvidenceDirectory
)
$ErrorActionPreference = 'Stop'
$packageRoot = (Resolve-Path -LiteralPath $PackageDirectory).Path
$evidenceRoot = [IO.Path]::GetFullPath($EvidenceDirectory)
if (Test-Path -LiteralPath $evidenceRoot) { throw 'Use a new evidence directory' }
New-Item -ItemType Directory -Path $evidenceRoot | Out-Null
$env:PSModulePath = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/Modules'
$shellPath = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
$profilePath = Join-Path $evidenceRoot 'profile with spaces'
$launcherPath = Join-Path $packageRoot 'Start-ControlPanel.ps1'
$mainProcess = $null
$windowProcess = $null
$windowHandle = [IntPtr]::Zero
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class PackageWindowTest {
    private delegate bool EnumCallback(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumCallback callback, IntPtr data);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr window, StringBuilder text, int length);
    [DllImport("user32.dll")] private static extern bool PostMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
    public static IntPtr Find(int processId) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, data) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner != processId) return true;
            var title = new StringBuilder(512);
            GetWindowText(window, title, title.Capacity);
            if (title.ToString().Contains("Control Panel")) { found = window; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static bool Close(IntPtr window) {
        return window != IntPtr.Zero && PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero);
    }
}
'@
$checks = @()
function Start-Session([string]$LogName, [string]$ExtraArguments = '') {
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $launcherPath +
        '" -DataDirectory "' + $profilePath + '" ' + $ExtraArguments
    $process = Start-Process -FilePath $shellPath -ArgumentList $arguments -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $evidenceRoot "$LogName-output.log") `
        -RedirectStandardError (Join-Path $evidenceRoot "$LogName-error.log")
    [void]$process.Handle
    return $process
}
try {
    $mainProcess = Start-Session 'first'
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline -and !$mainProcess.HasExited) {
        foreach ($candidate in @(Get-CimInstance Win32_Process -Filter "Name='kdeconnect-app.exe'")) {
            if ($candidate.ParentProcessId -eq $mainProcess.Id -and
                $candidate.ExecutablePath -eq (Join-Path $packageRoot 'bin/kdeconnect-app.exe')) {
                $windowProcess = Get-Process -Id $candidate.ProcessId
                [void]$windowProcess.Handle
                break
            }
        }
        if ($windowProcess) { $windowHandle = [PackageWindowTest]::Find($windowProcess.Id) }
        if ($windowHandle -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 200
    }
    if (!$windowProcess -or $windowHandle -eq [IntPtr]::Zero) { throw 'Native main window not found within 30 seconds' }
    $checks += 'native_window_started'
    $duplicate = Start-Session 'duplicate' '-SelfTest'
    if (!$duplicate.WaitForExit(10000) -or $duplicate.ExitCode -eq 0) { throw 'Duplicate profile was not rejected' }
    if ((Get-Content (Join-Path $evidenceRoot 'duplicate-error.log') -Raw) -notmatch 'already running') {
        throw 'Duplicate profile failed for an unexpected reason'
    }
    if ($mainProcess.HasExited -or $windowProcess.HasExited) { throw 'Duplicate launcher stopped the original session' }
    $checks += 'duplicate_profile_rejected_without_stopping_first'
    $keyPath = Join-Path $profilePath 'kdeconnect/privateKey.pem'
    $keyHash = (Get-FileHash -LiteralPath $keyPath -Algorithm SHA256).Hash
    if (![PackageWindowTest]::Close($windowHandle)) { throw 'Could not send normal close to the native window' }
    if (!$mainProcess.WaitForExit(15000) -or $mainProcess.ExitCode -ne 0) { throw 'Normal session close failed' }
    $checks += 'normal_close_and_cleanup'
    $screenshot = Join-Path $evidenceRoot 'restart.png'
    $restart = Start-Session 'restart' ('-SelfTest -Screenshot "' + $screenshot + '"')
    if (!$restart.WaitForExit(60000) -or $restart.ExitCode -ne 0) { throw 'Restart self-test failed or timed out' }
    if (!(Test-Path -LiteralPath $screenshot)) { throw 'Restart screenshot missing' }
    if ((Get-FileHash -LiteralPath $keyPath -Algorithm SHA256).Hash -ne $keyHash) { throw 'Identity changed on restart' }
    $checks += 'restart_preserves_identity_and_passes_selftest'
    $packageProcesses = @(Get-CimInstance Win32_Process | Where-Object {
        $_.ExecutablePath -and $_.ExecutablePath.StartsWith($packageRoot + '\', [StringComparison]::OrdinalIgnoreCase)
    })
    if ($packageProcesses.Count) { throw 'Package processes remain after normal exit' }
    $checks += 'no_remaining_package_processes'
    [ordered]@{passed=$checks; package=$packageRoot; evidence=$evidenceRoot} | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath (Join-Path $evidenceRoot 'result.json') -Encoding UTF8
    Write-Output "PASS: $($checks.Count) package lifecycle checks"
} finally {
    if ($windowProcess -and !$windowProcess.HasExited) { [void][PackageWindowTest]::Close($windowHandle) }
    if ($mainProcess -and !$mainProcess.HasExited) {
        if (!$mainProcess.WaitForExit(15000)) {
            if ($windowProcess -and !$windowProcess.HasExited -and
                $windowProcess.Path -eq (Join-Path $packageRoot 'bin/kdeconnect-app.exe')) {
                $windowProcess.Kill()
                [void]$mainProcess.WaitForExit(10000)
            }
            if (!$mainProcess.HasExited) { Write-Warning 'Launcher still running; preserve evidence and inspect its owned children' }
        }
    }
}
