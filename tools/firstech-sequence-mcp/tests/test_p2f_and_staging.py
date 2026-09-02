from __future__ import annotations

import json
from pathlib import Path

import pytest

from firstech_sequence_mcp import p2f
from firstech_sequence_mcp.errors import EvidenceError
from firstech_sequence_mcp.p2f import compile_sequence
from firstech_sequence_mcp.staging import stage_simulation_input


def _fake_run_process(argv, *, cwd, timeout_seconds, stdout_path, stderr_path):
    del cwd, timeout_seconds
    source = Path(argv[-1])
    if "-dump" not in argv:
        output = source.with_suffix(".fcode")
    else:
        kind = argv[argv.index("-dump") + 1]
        suffix = {"par": ".par", "asm": ".s", "cpp": ".cpp"}[kind]
        output = source.with_suffix(suffix)
    output.write_text(f"synthetic {output.suffix}\n", encoding="utf-8")
    stdout_path.write_bytes(b"ok")
    stderr_path.write_bytes(b"")
    return {
        "argv": argv,
        "cwd": str(source.parent),
        "pid": 123,
        "exit_code": 0,
        "timed_out": False,
        "duration_seconds": 0.01,
        "stdout": stdout_path.name,
        "stderr": stderr_path.name,
    }


def test_compile_and_freeze_input(settings_factory, monkeypatch) -> None:
    settings = settings_factory()
    project = settings.source_roots[0] / "case"
    project.mkdir()
    source = project / "scan.src"
    source.write_text("void main() {}\n", encoding="utf-8")
    runtime_par = project / "proj.par"
    runtime_par.write_text("gwave = read.gwave\n", encoding="utf-8")
    (project / "read.gwave").write_text("0\n1\n0\n", encoding="utf-8")

    monkeypatch.setattr(p2f, "run_process", _fake_run_process)
    compiled = compile_sequence(settings, str(source))
    assert compiled["status"] == "ok"
    assert len(compiled["manifest_sha256"]) == 64
    compile_manifest = json.loads(Path(compiled["manifest_path"]).read_text(encoding="utf-8"))
    assert set(compile_manifest["artifacts"]) == {"fcode", "par", "asm", "cpp"}

    staged = stage_simulation_input(
        settings,
        compiled["run_id"],
        compiled["manifest_sha256"],
        str(runtime_par),
    )
    assert staged["status"] == "ok"
    stage_manifest = json.loads(Path(staged["manifest_path"]).read_text(encoding="utf-8"))
    staged_names = {Path(item["path"]).name for item in stage_manifest["files"]}
    assert "proj.par" in staged_names
    assert "read.gwave" in staged_names
    assert "scan.fcode" in staged_names


def test_runtime_par_with_absolute_reference_is_blocked(settings_factory, monkeypatch) -> None:
    settings = settings_factory()
    project = settings.source_roots[0] / "case"
    project.mkdir()
    source = project / "scan.src"
    source.write_text("void main() {}\n", encoding="utf-8")
    runtime_par = project / "proj.par"
    runtime_par.write_text(r"gwave=C:\unsafe\read.gwave", encoding="utf-8")
    monkeypatch.setattr(p2f, "run_process", _fake_run_process)
    compiled = compile_sequence(settings, str(source))
    assert compiled["status"] == "ok"
    with pytest.raises(EvidenceError, match="absolute/UNC"):
        stage_simulation_input(
            settings,
            compiled["run_id"],
            compiled["manifest_sha256"],
            str(runtime_par),
        )


def test_source_change_after_compile_requires_recompile(settings_factory, monkeypatch) -> None:
    settings = settings_factory()
    project = settings.source_roots[0] / "case"
    project.mkdir()
    source = project / "scan.src"
    source.write_text("void main() {}\n", encoding="utf-8")
    runtime_par = project / "proj.par"
    runtime_par.write_text("gwave = read.gwave\n", encoding="utf-8")
    (project / "read.gwave").write_text("0\n1\n0\n", encoding="utf-8")

    monkeypatch.setattr(p2f, "run_process", _fake_run_process)
    compiled = compile_sequence(settings, str(source))
    source.write_text("void main() { changed(); }\n", encoding="utf-8")

    with pytest.raises(EvidenceError, match="changed after compile"):
        stage_simulation_input(
            settings,
            compiled["run_id"],
            compiled["manifest_sha256"],
            str(runtime_par),
        )


def test_runtime_par_with_single_root_reference_is_blocked(
    settings_factory, monkeypatch
) -> None:
    settings = settings_factory()
    project = settings.source_roots[0] / "case"
    project.mkdir()
    source = project / "scan.src"
    source.write_text("void main() {}\n", encoding="utf-8")
    runtime_par = project / "proj.par"
    runtime_par.write_text("gwave=/rooted/outside.gwave\n", encoding="utf-8")
    monkeypatch.setattr(p2f, "run_process", _fake_run_process)
    compiled = compile_sequence(settings, str(source))
    with pytest.raises(EvidenceError, match="single-root absolute"):
        stage_simulation_input(
            settings,
            compiled["run_id"],
            compiled["manifest_sha256"],
            str(runtime_par),
        )


def test_tampered_compile_manifest_is_rejected(settings_factory, monkeypatch) -> None:
    settings = settings_factory()
    project = settings.source_roots[0] / "case"
    project.mkdir()
    source = project / "scan.src"
    source.write_text("void main() {}\n", encoding="utf-8")
    runtime_par = project / "proj.par"
    runtime_par.write_text("gwave=read.gwave\n", encoding="utf-8")
    (project / "read.gwave").write_text("0\n1\n0\n", encoding="utf-8")
    monkeypatch.setattr(p2f, "run_process", _fake_run_process)
    compiled = compile_sequence(settings, str(source))
    manifest_path = Path(compiled["manifest_path"])
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["tampered"] = True
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

    with pytest.raises(EvidenceError, match="compile manifest SHA"):
        stage_simulation_input(
            settings,
            compiled["run_id"],
            compiled["manifest_sha256"],
            str(runtime_par),
        )


def test_nonzero_p2f_exit_is_preserved_as_failed_run(settings_factory, monkeypatch) -> None:
    settings = settings_factory()
    project = settings.source_roots[0] / "case"
    project.mkdir()
    source = project / "scan.src"
    source.write_text("void main() {}\n", encoding="utf-8")

    def fail_once(argv, *, cwd, timeout_seconds, stdout_path, stderr_path):
        del cwd, timeout_seconds
        stdout_path.write_bytes(b"")
        stderr_path.write_bytes(b"synthetic failure")
        return {
            "argv": argv,
            "cwd": str(source.parent),
            "pid": 456,
            "exit_code": 9,
            "timed_out": False,
            "duration_seconds": 0.01,
            "stdout": stdout_path.name,
            "stderr": stderr_path.name,
        }

    monkeypatch.setattr(p2f, "run_process", fail_once)
    result = compile_sequence(settings, str(source))
    assert result["status"] == "failed"
    manifest = json.loads(Path(result["manifest_path"]).read_text(encoding="utf-8"))
    assert manifest["actions"][0]["exit_code"] == 9
    assert manifest["status"] == "failed"
