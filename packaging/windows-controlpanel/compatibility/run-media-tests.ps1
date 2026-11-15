param([Parameter(Mandatory=$true)][string]$PackageRoot,
      [Parameter(Mandatory=$true)][string]$Probe,
      [Parameter(Mandatory=$true)][string]$Fixtures,
      [Parameter(Mandatory=$true)][string]$EvidenceDirectory,
      [switch]$ExpectNonRedistributable)
$ErrorActionPreference='Stop'
if (Test-Path $EvidenceDirectory) { throw 'Evidence directory must be new' }
New-Item -ItemType Directory -Path $EvidenceDirectory | Out-Null
$env:PATH="$PackageRoot\bin;$env:SystemRoot\System32;$env:SystemRoot"
$env:QT_PLUGIN_PATH="$PackageRoot\plugins"
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_MEDIA_BACKEND='ffmpeg'
$env:QT_FFMPEG_DECODING_HW_DEVICE_TYPES=','
$packagedProbe=Join-Path $PackageRoot 'bin/media-compatibility-probe.exe'
if (Test-Path $packagedProbe) { throw 'Refuse to replace an existing package probe' }
Copy-Item -LiteralPath $Probe -Destination $packagedProbe
$probeHash=(Get-FileHash -LiteralPath $packagedProbe).Hash
$results=@()
$failures=0
try {
foreach ($case in (Get-Content (Join-Path $Fixtures 'cases.json') -Raw | ConvertFrom-Json)) {
 $index=$results.Count
 $arguments='"'+$PackageRoot+'" "'+(Join-Path $Fixtures $case.name)+'" '+$case.mode
 $stdout=Join-Path $EvidenceDirectory "$index.json"
 $stderr=Join-Path $EvidenceDirectory "$index.stderr.log"
 $process=Start-Process $packagedProbe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
 [void]$process.Handle
 if (!$process.WaitForExit(25000)) {
  $process.Kill($true)
  throw "Media probe timed out: $($case.name)"
 }
 $result=Get-Content $stdout -Raw | ConvertFrom-Json
 if (!$result) { throw "Missing result: $($case.name), exit $($process.ExitCode)" }
 $valid=if ($ExpectNonRedistributable) {
  $result.contentPass -and $result.correctLibrary -and !$result.distributable -and !$result.passed -and !$result.timeout
 } else { $process.ExitCode -eq 0 -and $result.passed -and $result.distributable }
 if (!$valid) { $failures++ }
 $results+=@{case=$case.name;exitCode=$process.ExitCode;expectedNonRedistributable=[bool]$ExpectNonRedistributable;checkPassed=[bool]$valid;result=$result}
 $results | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $EvidenceDirectory 'results.json') -Encoding utf8
 Write-Output "Checked $($case.name): $valid, audioFrames=$($result.audioFrames), videoFrames=$($result.videoFrames)"
}
if ($failures) { throw "$failures media compatibility checks failed" }
Write-Output "PASS $($results.Count) media checks, nonredistributable baseline=$ExpectNonRedistributable"
} finally {
 if ((Get-FileHash -LiteralPath $packagedProbe).Hash -ne $probeHash) { throw 'Probe changed unexpectedly; retain it for inspection' }
 Remove-Item -LiteralPath $packagedProbe
}
