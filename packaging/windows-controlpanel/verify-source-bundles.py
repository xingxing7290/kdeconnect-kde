#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Verify indexed source archives inside local release ZIP parts without extraction."""
import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys
import zipfile


def verify(index_path, parts_dir):
    records = json.loads(index_path.read_text(encoding="utf-8"))
    if not isinstance(records, list) or not records:
        raise ValueError("Source index must be a nonempty array")
    results = []
    with ExitStack() as stack:
        parts = {}
        seen = set()
        seen_members = set()
        for record in records:
            if not isinstance(record, dict):
                raise ValueError("Source index entries must be objects")
            component = record.get("path", "")
            archive = record.get("archive", "")
            part = record.get("sourcePart", "")
            expected = record.get("sha256", "")
            result = {"component": component, "part": part, "archive": archive, "ok": False}
            try:
                if not isinstance(component, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*", component):
                    raise ValueError("invalid component path")
                if any(piece in (".", "..") for piece in component.split("/")):
                    raise ValueError("invalid component path")
                if component.casefold() in seen:
                    raise ValueError("duplicate component")
                seen.add(component.casefold())
                if not isinstance(archive, str) or not archive or PurePosixPath(archive).name != archive or "\\" in archive:
                    raise ValueError("invalid archive name")
                if not isinstance(part, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*\.zip", part):
                    raise ValueError("invalid part name")
                if not isinstance(expected, str) or not re.fullmatch(r"[a-fA-F0-9]{64}", expected):
                    raise ValueError("invalid SHA-256")
                member = f"archives/{component.replace('/', '__')}/{archive}"
                member_key = (part.casefold(), member.casefold())
                if member_key in seen_members:
                    raise ValueError("duplicate archive path")
                seen_members.add(member_key)
                if part not in parts:
                    parts[part] = stack.enter_context(zipfile.ZipFile(parts_dir / part))
                bundle = parts[part]
                entries = [entry for entry in bundle.infolist() if entry.filename == member and not entry.is_dir()]
                if len(entries) != 1:
                    raise ValueError("archive entry missing or duplicated")
                digest = hashlib.sha256()
                with bundle.open(entries[0]) as source:
                    while chunk := source.read(1024 * 1024):
                        digest.update(chunk)
                result["actualSha256"] = digest.hexdigest()
                if digest.hexdigest().lower() != expected.lower():
                    raise ValueError("SHA-256 mismatch")
                result["ok"] = True
            except (OSError, ValueError, zipfile.BadZipFile, KeyError) as error:
                result["error"] = str(error)
            results.append(result)
            print(f"{'PASS' if result['ok'] else 'FAIL'} {component}", flush=True)
    return results


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--index", type=Path, required=True)
    parser.add_argument("--parts", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if args.report.exists():
        parser.error("Report already exists")
    results = verify(args.index, args.parts)
    with args.report.open("x", encoding="utf-8") as output:
        json.dump(results, output, ensure_ascii=False, indent=2)
    failures = sum(not item["ok"] for item in results)
    print(f"Checked {len(results)} components; failures={failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
