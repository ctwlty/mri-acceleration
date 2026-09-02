from __future__ import annotations

import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True

SOURCE_ROOT = Path(__file__).resolve().parents[1] / "src"
sys.path.insert(0, str(SOURCE_ROOT))

from firstech_sequence_mcp.pe import inspect_pe  # noqa: E402


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: pe_probe.py <pe-file>", file=sys.stderr)
        return 2
    try:
        result = inspect_pe(Path(sys.argv[1]))
    except Exception as exc:
        print(json.dumps({"status": "error", "error": str(exc)}), file=sys.stderr)
        return 1
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
