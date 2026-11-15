#requires -Version 7.4
# SPDX-License-Identifier: GPL-2.0-or-later
param(
    [Parameter(Mandatory = $true)][ValidateSet('kdeconnect-android', 'kdeconnect-kde')][string]$Repository,
    [Parameter(Mandatory = $true)][string]$Tag,
    [Parameter(Mandatory = $true)][string]$NotesFile,
    [string[]]$Assets = @(),
    [switch]$Publish
)
$ErrorActionPreference = 'Stop'
$env:GIT_TERMINAL_PROMPT = '0'
$env:GCM_INTERACTIVE = 'Never'
$credentialRequest = @('protocol=https', 'host=github.com', '') -join "`n"
$credentialResponse = ($credentialRequest | git credential fill 2>$null) -join "`n"
$token = ([regex]::Match($credentialResponse, '(?m)^password=(.+)$')).Groups[1].Value.Trim()
if (!$token) { throw 'No non-interactive GitHub credential available' }
$headers = @{Authorization="Bearer $token"; 'User-Agent'='KDE-ControlPanel-Preview'; Accept='application/vnd.github+json'}
$api = "https://api.github.com/repos/xingxing7290/$Repository"
$notes = Get-Content -LiteralPath $NotesFile -Raw
$releases = Invoke-RestMethod -Uri "$api/releases?per_page=100" -Headers $headers -TimeoutSec 45
$release = @(foreach ($candidate in $releases) { if ($candidate.tag_name -eq $Tag) { $candidate } })
if ($release.Count -gt 1) { throw 'Ambiguous release' }
if ($release.Count -eq 1) {
    $release = $release[0]
    if (!$release.draft) { throw 'Refusing to overwrite an already published release' }
} else {
    if ($Tag -notmatch '^controlpanel-(?:release-)?v([0-9]+\.[0-9]+\.[0-9]+-alpha\.[0-9]+)$') { throw 'Unexpected preview tag' }
    $releaseName = '控制面板 ' + $Matches[1] + ' 功能体验版'
    if ($Tag.StartsWith('controlpanel-release-')) { $releaseName += ' (Release构建)' }
    $payload = @{tag_name=$Tag; name=$releaseName; body=$notes; draft=$true; prerelease=$true} | ConvertTo-Json
    $release = Invoke-RestMethod -Method Post -Uri "$api/releases" -Headers $headers -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($payload)) -TimeoutSec 45
}
if ("$($release.id)" -notmatch '^\d+$') { throw 'Release selection must resolve to exactly one numeric ID' }
foreach ($assetPath in $Assets) {
    $release = Invoke-RestMethod -Uri "$api/releases/$($release.id)" -Headers $headers -TimeoutSec 45
    if (!$release.draft -or $release.tag_name -ne $Tag) { throw 'Release state changed while uploading' }
    $asset = Get-Item -LiteralPath $assetPath
    if ($asset.PSIsContainer -or $asset.Length -le 0) { throw 'Asset must be a nonempty file' }
    $existing = @($release.assets | Where-Object name -eq $asset.Name)
    if ($existing.Count) {
        $digest = 'sha256:' + (Get-FileHash -LiteralPath $asset.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($existing.Count -eq 1 -and $existing[0].size -eq $asset.Length -and $existing[0].digest -eq $digest) {
            Write-Output "Verified existing asset: $($asset.Name)"
            continue
        }
        throw "Refusing to replace a different existing asset: $($asset.Name)"
    }
    $upload = "https://uploads.github.com/repos/xingxing7290/$Repository/releases/$($release.id)/assets?name=$([uri]::EscapeDataString($asset.Name))"
    $uploaded = Invoke-RestMethod -Method Post -Uri $upload -Headers $headers -ContentType 'application/octet-stream' -InFile $asset.FullName -TimeoutSec 300 -HttpVersion 1.1 -DisableKeepAlive
    if ($uploaded.size -ne $asset.Length -or $uploaded.state -ne 'uploaded') { throw 'Uploaded asset did not match local size/state' }
    if ($uploaded.digest -and $uploaded.digest -ne ('sha256:' + (Get-FileHash -LiteralPath $asset.FullName -Algorithm SHA256).Hash.ToLowerInvariant())) {
        throw 'Uploaded asset digest mismatch'
    }
    Write-Output "Uploaded: $($uploaded.name) ($($uploaded.size) bytes)"
}
if ($Publish) {
    $payload = @{body=$notes; draft=$false; prerelease=$true; make_latest='false'} | ConvertTo-Json
    $release = Invoke-RestMethod -Method Patch -Uri "$api/releases/$($release.id)" -Headers $headers -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($payload)) -TimeoutSec 45
}
[pscustomobject]@{url=$release.html_url; tag=$release.tag_name; draft=$release.draft; prerelease=$release.prerelease} | ConvertTo-Json
