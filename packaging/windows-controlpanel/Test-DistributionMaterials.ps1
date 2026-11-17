param([Parameter(Mandatory = $true)][string]$EvidenceDirectory)
$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $EvidenceDirectory) { throw 'Evidence directory must be new' }
New-Item -ItemType Directory -Path $EvidenceDirectory | Out-Null
$validator = Join-Path $PSScriptRoot 'Assert-DistributionMaterials.ps1'
$valid = @{path = 'libs/example'; archive = 'source.tar.gz'; sha256 = ('a' * 64); sourcePart = 'sources-part-1.zip'}
$missingPart = $valid.Clone(); $missingPart.sourcePart = ''
$traversal = $valid.Clone(); $traversal.sourcePart = '../sources.zip'
$badHash = $valid.Clone(); $badHash.sha256 = 'invalid'
$missingComponent = $valid.Clone(); $missingComponent.path = ''
$traversalComponent = $valid.Clone(); $traversalComponent.path = 'libs/../example'
$traversalArchive = $valid.Clone(); $traversalArchive.archive = '../source.tar.gz'
$backslashArchive = $valid.Clone(); $backslashArchive.archive = 'folder\source.tar.gz'
$cases = @(
    @{name = 'valid'; record = $valid; accepted = $true},
    @{name = 'missing-part'; record = $missingPart; accepted = $false},
    @{name = 'traversal-part'; record = $traversal; accepted = $false},
    @{name = 'bad-hash'; record = $badHash; accepted = $false},
    @{name = 'missing-component'; record = $missingComponent; accepted = $false},
    @{name = 'traversal-component'; record = $traversalComponent; accepted = $false},
    @{name = 'traversal-archive'; record = $traversalArchive; accepted = $false},
    @{name = 'backslash-archive'; record = $backslashArchive; accepted = $false}
)
foreach ($case in $cases) {
    $root = Join-Path $EvidenceDirectory $case.name
    New-Item -ItemType Directory -Path (Join-Path $root 'licenses/third-party') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $root 'THIRD-PARTY-NOTICES.txt') -Value 'test fixture' -Encoding utf8
    ConvertTo-Json -InputObject @($case.record) | Set-Content -LiteralPath (Join-Path $root 'source-index.json') -Encoding utf8
    $accepted = $true
    try { & $validator -DistributionMaterialsRoot $root | Out-Null } catch { $accepted = $false }
    if ($accepted -ne $case.accepted) { throw "Unexpected result for $($case.name)" }
    Write-Output "PASS: $($case.name)"
}
$duplicateRoot = Join-Path $EvidenceDirectory 'duplicate-component'
New-Item -ItemType Directory -Path (Join-Path $duplicateRoot 'licenses/third-party') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $duplicateRoot 'THIRD-PARTY-NOTICES.txt') -Value 'test fixture' -Encoding utf8
ConvertTo-Json -InputObject @($valid, $valid) | Set-Content -LiteralPath (Join-Path $duplicateRoot 'source-index.json') -Encoding utf8
try {
    & $validator -DistributionMaterialsRoot $duplicateRoot | Out-Null
    throw 'Expected duplicate rejection'
} catch {
    if ($_.Exception.Message -notmatch 'Duplicate source index entry') { throw }
    Write-Output 'PASS: duplicate-component'
}
$collisionRoot = Join-Path $EvidenceDirectory 'duplicate-archive-path'
New-Item -ItemType Directory -Path (Join-Path $collisionRoot 'licenses/third-party') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $collisionRoot 'THIRD-PARTY-NOTICES.txt') -Value 'test fixture' -Encoding utf8
$collision = $valid.Clone(); $collision.path = 'libs__example'
ConvertTo-Json -InputObject @($valid, $collision) | Set-Content -LiteralPath (Join-Path $collisionRoot 'source-index.json') -Encoding utf8
try {
    & $validator -DistributionMaterialsRoot $collisionRoot | Out-Null
    throw 'Expected duplicate archive path rejection'
} catch {
    if ($_.Exception.Message -notmatch 'Duplicate archive path:') { throw }
    Write-Output 'PASS: duplicate-archive-path'
}
