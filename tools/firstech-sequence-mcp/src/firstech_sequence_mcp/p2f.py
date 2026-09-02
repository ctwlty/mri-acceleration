from __future__ import annotations

import shutil
from pathlib import Path
from typing import Any

from .config import Settings
from .core import (
    copy_tree_frozen,
    create_run_dir,
    decode_preview,
    file_record,
    records_under,
    resolve_allowed_existing,
    run_process,
    sha256_file,
    utc_now,
    write_json_new,
)
from .errors import EvidenceError


def _candidate_names(source: Path) -> dict[str, set[str]]:
    stem = source.stem
    name = source.name
    return {
        "fcode": {f"{stem}.fcode", f"{name}.fcode"},
        "par": {f"{stem}.par", f"{name}.par"},
        "asm": {f"{stem}.s", f"{name}.s"},
        "cpp": {f"{stem}.cpp", f"{name}.cpp"},
    }


def _remove_staged_old_outputs(workspace: Path, staged_source: Path) -> None:
    candidate_names = {
        name.lower()
        for names in _candidate_names(staged_source).values()
        for name in names
    }
    for path in workspace.rglob("*"):
        if path.is_file() and path.name.lower() in candidate_names:
            path.unlink()


def _find_expected_artifact(workspace: Path, staged_source: Path, kind: str) -> Path:
    names = {name.lower() for name in _candidate_names(staged_source)[kind]}
    matches = [
        path
        for path in workspace.rglob("*")
        if path.is_file() and path.name.lower() in names
    ]
    if len(matches) != 1:
        raise EvidenceError(
            f"expected exactly one {kind} artifact for {staged_source.name}, found {len(matches)}"
        )
    return matches[0]


def _finalize(run_dir: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    manifest["files"] = records_under(run_dir, exclude_names={"manifest.json"})
    manifest_path = run_dir / "manifest.json"
    write_json_new(manifest_path, manifest)
    return {
        "schema": "firstech-operation-result/v1",
        "status": manifest["status"],
        "run_id": manifest["run_id"],
        "manifest_path": str(manifest_path),
        "manifest_sha256": sha256_file(manifest_path),
        "artifacts": manifest.get("artifacts", {}),
        "error": manifest.get("error"),
    }


def compile_sequence(settings: Settings, source_path: str) -> dict[str, Any]:
    source = resolve_allowed_existing(source_path, settings.source_roots, expected="file")
    if source.suffix.lower() != ".src":
        raise EvidenceError("compile_sequence accepts only a .src file")
    if not settings.p2f.exe.is_file():
        raise EvidenceError(f"configured P2F does not exist: {settings.p2f.exe}")
    if not settings.p2f.include_dir.is_dir():
        raise EvidenceError(
            f"configured include directory does not exist: {settings.p2f.include_dir}"
        )
    if settings.p2f.system_sel is not None and not settings.p2f.system_sel.is_file():
        raise EvidenceError(f"configured systemSel does not exist: {settings.p2f.system_sel}")

    run_id, run_dir = create_run_dir(settings.run_root, "compile")
    manifest: dict[str, Any] = {
        "schema": "firstech-compile-run/v1",
        "kind": "compile",
        "run_id": run_id,
        "created_at": utc_now(),
        "status": "running",
        "mode": "offline-only",
        "source_original": file_record(source),
        "p2f": file_record(settings.p2f.exe),
        "actions": [],
        "artifacts": {},
        "limitations": [
            "compile evidence only; no simulation or physical trajectory claim",
            "systemSel is snapshotted for identity; this CLI shape does not prove it was consumed",
        ],
    }
    try:
        input_dir = run_dir / "input"
        workspace = input_dir / "workspace"
        include_snapshot = input_dir / "include"
        source_stats = copy_tree_frozen(
            source.parent,
            workspace,
            max_files=settings.max_stage_files,
            max_bytes=settings.max_stage_bytes,
        )
        include_stats = copy_tree_frozen(
            settings.p2f.include_dir,
            include_snapshot,
            max_files=settings.max_stage_files,
            max_bytes=settings.max_stage_bytes,
        )
        staged_source = workspace / source.name
        if not staged_source.is_file():
            raise EvidenceError("staged source is missing")
        _remove_staged_old_outputs(workspace, staged_source)
        manifest["staging"] = {
            "source_workspace": source_stats,
            "include_snapshot": include_stats,
            "staged_source": file_record(staged_source, relative_to=run_dir),
        }
        if settings.p2f.system_sel is not None:
            system_destination = input_dir / "system" / settings.p2f.system_sel.name
            system_destination.parent.mkdir(parents=True)
            shutil.copy2(settings.p2f.system_sel, system_destination)
            manifest["staging"]["system_sel"] = file_record(
                system_destination, relative_to=run_dir
            )

        commands = [
            ("compile", []),
            ("dump_par", ["-dump", "par"]),
            ("dump_asm", ["-dump", "asm"]),
            ("dump_cpp", ["-dump", "cpp"]),
        ]
        for index, (name, extra_args) in enumerate(commands, start=1):
            argv = [
                str(settings.p2f.exe),
                "-i",
                str(include_snapshot),
                *extra_args,
                str(staged_source),
            ]
            stdout_path = run_dir / f"{index:02d}-{name}.stdout.bin"
            stderr_path = run_dir / f"{index:02d}-{name}.stderr.bin"
            action = run_process(
                argv,
                cwd=workspace,
                timeout_seconds=settings.p2f.timeout_seconds,
                stdout_path=stdout_path,
                stderr_path=stderr_path,
            )
            action["name"] = name
            action["stdout_preview"] = decode_preview(stdout_path)
            action["stderr_preview"] = decode_preview(stderr_path)
            manifest["actions"].append(action)
            if action["timed_out"] or action["exit_code"] != 0:
                raise EvidenceError(
                    f"P2F {name} failed: exit={action['exit_code']} timeout={action['timed_out']}"
                )

        artifacts = {
            kind: file_record(
                _find_expected_artifact(workspace, staged_source, kind), relative_to=run_dir
            )
            for kind in ("fcode", "par", "asm", "cpp")
        }
        manifest["artifacts"] = artifacts
        manifest["status"] = "ok"
    except Exception as exc:
        manifest["status"] = "failed"
        manifest["error"] = f"{type(exc).__name__}: {exc}"
    return _finalize(run_dir, manifest)
