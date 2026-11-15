param([Parameter(Mandatory=$true)][string]$FFmpeg,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if (Test-Path $OutputDirectory) { throw 'Fixture directory must be new' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$cases=@(
 @{name='pcm.wav';mode='audio';audio='pcm_s16le'},
 @{name='mp3.mp3';mode='audio';audio='libmp3lame'},
 @{name='aac.m4a';mode='audio';audio='aac'},
 @{name='flac.flac';mode='audio';audio='flac'},
 @{name='vorbis.ogg';mode='audio';audio='libvorbis'},
 @{name='opus.opus';mode='audio';audio='libopus'},
 @{name='h264-aac.mp4';mode='video';video='libx264';audio='aac';extra=@('-preset','ultrafast')},
 @{name='hevc-aac.mp4';mode='video';video='libx265';audio='aac';extra=@('-preset','ultrafast')},
 @{name='vp9-opus.webm';mode='video';video='libvpx-vp9';audio='libopus';extra=@('-deadline','realtime','-cpu-used','8')},
 @{name='av1-aac.mp4';mode='video';video='libsvtav1';audio='aac';extra=@('-preset','12')}
)
foreach ($case in $cases) {
 $arguments=@('-nostdin','-v','error')
 if ($case.mode -eq 'video') { $arguments+=@('-f','lavfi','-i','testsrc2=size=160x96:rate=10') }
 $arguments+=@('-f','lavfi','-i','sine=frequency=440:sample_rate=48000','-t','2')
 if ($case.mode -eq 'video') { $arguments+=@('-c:v',$case.video); $arguments+=@($case.extra) }
 $arguments+=@('-c:a',$case.audio,(Join-Path $OutputDirectory $case.name))
 & $FFmpeg @arguments *> (Join-Path $OutputDirectory ($case.name+'.generate.log'))
 if ($LASTEXITCODE) { throw "Fixture encoder failed: $($case.name)" }
 Write-Output "Generated $($case.name)"
}
Copy-Item (Join-Path $OutputDirectory 'h264-aac.mp4') (Join-Path $OutputDirectory '中文 media.mp4')
$cases+=@{name='中文 media.mp4';mode='seek'}
# A deliberately invalid local file: no network access, private files or external media.
[IO.File]::WriteAllBytes((Join-Path $OutputDirectory 'invalid.bin'),[Text.Encoding]::UTF8.GetBytes('not a media container'))
$cases+=@{name='invalid.bin';mode='invalid'}
$cases | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputDirectory 'cases.json') -Encoding utf8
