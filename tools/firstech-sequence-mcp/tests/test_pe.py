from __future__ import annotations

import struct
from pathlib import Path

from firstech_sequence_mcp.pe import inspect_pe


def test_static_pe_probe_reads_machine_without_loading(tmp_path: Path) -> None:
    data = bytearray(1024)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    coff = 0x84
    struct.pack_into("<HHIIIHH", data, coff, 0x014C, 1, 0, 0, 0, 224, 0)
    optional = coff + 20
    struct.pack_into("<H", data, optional, 0x10B)
    section = optional + 224
    data[section : section + 8] = b".text\0\0\0"
    struct.pack_into("<IIII", data, section + 8, 0x100, 0x1000, 0x100, 0x200)
    target = tmp_path / "synthetic.dll"
    target.write_bytes(data)

    result = inspect_pe(target)
    assert result["machine"] == "x86"
    assert result["pe_format"] == "PE32"
    assert result["exports"] == []
    assert result["inspection_method"] == "static-file-bytes-no-loadlibrary"
