from __future__ import annotations

import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True

SOURCE_ROOT = Path(__file__).resolve().parents[1] / "src"
sys.path.insert(0, str(SOURCE_ROOT))

from firstech_sequence_mcp.config import Settings  # noqa: E402
from firstech_sequence_mcp.safety import inspect_toolchain  # noqa: E402


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: probe_toolchain.py <config.toml> <output.json>", file=sys.stderr)
        return 2
    config_path = Path(sys.argv[1]).resolve(strict=True)
    output_path = Path(sys.argv[2]).resolve(strict=False)
    if output_path.exists():
        print(f"refusing to overwrite evidence: {output_path}", file=sys.stderr)
        return 1
    try:
        settings = Settings.load(config_path)
        for vendor_root in settings.vendor_install_roots:
            try:
                output_path.relative_to(vendor_root.resolve(strict=False))
            except ValueError:
                continue
            raise ValueError("probe output may not be written inside a vendor install root")
        result = inspect_toolchain(settings)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with output_path.open("x", encoding="utf-8", newline="\n") as handle:
            json.dump(result, handle, ensure_ascii=False, indent=2, sort_keys=True)
            handle.write("\n")
    except Exception as exc:
        print(f"probe failed: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 1
    print(str(output_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
