param([Parameter(Mandatory = $true)][string]$DistributionMaterialsRoot)
$ErrorActionPreference = 'Stop'
foreach ($required in @('source-index.json', 'THIRD-PARTY-NOTICES.txt', 'licenses/third-party')) {
    if (!(Test-Path -LiteralPath (Join-Path $DistributionMaterialsRoot $required))) { throw "Missing distribution materials: $required" }
}
$records = @(Get-Content -LiteralPath (Join-Path $DistributionMaterialsRoot 'source-index.json') -Raw | ConvertFrom-Json)
if ($records.Count -eq 0) { throw 'The source index contains no components' }
$components = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$archivePaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($record in $records) {
    $part = [string]$record.sourcePart
    $component = [string]$record.path
    $archive = [string]$record.archive
    if ($component -notmatch '^[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*$' -or
        @($component -split '/' | Where-Object { $_ -eq '.' -or $_ -eq '..' }).Count -gt 0 -or
        [string]::IsNullOrWhiteSpace($archive) -or $archive -match '[/\\]' -or $archive -in @('.', '..') -or
        [string]$record.sha256 -notmatch '^[a-fA-F0-9]{64}$' -or
        $part -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*\.zip$') {
        throw "Incomplete source index entry: $($record.path)"
    }
    if (!$components.Add([string]$record.path)) { throw "Duplicate source index entry: $($record.path)" }
    $member = 'archives/' + ([string]$record.path).Replace('/', '__') + '/' + [string]$record.archive
    if (!$archivePaths.Add($part + "`0" + $member)) { throw "Duplicate archive path: $member" }
}
Write-Output "Source index structure verified: $($records.Count) components"
