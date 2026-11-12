# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
param([Parameter(Mandatory = $true)][string]$EvidenceDirectory)
$ErrorActionPreference = 'Stop'
$evidenceRoot = [IO.Path]::GetFullPath($EvidenceDirectory)
if ([IO.Path]::GetPathRoot($evidenceRoot) -notmatch '^[A-Za-z]:\\$' -or
    $evidenceRoot.TrimEnd('\') -eq [IO.Path]::GetPathRoot($evidenceRoot).TrimEnd('\')) { throw 'Use a dedicated local evidence directory' }
if (Test-Path -LiteralPath $evidenceRoot) { throw 'Use a new evidence directory' }
New-Item -ItemType Directory -Path $evidenceRoot | Out-Null
$tokens = $null
$parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'Start-ControlPanel.ps1'), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw 'Launcher parse error' }
$definitions = @($ast.FindAll({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Wait-PackageIdentity'
}, $false))
if ($definitions.Count -ne 1) { throw 'Expected one production readiness function' }
. ([scriptblock]::Create($definitions[0].Extent.Text))
$daemon = Get-Process -Id $PID
$first = Join-Path $evidenceRoot 'synthetic-first.pem'
$second = Join-Path $evidenceRoot 'synthetic-second.pem'
$checks = @()
$writer = $null
try {
    [IO.File]::WriteAllText($first, '')
    [IO.File]::WriteAllText($second, '')
    $writer = Start-Job -ScriptBlock {
        param($one, $two)
        Start-Sleep -Milliseconds 400
        [IO.File]::WriteAllText($one, 'Synthetic readiness fixture - not a key')
        Start-Sleep -Milliseconds 400
        [IO.File]::WriteAllText($two, 'Synthetic readiness fixture - not a certificate')
    } -ArgumentList $first, $second
    $timer = [Diagnostics.Stopwatch]::StartNew()
    Wait-PackageIdentity -Daemon $daemon -IdentityPaths @($first, $second) -TimeoutMilliseconds 10000
    if ($timer.ElapsedMilliseconds -lt 750) { throw 'Empty or incomplete files were accepted' }
    if (!(Wait-Job $writer -Timeout 10)) { throw 'Synthetic writer did not finish' }
    Receive-Job $writer -ErrorAction Stop | Out-Null
    $checks += 'waits_for_both_nonempty_files'
    Wait-PackageIdentity -Daemon $daemon -IdentityPaths @($first, $second) -TimeoutMilliseconds 100
    $checks += 'accepts_existing_ready_identity'
    $failed = $false
    try {
        Wait-PackageIdentity -Daemon $daemon -IdentityPaths @((Join-Path $evidenceRoot 'missing-first'), (Join-Path $evidenceRoot 'missing-second')) -TimeoutMilliseconds 250
    } catch {
        if ($_.Exception.Message -notmatch 'did not become ready') { throw }
        $failed = $true
    }
    if (!$failed) { throw 'Missing identity did not time out' }
    $checks += 'missing_identity_has_bounded_failure'
    $exited = Start-Process -FilePath (Join-Path $env:SystemRoot 'System32/cmd.exe') -ArgumentList @('/c', 'exit', '0') -WindowStyle Hidden -PassThru
    [void]$exited.Handle
    if (!$exited.WaitForExit(5000)) { throw 'Synthetic exited process did not finish' }
    $failed = $false
    try { Wait-PackageIdentity -Daemon $exited -IdentityPaths @($first, $second) } catch {
        if ($_.Exception.Message -notmatch 'Daemon exited') { throw }
        $failed = $true
    }
    if (!$failed) { throw 'An exited daemon was accepted' }
    $checks += 'exited_daemon_is_rejected_even_with_ready_files'
    [ordered]@{passed=$checks; syntheticFilesOnly=$true} | ConvertTo-Json -Depth 3 |
        Set-Content -LiteralPath (Join-Path $evidenceRoot 'result.json') -Encoding UTF8
    Write-Output "PASS: $($checks.Count) identity readiness checks"
} finally {
    if ($writer) { Stop-Job $writer -ErrorAction SilentlyContinue; Remove-Job $writer -Force -ErrorAction SilentlyContinue }
}
