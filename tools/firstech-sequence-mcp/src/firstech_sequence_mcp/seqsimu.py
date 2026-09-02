from __future__ import annotations

import re
from pathlib import Path
from typing import Any

from .config import Settings
from .core import (
    read_json,
    resolve_run_dir,
    sha256_file,
    verify_records_exact,
)
from .errors import EvidenceError, SafetyBlocked

SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")


def probe_simulation_backend(settings: Settings) -> dict[str, Any]:
    del settings
    return {
        "schema": "firstech-simulation-backend-probe/v1",
        "status": "blocked",
        "reason": (
            "V1 dynamic simbridge is intentionally unreleased pending Windows target, "
            "ABI, network-isolation, and process-containment verification"
        ),
        "dynamic_process_started": False,
    }


def _load_stage(
    settings: Settings,
    simulation_input_id: str,
    expected_manifest_sha256: str,
) -> tuple[Path, dict[str, Any]]:
    normalized_sha = expected_manifest_sha256.lower()
    if not SHA256_PATTERN.fullmatch(normalized_sha):
        raise EvidenceError("expected_manifest_sha256 must be 64 lowercase hex characters")
    stage_dir = resolve_run_dir(settings.run_root, simulation_input_id)
    manifest_path = stage_dir / "manifest.json"
    if sha256_file(manifest_path) != normalized_sha:
        raise EvidenceError("staged simulation-input manifest SHA does not match")
    manifest = read_json(manifest_path)
    if manifest.get("schema") != "firstech-simulation-input/v1":
        raise EvidenceError("simulation_input_id does not point to a staged simulation input")
    if manifest.get("run_id") != simulation_input_id:
        raise EvidenceError("simulation-input manifest run_id does not match the requested run")
    if manifest.get("status") != "ok":
        raise EvidenceError("staged simulation input is not successful")
    verify_records_exact(
        stage_dir,
        manifest.get("files"),
        exclude_names={"manifest.json"},
    )
    compile_run_id = manifest.get("compile_run_id")
    if not isinstance(compile_run_id, str):
        raise EvidenceError("simulation-input compile_run_id is missing")
    compile_dir = resolve_run_dir(settings.run_root, compile_run_id)
    if sha256_file(compile_dir / "manifest.json") != manifest.get("compile_manifest_sha256"):
        raise EvidenceError("referenced compile manifest changed after staging")
    return stage_dir, manifest


def simulate_sequence(
    settings: Settings,
    simulation_input_id: str,
    expected_manifest_sha256: str,
) -> dict[str, Any]:
    _load_stage(settings, simulation_input_id, expected_manifest_sha256)
    raise SafetyBlocked(
        "simulate_sequence remains blocked in V1; implement and target-verify the isolated "
        "Windows simbridge contract before any dynamic DLL load"
    )
