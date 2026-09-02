from __future__ import annotations

import json
import os
import re
import stat
import sys
from pathlib import Path
from typing import Any

sys.dont_write_bytecode = True

from package_policy import (  # noqa: E402
    MANIFEST_NAME,
    PACKAGE_ID,
    PackagePolicyError,
    inventory,
    validate_relative_path,
)

SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")


def _fail(reason: str) -> int:
    print(json.dumps({"status": "FAIL", "reason": reason}, ensure_ascii=False, indent=2))
    return 1


def _expected_records(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    raw_files = manifest.get("files")
    if not isinstance(raw_files, list):
        raise PackagePolicyError("manifest files must be a list")
    expected: dict[str, dict[str, Any]] = {}
    seen_casefolded: set[str] = set()
    for item in raw_files:
        if not isinstance(item, dict) or not isinstance(item.get("path"), str):
            raise PackagePolicyError("invalid manifest file record")
        relative = item["path"]
        validate_relative_path(relative)
        folded = relative.casefold()
        if relative in expected or folded in seen_casefolded:
            raise PackagePolicyError(f"duplicate manifest path: {relative}")
        if not isinstance(item.get("size"), int) or item["size"] < 0:
            raise PackagePolicyError(f"invalid manifest size: {relative}")
        if not isinstance(item.get("sha256"), str) or not SHA256_PATTERN.fullmatch(
            item["sha256"]
        ):
            raise PackagePolicyError(f"invalid manifest SHA-256: {relative}")
        expected[relative] = item
        seen_casefolded.add(folded)
    return expected


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: verify_package.py <manifest.json> <package-root>", file=sys.stderr)
        return 2
    try:
        root = Path(sys.argv[2]).resolve(strict=True)
        manifest_path = Path(sys.argv[1]).resolve(strict=True)
        if manifest_path != root / MANIFEST_NAME:
            raise PackagePolicyError("manifest must be package-root/package-manifest.json")
        manifest_info = os.lstat(manifest_path)
        if not stat.S_ISREG(manifest_info.st_mode):
            raise PackagePolicyError("manifest is not a regular file")
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if not isinstance(manifest, dict):
            raise PackagePolicyError("manifest root must be an object")
        if manifest.get("schema") != "handoff-package-manifest/v1":
            raise PackagePolicyError("unsupported schema")
        if manifest.get("package_id") != PACKAGE_ID:
            raise PackagePolicyError("manifest package_id is not the expected module identity")
        if manifest.get("manifest_excludes") != [MANIFEST_NAME]:
            raise PackagePolicyError("manifest may exclude only package-manifest.json")
        expected = _expected_records(manifest)
        if manifest.get("file_count") != len(expected):
            raise PackagePolicyError("manifest file_count is inconsistent")
        expected_total = sum(item["size"] for item in expected.values())
        if manifest.get("total_bytes") != expected_total:
            raise PackagePolicyError("manifest total_bytes is inconsistent")

        actual_records = inventory(root, excluded={MANIFEST_NAME})
        actual = {item["path"]: item for item in actual_records}
        missing = sorted(set(expected) - set(actual))
        extra = sorted(set(actual) - set(expected))
        mismatched = [
            {
                "path": relative,
                "expected_size": expected[relative]["size"],
                "actual_size": actual[relative]["size"],
                "expected_sha256": expected[relative]["sha256"],
                "actual_sha256": actual[relative]["sha256"],
            }
            for relative in sorted(set(expected) & set(actual))
            if expected[relative]["size"] != actual[relative]["size"]
            or expected[relative]["sha256"] != actual[relative]["sha256"]
        ]
        status = "PASS" if not (missing or extra or mismatched) else "FAIL"
        report = {
            "status": status,
            "package_id": PACKAGE_ID,
            "directory_name": root.name,
            "file_count": len(actual),
            "total_bytes": sum(item["size"] for item in actual.values()),
            "missing": missing,
            "extra": extra,
            "mismatched": mismatched,
            "policy": "no generated caches, reparse points, executables, RAW, or archives",
        }
        print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
        return 0 if status == "PASS" else 1
    except (OSError, json.JSONDecodeError, PackagePolicyError) as exc:
        return _fail(str(exc))


if __name__ == "__main__":
    raise SystemExit(main())
