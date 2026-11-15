#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Create readable notices and a source bundle, excluding caches, keys and Git history."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tarfile
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--sources', type=Path, required=True)
p.add_argument('--ffmpeg', type=Path, required=True)
p.add_argument('--microsoft', type=Path, required=True)
p.add_argument('--build-system', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--bundle', type=Path, required=True)
a = p.parse_args()
if a.output.exists() or a.bundle.exists():
    raise SystemExit('Refusing to overwrite release materials')
a.output.mkdir(parents=True)
license_root = a.output / 'licenses/third-party'
index = []
archives = []
records = json.loads((a.sources/'manifest.json').read_text(encoding='utf-8'))
for r in sorted(records, key=lambda x: x['path']):
    component = r['path'].replace('/', '__')
    directory = a.sources / component
    if r['path'] == 'libs/runtime':
        continue
    if r['path'] in ('libs/x264', 'libs/x265'):
        candidates = list(directory.glob('*.zip'))
        if len(candidates) != 1:
            raise SystemExit(f'Missing pinned Git source archive: {r["path"]}')
        archive = candidates[0]
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                if Path(member.filename).name.upper().startswith(('COPYING', 'LICENSE', 'README', 'AUTHORS')) and not member.is_dir():
                    dest = license_root / component / Path(member.filename).name
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(source.read(member))
        r['status'] = 'fetched-pinned-git-commit'
    else:
        if r['status'] != 'fetched':
            raise SystemExit(f'Unresolved source: {r["path"]}')
        archive = Path(r['archive'])
        if r['path'] == 'libs/amf':
            # Only the AMF public headers are compiled into FFmpeg. The upstream
            # SDK repository also contains unrelated prebuilt FFmpeg binaries;
            # those are not corresponding source and must not be redistributed.
            original = archive
            archive = directory/'amf-1.5.2-public-headers.zip'
            if not archive.exists():
                with tarfile.open(original) as source, zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as subset:
                    for member in source:
                        name = Path(member.name)
                        root_notice = len(name.parts) == 2 and name.name.lower().startswith(('license', 'readme'))
                        if member.isfile() and ('/amf/public/include/' in member.name or root_notice):
                            subset.writestr(member.name, source.extractfile(member).read())
            r['originArchiveSha256'] = r['sha256']
            r['originArchiveUpstreamChecksumVerified'] = r['upstreamChecksumVerified']
            r['upstreamChecksumVerified'] = False
            r['sourceSubset'] = 'Complete AMF public headers and root notices; unrelated prebuilt SDK samples excluded'
        notices = directory/'notices'
        if r['path'] == 'libs/amf':
            with zipfile.ZipFile(archive) as subset:
                for name in subset.namelist():
                    if len(Path(name).parts) == 2:
                        dest = license_root/component/Path(name).name
                        dest.parent.mkdir(parents=True, exist_ok=True)
                        dest.write_bytes(subset.read(name))
        elif notices.exists():
            shutil.copytree(notices, license_root/component, dirs_exist_ok=True)
        if r['path'] == 'libs/sqlite':
            # SQLite's public-domain dedication is in the amalgamation header.
            with zipfile.ZipFile(archive) as source:
                header = next(x for x in source.namelist() if x.endswith('/sqlite3.h'))
                dest = license_root/component/'sqlite3-header-notice.txt'
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(b'\n'.join(source.read(header).splitlines()[:35]))
    with archive.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    r = dict(r, archive=archive.name, sha256=digest)
    index.append(r)
    archives.append((archive, f'archives/{component}/{archive.name}'))
    if not any(x.is_file() for x in (license_root/component).rglob('*')):
        raise SystemExit(f'No readable notices: {component}')
shutil.copytree(a.microsoft, license_root/'microsoft', dirs_exist_ok=True)
ffmpeg_license = license_root/'ffmpeg-build'
ffmpeg_license.mkdir()
for name in ('COPYING.GPLv3', 'LICENSE.md', 'COPYING.LGPLv3', 'COPYING.LGPLv2.1', 'COPYING.GPLv2'):
    shutil.copy2(a.ffmpeg/'ffmpeg-8.1.1'/name, ffmpeg_license/name)
parts = []
current = []
current_bytes = 0
for archive, name in archives:
    if current and current_bytes + archive.stat().st_size > 110*1024*1024:
        parts.append(current)
        current, current_bytes = [], 0
    current.append((archive, name))
    current_bytes += archive.stat().st_size
if current:
    parts.append(current)
for number, part in enumerate(parts, 1):
    part_name = a.bundle.stem+f'-part-{number}.zip'
    for archive, member_name in part:
        for record in index:
            if member_name.startswith('archives/'+record['path'].replace('/', '__')+'/'):
                record['sourcePart'] = part_name
source_index = a.output/'source-index.json'
source_index.write_text(json.dumps(index, ensure_ascii=False, indent=2), encoding='utf-8')
lines = ['KDE Control Panel personal fork - third-party notices',
         'This is not an official KDE release.',
         'This distribution selects GPLv3 for the KDE application and FFmpeg build.',
         'Individual dependencies retain their original licenses and copyright notices.',
         'FFmpeg: GPL version 3 or later; --enable-nonfree is NOT enabled.',
         'Qt/KDE and all other notices: licenses/third-party and licenses/kdeconnect.',
         'Microsoft CRT and Windows SDK redistributables retain Microsoft license terms.',
         'No debug_nonredist binaries or private pairing/signing keys are included.',
         '', 'Complete corresponding open-source archives, build recipes and patches:',
         'https://github.com/xingxing7290/kdeconnect-kde/releases/tag/controlpanel-release-v0.1.0-alpha.4',
         'Asset: kdeconnect-controlpanel-0.1.0-alpha.4-third-party-sources.zip',
         'Also download all third-party-sources-part-N.zip assets; extract each into the same folder.',
         'The application source archive is a separate asset on the same release.',
         'Source hashes and origins: source-index.json. No reverse-engineering restriction is imposed.',
         'Microsoft system redistributables are proprietary; their source is not supplied.',
         '', 'Component | Installed version | Source target']
lines += [f"{r['path']} | {r['version']} | {r['target']}" for r in index]
(a.output/'THIRD-PARTY-NOTICES.txt').write_text('\n'.join(lines)+'\n', encoding='utf-8')
with zipfile.ZipFile(a.bundle, 'x', compression=zipfile.ZIP_STORED, allowZip64=True) as bundle:
    for file in sorted(a.build_system.rglob('*')):
        if file.is_file():
            bundle.write(file, 'build-system/'+file.relative_to(a.build_system).as_posix())
    for name in ('ffmpeg-8.1.1.tar.xz', 'ffmpeg-8.1.1.tar.xz.asc', 'ffmpeg-devel.asc'):
        bundle.write(a.ffmpeg/name, 'ffmpeg-original/'+name)
    for file in sorted(a.output.rglob('*')):
        if file.is_file():
            bundle.write(file, 'release-materials/'+file.relative_to(a.output).as_posix())
for number, part in enumerate(parts, 1):
    part_file = a.bundle.with_name(a.bundle.stem+f'-part-{number}.zip')
    with zipfile.ZipFile(part_file, 'x', compression=zipfile.ZIP_STORED) as bundle:
        for file, name in part:
            bundle.write(file, name)
    print(f'PART {number}: {part_file.stat().st_size} bytes', flush=True)
print(f'PASS {len(index)} open-source components; main bundle={a.bundle.stat().st_size}; parts={len(parts)}', flush=True)
