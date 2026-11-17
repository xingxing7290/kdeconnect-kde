#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
import warnings
import zipfile

spec = importlib.util.spec_from_file_location("source_bundles", Path(__file__).with_name("verify-source-bundles.py"))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class VerifySourceBundlesTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)
        self.record = {"path": "libs/example", "archive": "example.tar.gz", "sourcePart": "part-1.zip",
                       "sha256": hashlib.sha256(b"source fixture").hexdigest()}
        self.index = self.root / "index.json"
        with zipfile.ZipFile(self.root / "part-1.zip", "w") as bundle:
            bundle.writestr("archives/libs__example/example.tar.gz", b"source fixture")

    def check(self):
        self.index.write_text(json.dumps([self.record]), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            return module.verify(self.index, self.root)[0]

    def test_valid_archive(self):
        self.assertTrue(self.check()["ok"])

    def test_wrong_hash_is_rejected(self):
        self.record["sha256"] = "0" * 64
        self.assertEqual("SHA-256 mismatch", self.check()["error"])

    def test_missing_archive_is_rejected(self):
        self.record["archive"] = "missing.tar.gz"
        self.assertEqual("archive entry missing or duplicated", self.check()["error"])

    def test_unsafe_part_is_rejected(self):
        self.record["sourcePart"] = "../part-1.zip"
        self.assertEqual("invalid part name", self.check()["error"])

    def test_missing_part_is_rejected(self):
        self.record["sourcePart"] = "missing.zip"
        self.assertFalse(self.check()["ok"])

    def test_duplicate_zip_member_is_rejected(self):
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(self.root / "part-1.zip", "a") as bundle:
                bundle.writestr("archives/libs__example/example.tar.gz", b"source fixture")
        self.assertEqual("archive entry missing or duplicated", self.check()["error"])

    def test_corrupted_archive_is_rejected(self):
        path = self.root / "part-1.zip"
        raw = path.read_bytes().replace(b"source fixture", b"broken fixture", 1)
        path.write_bytes(raw)
        self.assertFalse(self.check()["ok"])

    def test_duplicate_index_component_is_rejected(self):
        self.index.write_text(json.dumps([self.record, self.record]), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            results = module.verify(self.index, self.root)
        self.assertTrue(results[0]["ok"])
        self.assertEqual("duplicate component", results[1]["error"])

    def test_distinct_components_cannot_share_flattened_archive_path(self):
        first = dict(self.record, path="libs/example")
        second = dict(self.record, path="libs__example")
        self.index.write_text(json.dumps([first, second]), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            results = module.verify(self.index, self.root)
        self.assertTrue(results[0]["ok"])
        self.assertEqual("duplicate archive path", results[1]["error"])


if __name__ == "__main__":
    unittest.main()
