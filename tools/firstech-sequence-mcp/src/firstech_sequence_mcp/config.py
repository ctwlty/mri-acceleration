from __future__ import annotations

import ipaddress
import os
import stat
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .errors import ConfigurationError


def _required_path(value: Any, field: str) -> Path:
    if not isinstance(value, str) or not value.strip():
        raise ConfigurationError(f"{field} must be a non-empty absolute path")
    if value.replace("/", "\\").startswith("\\\\"):
        raise ConfigurationError(f"{field} may not be a UNC path")
    path = Path(value).expanduser()
    if not path.is_absolute():
        raise ConfigurationError(f"{field} must be absolute: {value!r}")
    return path


def _optional_path(value: Any, field: str) -> Path | None:
    if value in (None, ""):
        return None
    return _required_path(value, field)


def _path_list(value: Any, field: str, *, allow_empty: bool = False) -> tuple[Path, ...]:
    if not isinstance(value, list) or (not value and not allow_empty):
        raise ConfigurationError(f"{field} must be a non-empty list of absolute paths")
    return tuple(_required_path(item, f"{field}[]") for item in value)


def _string_list(value: Any, field: str) -> tuple[str, ...]:
    if value is None:
        return ()
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise ConfigurationError(f"{field} must be a list of strings")
    return tuple(item.strip() for item in value if item.strip())


def _is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def _has_existing_reparse_component(path: Path) -> bool:
    current = path
    while not current.exists() and current.parent != current:
        current = current.parent
    while True:
        try:
            info = os.lstat(current)
        except OSError:
            return True
        attributes = getattr(info, "st_file_attributes", 0)
        reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
        if stat.S_ISLNK(info.st_mode) or attributes & reparse_flag:
            return True
        if current.parent == current:
            return False
        current = current.parent


@dataclass(frozen=True)
class P2FConfig:
    exe: Path
    include_dir: Path
    system_sel: Path | None
    timeout_seconds: int


@dataclass(frozen=True)
class InspectionConfig:
    current_dll: Path | None
    optional_binaries: tuple[Path, ...]
    forbidden_process_names: tuple[str, ...]
    spectrometer_targets: tuple[str, ...]


@dataclass(frozen=True)
class SimulationConfig:
    backend: str
    bridge_exe: Path | None
    gate_file: Path | None
    timeout_seconds: int


