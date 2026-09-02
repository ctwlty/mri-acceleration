from __future__ import annotations

import math
import re
from collections.abc import Iterable
from pathlib import Path
from typing import Any

from .config import Settings
from .core import (
    assert_regular_tree,
    file_record,
    read_json,
    resolve_run_dir,
    sha256_file,
    verify_records_exact,
)
from .errors import EvidenceError

CHANNEL_ALIASES = {
    "grads": ("gradsdata.txt", "grads.txt"),
    "gradr": ("grardata.txt", "gradrdata.txt", "gradr.txt"),
    "gradp": ("gradpdata.txt", "gradp.txt"),
    "rx": ("rxdata1.txt", "rxdata.txt", "rx.txt"),
}
REQUIRED_LOGICAL_CHANNELS = frozenset(CHANNEL_ALIASES)
SEQ_END_PATTERN = re.compile(r"\bSeqSimuEnd\b", re.IGNORECASE)
ERROR_PATTERN = re.compile(r"\bERROR\b", re.IGNORECASE)
ZERO_ERROR_PATTERN = re.compile(r"\bERROR\b\s*(?:[:=]|count\s*[:=]?)\s*0\b", re.IGNORECASE)


def _decode_text(path: Path) -> str:
    if path.stat().st_size > 256 * 1024 * 1024:
        raise EvidenceError(f"analysis file exceeds 256 MiB safety limit: {path}")
    data = path.read_bytes()
    for encoding in ("utf-8-sig", "gb18030", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("utf-8", errors="replace")


def _parse_numeric_file(path: Path) -> dict[str, Any]:
    points: list[tuple[float, float]] = []
    invalid_lines = 0
    for line_number, raw_line in enumerate(_decode_text(path).splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith(("#", ";", "//")):
            continue
        tokens = [token for token in re.split(r"[,;\s]+", line) if token]
        try:
            numbers = [float(token) for token in tokens]
        except ValueError:
            invalid_lines += 1
            continue
        if not all(math.isfinite(number) for number in numbers):
            invalid_lines += 1
            continue
        if len(numbers) == 1:
            points.append((float(len(points)), numbers[0]))
        elif len(numbers) >= 2:
            points.append((numbers[0], numbers[-1]))
        else:
            invalid_lines += 1
        if line_number > 20_000_000:
            raise EvidenceError(f"unreasonably large numeric file: {path}")
    if not points:
        raise EvidenceError(f"no numeric points parsed from {path}")
    return {"points": points, "invalid_lines": invalid_lines}


def _find_channel_files(result_dir: Path) -> dict[str, Path]:
    files: dict[str, list[Path]] = {}
    for path in result_dir.rglob("*"):
        if path.is_file():
            files.setdefault(path.name.lower(), []).append(path)
    found: dict[str, Path] = {}
    for channel, aliases in CHANNEL_ALIASES.items():
        matches = [path for alias in aliases for path in files.get(alias, [])]
        if len(matches) > 1:
            raise EvidenceError(f"multiple candidate files for logical channel {channel}")
        if matches:
            found[channel] = matches[0]
    return found


def _find_unique_file(result_dir: Path, name: str) -> Path | None:
    matches = [
        path
        for path in result_dir.rglob("*")
        if path.is_file() and path.name.lower() == name
    ]
    if len(matches) > 1:
        raise EvidenceError(f"multiple {name} files in one result")
    return matches[0] if matches else None


def _summary(path: Path) -> dict[str, Any]:
    parsed = _parse_numeric_file(path)
    points = parsed["points"]
    values = [value for _, value in points]
    nonzero = sum(not math.isclose(value, 0.0, abs_tol=1e-15) for value in values)
    return {
        **file_record(path),
        "point_count": len(points),
        "nonzero_count": nonzero,
        "min": min(values),
        "max": max(values),
        "first_x": points[0][0],
        "last_x": points[-1][0],
        "invalid_line_count": parsed["invalid_lines"],
    }


def analyze_directory(result_dir: Path) -> dict[str, Any]:
    result_dir = result_dir.resolve(strict=True)
    inf_log = _find_unique_file(result_dir, "inf.log")
    if inf_log is None:
        inf_summary = {
            "present": False,
            "seq_simu_end_count": 0,
            "error_mention_count": 0,
            "nonzero_error_count": 0,
        }
    else:
        log_text = _decode_text(inf_log)
        error_lines = [line for line in log_text.splitlines() if ERROR_PATTERN.search(line)]
        nonzero_error_lines = [line for line in error_lines if not ZERO_ERROR_PATTERN.search(line)]
        inf_summary = {
            "present": True,
            **file_record(inf_log),
            "seq_simu_end_count": len(SEQ_END_PATTERN.findall(log_text)),
            "error_mention_count": len(error_lines),
            "nonzero_error_count": len(nonzero_error_lines),
            "nonzero_error_preview": nonzero_error_lines[:20],
        }
    channel_files = _find_channel_files(result_dir)
    channels = {channel: _summary(path) for channel, path in channel_files.items()}
    missing_required_channels = sorted(REQUIRED_LOGICAL_CHANNELS - set(channels))
    invalid_channel_lines = sum(item["invalid_line_count"] for item in channels.values())
    too_short_channels = sorted(
        channel for channel, item in channels.items() if item["point_count"] < 2
    )
    success = (
        inf_summary["present"]
        and inf_summary["seq_simu_end_count"] == 1
        and inf_summary["nonzero_error_count"] == 0
        and not missing_required_channels
        and invalid_channel_lines == 0
        and not too_short_channels
    )
    return {
        "schema": "firstech-simulation-analysis/v1",
        "status": "ok" if success else "incomplete_or_failed",
        "result_dir": str(result_dir),
        "inf_log": inf_summary,
        "channels": channels,
        "required_logical_channels": sorted(REQUIRED_LOGICAL_CHANNELS),
        "missing_required_channels": missing_required_channels,
        "invalid_channel_line_count": invalid_channel_lines,
        "too_short_channels": too_short_channels,
        "parser_rule": "numeric-v1: one column=index,value; two-plus columns=first-x,last-value",
        "coordinate_space": "logical_GradS_GradR_GradP_RX",
        "gradient_units": "relative_or_unknown",
        "time_units": "source_x_or_index_unknown",
        "physical_axis_mapping": "unknown",
        "physical_calibration": False,
        "adc_pointwise_timestamps": "unknown_unless_separately_verified",
    }


def _result_dir_for_run(
    settings: Settings,
    run_id: str,
    *,
    require_success: bool = True,
) -> Path:
    run_dir = resolve_run_dir(settings.run_root, run_id)
    manifest_path = run_dir / "manifest.json"
    manifest = read_json(manifest_path)
    if manifest.get("schema") != "firstech-simulation-run/v1":
        raise EvidenceError("analysis tools accept only a simulation run ID")
    if manifest.get("run_id") != run_id:
        raise EvidenceError("simulation manifest run_id does not match the requested run")
    if require_success and manifest.get("status") != "ok":
        raise EvidenceError(
            "simulation run is not successful; use analyze_simulation for forensics"
        )
    verify_records_exact(
        run_dir,
        manifest.get("files"),
        exclude_names={"manifest.json"},
    )
    output_dir = run_dir / "output"
    if not output_dir.is_dir():
        raise EvidenceError("simulation output directory is missing")
    assert_regular_tree(output_dir)
    return output_dir


def analyze_simulation(settings: Settings, run_id: str) -> dict[str, Any]:
    run_dir = resolve_run_dir(settings.run_root, run_id)
    manifest_path = run_dir / "manifest.json"
    manifest = read_json(manifest_path)
    result = analyze_directory(
        _result_dir_for_run(settings, run_id, require_success=False)
    )
    artifact_status = result["status"]
    run_status = manifest.get("status", "unknown")
    if run_status != "ok":
        result["status"] = "run_failed_artifacts_forensic_only"
    result["run_status"] = run_status
    result["artifact_analysis_status"] = artifact_status
    result["run_id"] = run_id
    result["simulation_manifest_sha256"] = sha256_file(manifest_path)
    return result


def _load_channel(result_dir: Path, channel: str) -> tuple[Path, list[tuple[float, float]]]:
    normalized = channel.strip().lower()
    if normalized not in CHANNEL_ALIASES:
        raise EvidenceError(f"unsupported logical channel: {channel!r}")
    files = _find_channel_files(result_dir)
    if normalized not in files:
        raise EvidenceError(f"channel file is missing: {normalized}")
    path = files[normalized]
    points = _parse_numeric_file(path)["points"]
    return path, points


def read_channel_slice(
    settings: Settings,
    run_id: str,
    channel: str,
    start: int,
    count: int,
) -> dict[str, Any]:
    if start < 0:
        raise EvidenceError("start must be non-negative")
    if count <= 0 or count > settings.max_slice_points:
        raise EvidenceError(f"count must be 1..{settings.max_slice_points}")
    result_dir = _result_dir_for_run(settings, run_id)
    path, points = _load_channel(result_dir, channel)
    selected = points[start : start + count]
    return {
        "schema": "firstech-channel-slice/v1",
        "run_id": run_id,
        "channel": channel.lower(),
        "source": file_record(path),
        "start": start,
        "requested_count": count,
        "returned_count": len(selected),
        "total_count": len(points),
        "points": [{"x": x, "value": value} for x, value in selected],
    }


def _matching_channels(base_dir: Path, candidate_dir: Path) -> Iterable[str]:
    return sorted(set(_find_channel_files(base_dir)) & set(_find_channel_files(candidate_dir)))


def compare_simulations(
    settings: Settings,
    base_run_id: str,
    candidate_run_id: str,
) -> dict[str, Any]:
    base_dir = _result_dir_for_run(settings, base_run_id)
    candidate_dir = _result_dir_for_run(settings, candidate_run_id)
    channels: dict[str, Any] = {}
    for channel in _matching_channels(base_dir, candidate_dir):
        _, base_points = _load_channel(base_dir, channel)
        _, candidate_points = _load_channel(candidate_dir, channel)
        aligned_count = min(len(base_points), len(candidate_points))
        differences = [
            candidate_points[index][1] - base_points[index][1]
            for index in range(aligned_count)
        ]
        channels[channel] = {
            "base_count": len(base_points),
            "candidate_count": len(candidate_points),
            "aligned_count": aligned_count,
            "max_abs_difference": max((abs(value) for value in differences), default=0.0),
            "rmse": math.sqrt(
                sum(value * value for value in differences) / aligned_count
            )
            if aligned_count
            else None,
            "x_alignment_assumption": "index-aligned; physical timing not inferred",
        }
    return {
        "schema": "firstech-simulation-comparison/v1",
        "base_run_id": base_run_id,
        "candidate_run_id": candidate_run_id,
        "channels": channels,
    }


def _trapezoidal_integral(points: list[tuple[float, float]]) -> float:
    return sum(
        (right_x - left_x) * (left_value + right_value) / 2.0
        for (left_x, left_value), (right_x, right_value) in zip(
            points, points[1:], strict=False
        )
    )


def compute_logical_gradient_moment(settings: Settings, run_id: str) -> dict[str, Any]:
    result_dir = _result_dir_for_run(settings, run_id)
    moments: dict[str, Any] = {}
    for channel in ("gradr", "gradp"):
        try:
            path, points = _load_channel(result_dir, channel)
        except EvidenceError:
            moments[channel] = {"status": "missing"}
            continue
        moments[channel] = {
            "status": "ok",
            "source": file_record(path),
            "point_count": len(points),
            "relative_trapezoidal_integral": _trapezoidal_integral(points),
        }
    return {
        "schema": "firstech-logical-gradient-moment/v1",
        "run_id": run_id,
        "moments": moments,
        "coordinate_space": "logical_GradR_GradP",
        "gradient_units": "relative_or_unknown",
        "time_units": "source_x_or_index_unknown",
        "physical_axis_mapping": "unknown",
        "physical_calibration": False,
        "is_physical_k_space": False,
    }
