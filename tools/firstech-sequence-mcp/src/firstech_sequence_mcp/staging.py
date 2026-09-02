from __future__ import annotations

import re
from pathlib import Path
from typing import Any

from .config import Settings
from .core import (
    copy_file_exclusive,
    copy_tree_frozen,
    create_run_dir,
    file_record,
    read_json,
    records_under,
    resolve_allowed_existing,
    resolve_run_dir,
    sha256_file,
    utc_now,
    verify_records_exact,
    write_json_new,
)
from .errors import EvidenceError

ABSOLUTE_REFERENCE = re.compile(r"(?i)(?:[A-Z]:[\\/]|\\\\[^\\\s]+[\\])")
SINGLE_ROOT_REFERENCE = re.compile(
    r'''(?m)(?:^|[=,:;\s"'(])\s*(?P<path>[\\/](?![\\/])[^\s,;"')]+)'''
)
PARENT_REFERENCE = re.compile(r"\.\.[\\/]")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")


def _decode_text_file(path: Path) -> str:
    data = path.read_bytes()
    for encoding in ("utf-8-sig", "gb18030", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    raise EvidenceError(f"cannot decode runtime PAR: {path}")


def _validate_relative_runtime_references(runtime_par: Path) -> None:
    text = _decode_text_file(runtime_par)
    match = ABSOLUTE_REFERENCE.search(text)
    if match:
        raise EvidenceError(
            "runtime PAR contains an absolute/UNC reference; prepare a relative-path "
            "shadow workspace "
            f"before staging (first match: {match.group(0)!r})"
        )
    rooted_match = SINGLE_ROOT_REFERENCE.search(text)
    if rooted_match:
        raise EvidenceError(
            "runtime PAR contains a single-root absolute reference; all dependencies must "
            "use relative paths inside the frozen workspace "
            f"(first match: {rooted_match.group('path')!r})"
        )
    parent_match = PARENT_REFERENCE.search(text)
    if parent_match:
        raise EvidenceError(
            "runtime PAR contains a parent-directory reference; all dependencies must live "
            "inside the frozen workspace"
        )


def stage_simulation_input(
    settings: Settings,
    compile_run_id: str,
    expected_compile_manifest_sha256: str,
    runtime_par_path: str,
) -> dict[str, Any]:
    compile_dir = resolve_run_dir(settings.run_root, compile_run_id)
    compile_manifest_path = compile_dir / "manifest.json"
    normalized_compile_sha = expected_compile_manifest_sha256.lower()
    if not SHA256_PATTERN.fullmatch(normalized_compile_sha):
        raise EvidenceError(
            "expected_compile_manifest_sha256 must be 64 lowercase hex characters"
        )
    if sha256_file(compile_manifest_path) != normalized_compile_sha:
        raise EvidenceError("compile manifest SHA does not match the retained compile result")
    compile_manifest = read_json(compile_manifest_path)
    if compile_manifest.get("schema") != "firstech-compile-run/v1":
        raise EvidenceError("compile_run_id does not point to a compile run")
    if compile_manifest.get("run_id") != compile_run_id:
        raise EvidenceError("compile manifest run_id does not match the requested run")
    if compile_manifest.get("status") != "ok":
        raise EvidenceError("compile run is not successful")
    verify_records_exact(
        compile_dir,
        compile_manifest.get("files"),
        exclude_names={"manifest.json"},
    )

    runtime_par = resolve_allowed_existing(
        runtime_par_path, settings.source_roots, expected="file"
    )
    if runtime_par.suffix.lower() != ".par":
        raise EvidenceError("runtime_par_path must be a .par file")
    source_original = compile_manifest.get("source_original")
    if not isinstance(source_original, dict) or not isinstance(source_original.get("path"), str):
        raise EvidenceError("compile manifest source identity is missing")
    compile_staging = compile_manifest.get("staging")
    staged_source_record = (
        compile_staging.get("staged_source") if isinstance(compile_staging, dict) else None
    )
    if not isinstance(staged_source_record, dict) or not isinstance(
        staged_source_record.get("path"), str
    ):
        raise EvidenceError("compile manifest staged-source identity is missing")
    if (
        staged_source_record.get("size") != source_original.get("size")
        or staged_source_record.get("sha256") != source_original.get("sha256")
    ):
        raise EvidenceError("compiled staged source does not match the original source identity")
    staged_source = (compile_dir / staged_source_record["path"]).resolve(strict=True)
    staged_source.relative_to(compile_dir.resolve(strict=True))
    if (
        staged_source.stat().st_size != staged_source_record.get("size")
        or sha256_file(staged_source) != staged_source_record.get("sha256")
    ):
        raise EvidenceError("compiled staged source changed during or after compile")
    compile_source_parent = Path(source_original["path"]).resolve(strict=False).parent
    if runtime_par.parent.resolve(strict=True) != compile_source_parent:
        raise EvidenceError(
            "runtime PAR must be prepared beside the compiled source in the same shadow workspace"
        )
    current_source = compile_source_parent / Path(source_original["path"]).name
    if not current_source.is_file():
        raise EvidenceError("compiled source is no longer present in the runtime workspace")
    if (
        current_source.stat().st_size != source_original.get("size")
        or sha256_file(current_source) != source_original.get("sha256")
    ):
        raise EvidenceError("compiled source changed after compile; recompile before staging")
    _validate_relative_runtime_references(runtime_par)

    run_id, run_dir = create_run_dir(settings.run_root, "stage")
    manifest: dict[str, Any] = {
        "schema": "firstech-simulation-input/v1",
        "kind": "simulation-input",
        "run_id": run_id,
        "created_at": utc_now(),
        "status": "running",
        "compile_run_id": compile_run_id,
        "compile_manifest_sha256": normalized_compile_sha,
        "runtime_par_original": file_record(runtime_par),
        "limitations": [
            "relative-path workspace freeze; semantic PAR dependency parsing remains "
            "target-verified work"
        ],
    }
    try:
        workspace = run_dir / "workspace"
        stats = copy_tree_frozen(
            runtime_par.parent,
            workspace,
            max_files=settings.max_stage_files,
            max_bytes=settings.max_stage_bytes,
        )
        staged_par = workspace / runtime_par.name
        if not staged_par.is_file():
            raise EvidenceError("runtime PAR was not copied into the frozen workspace")

        copied_artifacts: dict[str, dict[str, Any]] = {}
        artifacts = compile_manifest.get("artifacts")
        if not isinstance(artifacts, dict):
            raise EvidenceError("compile manifest has no artifact map")
        for kind in ("fcode", "par", "asm", "cpp"):
            record = artifacts.get(kind)
            if not isinstance(record, dict) or not isinstance(record.get("path"), str):
                raise EvidenceError(f"compile artifact missing: {kind}")
            source_artifact = (compile_dir / record["path"]).resolve(strict=True)
            source_artifact.relative_to(compile_dir.resolve(strict=True))
            if sha256_file(source_artifact) != record.get("sha256"):
                raise EvidenceError(f"compile artifact SHA mismatch: {kind}")
            destination = workspace / source_artifact.name
            copy_file_exclusive(source_artifact, destination)
            copied_artifacts[kind] = file_record(destination, relative_to=run_dir)

        manifest["workspace"] = stats
        manifest["runtime_par_staged"] = file_record(staged_par, relative_to=run_dir)
        manifest["compile_artifacts_staged"] = copied_artifacts
        manifest["status"] = "ok"
    except Exception as exc:
        manifest["status"] = "failed"
        manifest["error"] = f"{type(exc).__name__}: {exc}"
    manifest["files"] = records_under(run_dir, exclude_names={"manifest.json"})
    manifest_path = run_dir / "manifest.json"
    write_json_new(manifest_path, manifest)
    return {
        "schema": "firstech-operation-result/v1",
        "status": manifest["status"],
        "run_id": run_id,
        "manifest_path": str(manifest_path),
        "manifest_sha256": sha256_file(manifest_path),
        "runtime_par": manifest.get("runtime_par_staged"),
        "error": manifest.get("error"),
    }
