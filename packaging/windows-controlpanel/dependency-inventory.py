#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Read Craft provenance for every deployed file; never modifies Craft binaries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--craft', type=Path, required=True)
parser.add_argument('--package', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if args.output.exists():
    raise SystemExit('Output already exists; retain earlier evidence')
os.environ['craftRoot'] = str(args.craft / 'craft')
sys.path.insert(0, str(args.craft / 'craft/bin'))
from Blueprints.CraftPackageObject import CraftPackageObject

db = sqlite3.connect(f'file:{args.craft.as_posix()}/etc/blueprints/install.db?mode=ro', uri=True)
packages = {row[0]: {'path': row[1], 'version': row[2], 'revision': row[3], 'cache': row[4]}
            for row in db.execute('select packageId,packagePath,version,revision,cacheVersion from packageList')}
owners = {}
for package_id, filename, digest in db.execute('select packageId,filename,fileHash from fileList'):
    owners.setdefault(filename.replace('\\', '/').lower(), []).append((package_id, digest))
used = set()
files = []
unowned = []
for file in sorted(args.package.rglob('*')):
    if not file.is_file():
        continue
    relative = file.relative_to(args.package).as_posix()
    key = relative.lower()
    candidates = [key]
    if key.startswith(('lib/qml/', 'lib/plugins/')):
        candidates.append(key[4:])
    matches = [entry for candidate in candidates for entry in owners.get(candidate, [])]
    if not matches:
        # Local application files and generated deployment metadata remain explicit.
        unowned.append(relative)
        continue
    digest = hashlib.file_digest(file.open('rb'), 'sha256').hexdigest()
    ids = sorted({entry[0] for entry in matches})
    used.update(ids)
    files.append({'file': relative, 'sha256': digest,
                  'packages': [packages[i]['path'] for i in ids],
                  'matchesInstallDatabase': any(h.lower() == '[sha256]' + digest for _, h in matches)})
# These components are incorporated statically or as headers by the deployed
# Qt/FFmpeg libraries, so looking only for shipped DLLs would omit their source.
incorporated = ('libs/libvpx', 'libs/libass', 'libs/fribidi', 'libs/aom',
                'libs/amf', 'libs/nvidia-codecs', 'libs/onevpl', 'libs/giflib',
                'libs/libarchive', 'libs/sqlite')
for package_id, package in packages.items():
    if package['path'] in incorporated:
        used.add(package_id)
result = {'files': files, 'unowned': unowned, 'incorporated': incorporated, 'packages': []}
for package_id in sorted(used):
    record = packages[package_id]
    package = CraftPackageObject.get(record['path'])
    if package is None:
        raise SystemExit(f'Missing recipe: {record}')
    instance = package.instance
    info = instance.subinfo
    target = info.defaultTarget
    record = dict(record, recipeVersion=str(package.version), target=str(target),
                  source=info.targets.get(target), git=info.svnTargets.get(target),
                  digest=repr(info.targetDigests.get(target)),
                  patches=[str(x) for x in info.patchToApply.get(target, [])],
                  recipe=str(package.source))
    result['packages'].append(record)
    print(f"{record['path']} installed={record['version']} recipe={record['recipeVersion']} source={record['source']} git={record['git']}", flush=True)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2, default=str), encoding='utf-8')
print(f'FILES={len(files)} PACKAGES={len(used)} UNOWNED={len(unowned)}', flush=True)