@dataclass(frozen=True)
class Settings:
    config_path: Path
    mode: str
    source_roots: tuple[Path, ...]
    run_root: Path
    vendor_install_roots: tuple[Path, ...]
    max_stage_files: int
    max_stage_bytes: int
    max_slice_points: int
    p2f: P2FConfig
    inspection: InspectionConfig
    simulation: SimulationConfig

    @classmethod
    def load(cls, path: str | Path | None = None) -> Settings:
        selected = path or os.environ.get("FIRSTECH_MCP_CONFIG")
        if not selected:
            raise ConfigurationError(
                "FIRSTECH_MCP_CONFIG is not set; copy config.example.toml to a private path"
            )
        config_path = Path(selected).expanduser()
        if not config_path.is_absolute():
            raise ConfigurationError("FIRSTECH_MCP_CONFIG must be an absolute path")
        try:
            raw = tomllib.loads(config_path.read_text(encoding="utf-8"))
        except (OSError, tomllib.TOMLDecodeError) as exc:
            raise ConfigurationError(f"cannot read config {config_path}: {exc}") from exc

        if raw.get("schema") != "firstech-sequence-mcp-config/v1":
            raise ConfigurationError("unsupported or missing config schema")

        p2f_raw = raw.get("p2f")
        inspection_raw = raw.get("inspection")
        simulation_raw = raw.get("simulation")
        if not all(isinstance(item, dict) for item in (p2f_raw, inspection_raw, simulation_raw)):
            raise ConfigurationError("p2f, inspection, and simulation tables are required")

        settings = cls(
            config_path=config_path,
            mode=str(raw.get("mode", "")),
            source_roots=_path_list(raw.get("source_roots"), "source_roots"),
            run_root=_required_path(raw.get("run_root"), "run_root"),
            vendor_install_roots=_path_list(
                raw.get("vendor_install_roots", []),
                "vendor_install_roots",
                allow_empty=True,
            ),
            max_stage_files=int(raw.get("max_stage_files", 5000)),
            max_stage_bytes=int(raw.get("max_stage_bytes", 512 * 1024 * 1024)),
            max_slice_points=int(raw.get("max_slice_points", 500)),
            p2f=P2FConfig(
                exe=_required_path(p2f_raw.get("exe"), "p2f.exe"),
                include_dir=_required_path(p2f_raw.get("include_dir"), "p2f.include_dir"),
                system_sel=_optional_path(p2f_raw.get("system_sel"), "p2f.system_sel"),
                timeout_seconds=int(p2f_raw.get("timeout_seconds", 180)),
            ),
            inspection=InspectionConfig(
                current_dll=_optional_path(
                    inspection_raw.get("current_dll"), "inspection.current_dll"
                ),
                optional_binaries=_path_list(
                    inspection_raw.get("optional_binaries", []),
                    "inspection.optional_binaries",
                    allow_empty=True,
                ),
                forbidden_process_names=_string_list(
                    inspection_raw.get("forbidden_process_names"),
                    "inspection.forbidden_process_names",
                ),
                spectrometer_targets=_string_list(
                    inspection_raw.get("spectrometer_targets"),
                    "inspection.spectrometer_targets",
                ),
            ),
            simulation=SimulationConfig(
                backend=str(simulation_raw.get("backend", "blocked")),
                bridge_exe=_optional_path(
                    simulation_raw.get("bridge_exe"), "simulation.bridge_exe"
                ),
                gate_file=_optional_path(
                    simulation_raw.get("gate_file"), "simulation.gate_file"
                ),
                timeout_seconds=int(simulation_raw.get("timeout_seconds", 900)),
            ),
        )
        settings.validate()
        return settings

    def validate(self) -> None:
        if self.mode != "offline-only":
            raise ConfigurationError("mode must be exactly 'offline-only'")
        if self.simulation.backend != "blocked":
            raise ConfigurationError(
                "V1 ships with the dynamic command backend unreleased; "
                "backend must remain 'blocked'"
            )
        for target in self.inspection.spectrometer_targets:
            host, separator, port_text = target.rpartition(":")
            try:
                ipaddress.ip_address(host.strip("[]"))
                port = int(port_text)
            except (ValueError, TypeError) as exc:
                raise ConfigurationError(
                    f"spectrometer target must be a literal IP:port: {target!r}"
                ) from exc
            if not separator or not 1 <= port <= 65535:
                raise ConfigurationError(f"invalid spectrometer target: {target!r}")
        positive_values = {
            "max_stage_files": self.max_stage_files,
            "max_stage_bytes": self.max_stage_bytes,
            "max_slice_points": self.max_slice_points,
            "p2f.timeout_seconds": self.p2f.timeout_seconds,
            "simulation.timeout_seconds": self.simulation.timeout_seconds,
        }
        for name, value in positive_values.items():
            if value <= 0:
                raise ConfigurationError(f"{name} must be positive")

        if not self.vendor_install_roots:
            raise ConfigurationError("vendor_install_roots must identify the current installation")
        for root in (*self.source_roots, self.run_root, *self.vendor_install_roots):
            if _has_existing_reparse_component(root):
                raise ConfigurationError(f"configured roots may not cross a reparse point: {root}")
        source_roots = tuple(path.resolve(strict=False) for path in self.source_roots)
        run_root = self.run_root.resolve(strict=False)
        vendor_roots = tuple(path.resolve(strict=False) for path in self.vendor_install_roots)
        if run_root.parent == run_root:
            raise ConfigurationError("run_root may not be a filesystem root")
        for source_root in source_roots:
            if source_root.parent == source_root:
                raise ConfigurationError("source_roots may not contain a filesystem root")
            if any(
                _is_within(source_root, vendor_root) or _is_within(vendor_root, source_root)
                for vendor_root in vendor_roots
            ):
                raise ConfigurationError("source_roots and vendor install roots must not overlap")
            if _is_within(run_root, source_root) or _is_within(source_root, run_root):
                raise ConfigurationError("run_root and source_roots must not overlap")
        if any(
            _is_within(run_root, vendor_root) or _is_within(vendor_root, run_root)
            for vendor_root in vendor_roots
        ):
            raise ConfigurationError("run_root and vendor install roots must not overlap")
        for index, source_root in enumerate(source_roots):
            for other in source_roots[index + 1 :]:
                if _is_within(source_root, other) or _is_within(other, source_root):
                    raise ConfigurationError("source_roots must not overlap each other")

        p2f_exe = self.p2f.exe.resolve(strict=False)
        include_dir = self.p2f.include_dir.resolve(strict=False)
        configured_vendor_files = tuple(
            path
            for path in (
                self.p2f.exe,
                self.p2f.system_sel,
                self.inspection.current_dll,
                *self.inspection.optional_binaries,
            )
            if path is not None
        )
        for path in (*configured_vendor_files, self.p2f.include_dir):
            if _has_existing_reparse_component(path):
                raise ConfigurationError(
                    f"configured tool paths may not cross a reparse point: {path}"
                )
        if self.p2f.exe.name.lower() != "p2f_x32.exe":
            raise ConfigurationError("p2f.exe must be the fixed P2F_x32.exe")
        if not any(_is_within(p2f_exe, root) for root in vendor_roots):
            raise ConfigurationError("P2F_x32.exe must be inside a vendor install root")
        if not any(_is_within(include_dir, root) for root in vendor_roots):
            raise ConfigurationError("p2f.include_dir must be inside a vendor install root")
        if self.p2f.system_sel is not None:
            system_sel = self.p2f.system_sel.resolve(strict=False)
            if not any(_is_within(system_sel, root) for root in vendor_roots):
                raise ConfigurationError("p2f.system_sel must be inside a vendor install root")
        for path in (
            self.inspection.current_dll,
            *self.inspection.optional_binaries,
        ):
            if path is None:
                continue
            resolved = path.resolve(strict=False)
            if not any(_is_within(resolved, root) for root in vendor_roots):
                raise ConfigurationError(
                    "inspection binaries must be inside a vendor install root"
                )
