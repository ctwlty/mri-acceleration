from __future__ import annotations

import json
import sys
from datetime import UTC, datetime
from pathlib import Path

sys.dont_write_bytecode = True

from package_policy import (  # noqa: E402
    MANIFEST_NAME,
    PACKAGE_ID,
    PackagePolicyError,
    inventory,
)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: build_manifest.py <package-root> <manifest-output>", file=sys.stderr)
        return 2
    try:
        root = Path(sys.argv[1]).resolve(strict=True)
        output = Path(sys.argv[2]).resolve(strict=False)
        required_output = root / MANIFEST_NAME
        if output != required_output:
            raise PackagePolicyError(f"manifest output must be {required_output}")
        if output.exists():
            raise PackagePolicyError(f"refusing to overwrite manifest: {output}")
        files = inventory(root, excluded={MANIFEST_NAME})
        payload = {
            "schema": "handoff-package-manifest/v1",
            "package_id": PACKAGE_ID,
            "source_directory_name": root.name,
            "created_at": datetime.now(UTC).isoformat(),
            "manifest_excludes": [MANIFEST_NAME],
            "file_count": len(files),
            "total_bytes": sum(item["size"] for item in files),
            "files": files,
            "binary_policy": (
                "vendor-manual PDFs only; no DLL/EXE/LIB/PDB/RAW/SDK/archive/runtime cache"
            ),
        }
        with output.open("x", encoding="utf-8", newline="\n") as handle:
            json.dump(payload, handle, ensure_ascii=False, indent=2, sort_keys=True)
            handle.write("\n")
    except (OSError, PackagePolicyError) as exc:
        print(f"manifest build failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
