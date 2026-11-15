#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Fetch pinned Craft source archives and preserve their original notices."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import tarfile
import time
import urllib.parse
import urllib.request
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('inventory', type=Path)
parser.add_argument('destination', type=Path)
args = parser.parse_args()
packages = json.loads(args.inventory.read_text(encoding='utf-8'))['packages']
args.destination.mkdir(parents=True, exist_ok=True)

def fetch(record):
    source = record['source']
    if not source:
        return dict(record, status='requires-separate-source')
    if not isinstance(source, str):
        raise ValueError(f'Multiple source archives need explicit handling: {record}')
    component = args.destination / record['path'].replace('/', '__')
    component.mkdir(exist_ok=True)
    name = Path(urllib.parse.urlparse(source).path).name
    archive = component / name
    expected = re.findall(r"'([a-fA-F0-9]{40,128})'", record['digest'])
    algorithm = 'sha512' if 'SHA512' in record['digest'] else 'sha256'
    if not archive.exists():
        for attempt in range(3):
            try:
                request = urllib.request.Request(source, headers={'User-Agent': 'KDE-ControlPanel-Source-Archive'})
                with urllib.request.urlopen(request, timeout=45) as response, archive.with_suffix(archive.suffix+'.part').open('wb') as output:
                    while data := response.read(1024*1024):
                        output.write(data)
                archive.with_suffix(archive.suffix+'.part').replace(archive)
                break
            except Exception:
                if attempt == 2:
                    raise
                time.sleep(1)
    with archive.open('rb') as stream:
        digest = hashlib.file_digest(stream, algorithm).hexdigest()
    if expected and digest.lower() not in [h.lower() for h in expected]:
        raise ValueError(f'Upstream checksum mismatch: {archive}')
    with archive.open('rb') as stream:
        sha256 = hashlib.file_digest(stream, 'sha256').hexdigest()
    license_dir = component / 'notices'
    count = 0
    def preserve(name, size, reader):
        nonlocal count
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or size > 4*1024*1024:
            return
        if not (re.match(r'(?i)^(copying|licen[sc]e|notice|copyright|authors|readme)([._-]|$)', path.name)
                or 'LICENSES' in path.parts or path.name == 'qt_attribution.json'):
            return
        target = license_dir.joinpath(*path.parts)
        target.parent.mkdir(parents=True, exist_ok=True)
        with reader() as input_stream:
            target.write_bytes(input_stream.read())
        count += 1
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as bundle:
            for item in bundle.infolist():
                if not item.is_dir():
                    preserve(item.filename, item.file_size, lambda i=item: bundle.open(i))
    else:
        with tarfile.open(archive) as bundle:
            for item in bundle:
                if item.isfile():
                    preserve(item.name, item.size, lambda i=item: bundle.extractfile(i))
    result = dict(record, archive=str(archive), sha256=sha256, upstreamChecksumVerified=bool(expected), notices=count, status='fetched')
    (component/'provenance.json').write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
    print(f"PASS {record['path']} {archive.stat().st_size} bytes, {count} notices", flush=True)
    return result

results = []
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    futures = {executor.submit(fetch, record): record for record in packages}
    for future in concurrent.futures.as_completed(futures):
        record = futures[future]
        try:
            results.append(future.result())
        except Exception as error:
            print(f"FAIL {record['path']}: {error}", flush=True)
            results.append(dict(record, status='failed', error=str(error)))
        (args.destination/'manifest.json').write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding='utf-8')
if any(item['status'] == 'failed' for item in results):
    raise SystemExit(1)
