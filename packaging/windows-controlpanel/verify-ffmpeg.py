#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Inspect the actual bundled DLLs; never trust the name of a downloaded build."""
import ctypes
import json
import os
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() / 'bin'
handle = os.add_dll_directory(str(root))
libraries = {'avcodec-62.dll': 'avcodec', 'avdevice-62.dll': 'avdevice',
             'avfilter-11.dll': 'avfilter', 'avformat-62.dll': 'avformat',
             'avutil-60.dll': 'avutil', 'swresample-6.dll': 'swresample', 'swscale-9.dll': 'swscale'}
results = []
for name, prefix in libraries.items():
    library = ctypes.CDLL(str(root / name))
    get_license = getattr(library, prefix + '_license')
    get_configuration = getattr(library, prefix + '_configuration')
    get_license.restype = get_configuration.restype = ctypes.c_char_p
    license_text = get_license().decode()
    configuration = get_configuration().decode()
    if 'nonfree' in license_text.lower() or 'unredistributable' in license_text.lower() or '--enable-nonfree' in configuration:
        raise SystemExit(f'Refusing nonredistributable FFmpeg DLL: {name}: {license_text}')
    if license_text != 'GPL version 3 or later':
        raise SystemExit(f'Unexpected FFmpeg license, review before distribution: {name}: {license_text}')
    results.append({'file': name, 'license': license_text, 'configuration': configuration})
print(json.dumps(results, indent=2))
