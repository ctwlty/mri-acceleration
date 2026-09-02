from __future__ import annotations

import json
import struct
import sys
from pathlib import Path
from typing import Any

from .core import sha256_file
from .errors import EvidenceError

MACHINE_NAMES = {
    0x014C: "x86",
    0x8664: "x64",
    0x01C0: "ARM",
    0xAA64: "ARM64",
}


def _unpack_from(fmt: str, data: bytes, offset: int) -> tuple[Any, ...]:
    size = struct.calcsize(fmt)
    if offset < 0 or offset + size > len(data):
        raise EvidenceError("truncated PE structure")
    return struct.unpack_from(fmt, data, offset)


def _read_c_string(data: bytes, offset: int, limit: int = 4096) -> str:
    if offset < 0 or offset >= len(data):
        raise EvidenceError("PE string offset outside file")
    end = data.find(b"\0", offset, min(len(data), offset + limit))
    if end < 0:
        raise EvidenceError("unterminated PE export name")
    return data[offset:end].decode("ascii", errors="replace")


def inspect_pe(path: Path) -> dict[str, Any]:
    target = path.resolve(strict=True)
    data = target.read_bytes()
    if len(data) < 64 or data[:2] != b"MZ":
        raise EvidenceError(f"not a PE file: {target}")
    (pe_offset,) = _unpack_from("<I", data, 0x3C)
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise EvidenceError(f"invalid PE signature: {target}")

    coff_offset = pe_offset + 4
    machine, section_count, timestamp, _, _, optional_size, characteristics = _unpack_from(
        "<HHIIIHH", data, coff_offset
    )
    optional_offset = coff_offset + 20
    (magic,) = _unpack_from("<H", data, optional_offset)
    if magic == 0x10B:
        pe_format = "PE32"
        data_directory_offset = optional_offset + 96
    elif magic == 0x20B:
        pe_format = "PE32+"
        data_directory_offset = optional_offset + 112
    else:
        raise EvidenceError(f"unsupported PE optional-header magic: 0x{magic:04x}")

    export_rva, export_size = _unpack_from("<II", data, data_directory_offset)
    section_offset = optional_offset + optional_size
    sections: list[dict[str, int | str]] = []
    for index in range(section_count):
        offset = section_offset + index * 40
        raw_name = data[offset : offset + 8]
        if len(raw_name) != 8:
            raise EvidenceError("truncated PE section table")
        name = raw_name.split(b"\0", 1)[0].decode("ascii", errors="replace")
        virtual_size, virtual_address, raw_size, raw_pointer = _unpack_from(
            "<IIII", data, offset + 8
        )
        sections.append(
            {
                "name": name,
                "virtual_size": virtual_size,
                "virtual_address": virtual_address,
                "raw_size": raw_size,
                "raw_pointer": raw_pointer,
            }
        )

    def rva_to_offset(rva: int) -> int:
        for section in sections:
            start = int(section["virtual_address"])
            size = max(int(section["virtual_size"]), int(section["raw_size"]))
            if start <= rva < start + size:
                return int(section["raw_pointer"]) + (rva - start)
        if rva < len(data):
            return rva
        raise EvidenceError(f"PE RVA outside mapped sections: 0x{rva:x}")

    exports: list[str] = []
    if export_rva and export_size:
        export_offset = rva_to_offset(export_rva)
        fields = _unpack_from("<IIHHIIIIIII", data, export_offset)
        number_of_names = int(fields[7])
        address_of_names = int(fields[9])
        if number_of_names > 100_000:
            raise EvidenceError("unreasonable PE export-name count")
        names_offset = rva_to_offset(address_of_names)
        for index in range(number_of_names):
            (name_rva,) = _unpack_from("<I", data, names_offset + index * 4)
            exports.append(_read_c_string(data, rva_to_offset(name_rva)))

    return {
        "schema": "static-pe-inspection/v1",
        "path": str(target),
        "size": len(data),
        "sha256": sha256_file(target),
        "machine": MACHINE_NAMES.get(machine, f"unknown-0x{machine:04x}"),
        "pe_format": pe_format,
        "timestamp_raw": timestamp,
        "characteristics_raw": characteristics,
        "section_count": section_count,
        "exports": sorted(set(exports)),
        "inspection_method": "static-file-bytes-no-loadlibrary",
    }


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: python -m firstech_sequence_mcp.pe <pe-file>", file=sys.stderr)
        return 2
    try:
        result = inspect_pe(Path(sys.argv[1]))
    except Exception as exc:  # CLI evidence wrapper
        print(json.dumps({"status": "error", "error": str(exc)}), file=sys.stderr)
        return 1
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
