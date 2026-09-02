from __future__ import annotations

from pathlib import Path

import pytest

from firstech_sequence_mcp.config import Settings
from firstech_sequence_mcp.errors import ConfigurationError


def _write_config(path: Path, root: Path, *, mode: str = "offline-only") -> None:
    text = f'''
schema = "firstech-sequence-mcp-config/v1"
mode = "{mode}"
source_roots = ["{(root / 'source').as_posix()}"]
run_root = "{(root / 'runs').as_posix()}"
vendor_install_roots = ["{(root / 'vendor').as_posix()}"]

[p2f]
exe = "{(root / 'vendor/bin/P2F_x32.exe').as_posix()}"
include_dir = "{(root / 'vendor/include').as_posix()}"
system_sel = ""
timeout_seconds = 10

[inspection]
current_dll = ""
optional_binaries = []
forbidden_process_names = ["SpectrometerIDE"]
spectrometer_targets = ["192.0.2.10:8701"]

[simulation]
backend = "blocked"
bridge_exe = ""
gate_file = ""
timeout_seconds = 10
'''
    path.write_text(text, encoding="utf-8")


def test_load_offline_config(tmp_path: Path) -> None:
    config = tmp_path / "config.toml"
    _write_config(config, tmp_path)
    settings = Settings.load(config)
    assert settings.mode == "offline-only"
    assert settings.simulation.backend == "blocked"


def test_rejects_non_offline_mode(tmp_path: Path) -> None:
    config = tmp_path / "config.toml"
    _write_config(config, tmp_path, mode="device")
    with pytest.raises(ConfigurationError, match="offline-only"):
        Settings.load(config)


def test_rejects_run_root_inside_vendor(tmp_path: Path) -> None:
    config = tmp_path / "config.toml"
    _write_config(config, tmp_path)
    content = config.read_text(encoding="utf-8")
    content = content.replace(
        f'run_root = "{(tmp_path / "runs").as_posix()}"',
        f'run_root = "{(tmp_path / "vendor/runs").as_posix()}"',
    )
    config.write_text(content, encoding="utf-8")
    with pytest.raises(ConfigurationError, match="vendor"):
        Settings.load(config)


def test_rejects_run_root_inside_source(tmp_path: Path) -> None:
    config = tmp_path / "config.toml"
    _write_config(config, tmp_path)
    content = config.read_text(encoding="utf-8")
    content = content.replace(
        f'run_root = "{(tmp_path / "runs").as_posix()}"',
        f'run_root = "{(tmp_path / "source/runs").as_posix()}"',
    )
    config.write_text(content, encoding="utf-8")
    with pytest.raises(ConfigurationError, match="source_roots"):
        Settings.load(config)


def test_rejects_arbitrary_vendor_executable(tmp_path: Path) -> None:
    config = tmp_path / "config.toml"
    _write_config(config, tmp_path)
    content = config.read_text(encoding="utf-8").replace(
        "vendor/bin/P2F_x32.exe", "vendor/bin/SpectrometerIDE.exe"
    )
    config.write_text(content, encoding="utf-8")
    with pytest.raises(ConfigurationError, match="P2F_x32.exe"):
        Settings.load(config)


def test_rejects_inspection_binary_outside_vendor(tmp_path: Path) -> None:
    config = tmp_path / "config.toml"
    _write_config(config, tmp_path)
    content = config.read_text(encoding="utf-8").replace(
        'current_dll = ""',
        f'current_dll = "{(tmp_path / "outside/mridll.dll").as_posix()}"',
    )
    config.write_text(content, encoding="utf-8")
    with pytest.raises(ConfigurationError, match="inspection binaries"):
        Settings.load(config)


def test_rejects_source_root_symlink(tmp_path: Path) -> None:
    real_source = tmp_path / "real-source"
    real_source.mkdir()
    source_link = tmp_path / "source-link"
    try:
        source_link.symlink_to(real_source, target_is_directory=True)
    except OSError:
        pytest.skip("symlink creation unavailable")
    config = tmp_path / "config.toml"
    _write_config(config, tmp_path)
    content = config.read_text(encoding="utf-8").replace(
        f'"{(tmp_path / "source").as_posix()}"',
        f'"{source_link.as_posix()}"',
    )
    config.write_text(content, encoding="utf-8")
    with pytest.raises(ConfigurationError, match="reparse"):
        Settings.load(config)
