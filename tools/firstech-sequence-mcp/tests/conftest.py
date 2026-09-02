from __future__ import annotations

from pathlib import Path

import pytest

from firstech_sequence_mcp.config import (
    InspectionConfig,
    P2FConfig,
    Settings,
    SimulationConfig,
)


@pytest.fixture
def settings_factory(tmp_path: Path):
    def build() -> Settings:
        source_root = tmp_path / "sources"
        run_root = tmp_path / "runs"
        vendor_root = tmp_path / "vendor"
        include_dir = vendor_root / "include"
        bin_dir = vendor_root / "bin"
        source_root.mkdir(exist_ok=True)
        include_dir.mkdir(parents=True, exist_ok=True)
        bin_dir.mkdir(parents=True, exist_ok=True)
        p2f_exe = bin_dir / "P2F_x32.exe"
        if not p2f_exe.exists():
            p2f_exe.write_bytes(b"synthetic-p2f-identity")
        header = include_dir / "base.h"
        if not header.exists():
            header.write_text("// synthetic include\n", encoding="utf-8")
        return Settings(
            config_path=tmp_path / "config.toml",
            mode="offline-only",
            source_roots=(source_root,),
            run_root=run_root,
            vendor_install_roots=(vendor_root,),
            max_stage_files=100,
            max_stage_bytes=1024 * 1024,
            max_slice_points=5,
            p2f=P2FConfig(
                exe=p2f_exe,
                include_dir=include_dir,
                system_sel=None,
                timeout_seconds=10,
            ),
            inspection=InspectionConfig(
                current_dll=None,
                optional_binaries=(),
                forbidden_process_names=("SpectrometerIDE",),
                spectrometer_targets=("192.0.2.10:8701",),
            ),
            simulation=SimulationConfig(
                backend="blocked",
                bridge_exe=None,
                gate_file=None,
                timeout_seconds=10,
            ),
        )

    return build
