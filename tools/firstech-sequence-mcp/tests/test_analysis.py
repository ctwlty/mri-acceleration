from __future__ import annotations

import json
import shutil
from pathlib import Path

import pytest

from firstech_sequence_mcp.analysis import (
    analyze_directory,
    analyze_simulation,
    compare_simulations,
    compute_logical_gradient_moment,
    read_channel_slice,
)
from firstech_sequence_mcp.core import records_under
from firstech_sequence_mcp.errors import EvidenceError

FIXTURE = Path(__file__).parent / "fixtures" / "simulation_ok"


def _make_sim_run(settings, run_id: str, scale: float = 1.0) -> Path:
    run_dir = settings.run_root / run_id
    output = run_dir / "output"
    output.mkdir(parents=True)
    for source in FIXTURE.iterdir():
        destination = output / source.name
        text = source.read_text(encoding="utf-8")
        if scale != 1.0 and source.name.lower().startswith("grad"):
            lines = []
            for line in text.splitlines():
                x, value = line.split()
                lines.append(f"{x} {float(value) * scale}")
            text = "\n".join(lines) + "\n"
        destination.write_text(text, encoding="utf-8")
    manifest = {
        "schema": "firstech-simulation-run/v1",
        "kind": "simulation",
        "run_id": run_id,
        "status": "ok",
        "files": records_under(run_dir, exclude_names={"manifest.json"}),
    }
    (run_dir / "manifest.json").write_text(
        json.dumps(manifest), encoding="utf-8"
    )
    return run_dir


def test_fixture_analysis_is_compact_and_logical() -> None:
    result = analyze_directory(FIXTURE)
    assert result["status"] == "ok"
    assert result["inf_log"]["seq_simu_end_count"] == 1
    assert result["inf_log"]["nonzero_error_count"] == 0
    assert result["channels"]["gradp"]["point_count"] == 6
    assert result["physical_calibration"] is False


def test_slice_limit_compare_and_moment(settings_factory) -> None:
    settings = settings_factory()
    base_id = "sim-20260811T000000Z-aaaaaaaaaaaa"
    candidate_id = "sim-20260811T000001Z-bbbbbbbbbbbb"
    _make_sim_run(settings, base_id)
    _make_sim_run(settings, candidate_id, scale=2.0)

    assert analyze_simulation(settings, base_id)["status"] == "ok"
    sliced = read_channel_slice(settings, base_id, "gradp", 1, 3)
    assert sliced["returned_count"] == 3
    with pytest.raises(EvidenceError, match="1..5"):
        read_channel_slice(settings, base_id, "gradp", 0, 6)

    compared = compare_simulations(settings, base_id, candidate_id)
    assert compared["channels"]["gradp"]["max_abs_difference"] == 1.0
    moment = compute_logical_gradient_moment(settings, base_id)
    assert moment["is_physical_k_space"] is False
    assert moment["moments"]["gradp"]["relative_trapezoidal_integral"] == 2.0


def test_nonzero_error_is_failure(tmp_path: Path) -> None:
    target = tmp_path / "result"
    shutil.copytree(FIXTURE, target)
    (target / "inf.log").write_text("ERROR: socket failed\nSeqSimuEnd\n", encoding="utf-8")
    result = analyze_directory(target)
    assert result["status"] == "incomplete_or_failed"
    assert result["inf_log"]["nonzero_error_count"] == 1


def test_nonfinite_and_duplicate_channels_are_rejected(tmp_path: Path) -> None:
    nonfinite = tmp_path / "nonfinite"
    shutil.copytree(FIXTURE, nonfinite)
    with (nonfinite / "gradpData.txt").open("a", encoding="utf-8") as handle:
        handle.write("6 NaN\n")
    result = analyze_directory(nonfinite)
    assert result["status"] == "incomplete_or_failed"
    assert result["invalid_channel_line_count"] == 1

    duplicate = tmp_path / "duplicate"
    shutil.copytree(FIXTURE, duplicate)
    nested = duplicate / "nested"
    nested.mkdir()
    shutil.copy2(duplicate / "gradpData.txt", nested / "gradpData.txt")
    with pytest.raises(EvidenceError, match="multiple candidate"):
        analyze_directory(duplicate)


def test_failed_run_is_forensic_only(settings_factory) -> None:
    settings = settings_factory()
    run_id = "sim-20260811T000002Z-cccccccccccc"
    run_dir = _make_sim_run(settings, run_id)
    manifest_path = run_dir / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["status"] = "failed"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

    result = analyze_simulation(settings, run_id)
    assert result["status"] == "run_failed_artifacts_forensic_only"
    assert result["artifact_analysis_status"] == "ok"
    with pytest.raises(EvidenceError, match="not successful"):
        read_channel_slice(settings, run_id, "gradp", 0, 2)
