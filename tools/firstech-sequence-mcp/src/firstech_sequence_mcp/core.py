from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import stat
import subprocess
import time
import uuid
from collections.abc import Iterable
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from .errors import EvidenceError, PathPolicyError

SKIP_DIRECTORY_NAMES = {
    ".git",
    ".mypy_cache",
    ".pytest_cache",
    ".ruff_cache",
    ".venv",
    "__pycache__",
    "node_modules",
    "runs",
}
FORBIDDEN_STAGE_SUFFIXES = {
    ".7z",
    ".bz2",
    ".dcm",
    ".dll",
    ".dylib",
    ".exe",
    ".gz",
    ".img",
    ".iso",
    ".lib",
    ".msi",
    ".nii",
    ".nrrd",
    ".pdb",
    ".pyc",
    ".pyd",
    ".rar",
    ".raw",
    ".so",
    ".tar",
    ".whl",
    ".xz",
    ".zip",
}
RUN_ID_PATTERN = re.compile(r"^(?:compile|stage|sim)-\d{8}T\d{6}Z-[0-9a-f]{12}$")


def utc_now() -> str:
    return datetime.now(UTC).isoformat()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def file_record(path: Path, *, relative_to: Path | None = None) -> dict[str, Any]:
    target = path.resolve(strict=True)
    shown = target.relative_to(relative_to.resolve(strict=True)) if relative_to else target
    return {
        "path": shown.as_posix(),
        "size": target.stat().st_size,
        "sha256": sha256_file(target),
    }


def write_json_new(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8", newline="\n") as handle:
        json.dump(payload, handle, ensure_ascii=False, indent=2, sort_keys=True)
        handle.write("\n")


def read_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise EvidenceError(f"cannot read JSON evidence {path}: {exc}") from exc


def create_run_dir(run_root: Path, prefix: str) -> tuple[str, Path]:
    run_root.mkdir(parents=True, exist_ok=True)
    run_id = f"{prefix}-{datetime.now(UTC).strftime('%Y%m%dT%H%M%SZ')}-{uuid.uuid4().hex[:12]}"
    run_dir = run_root / run_id
    run_dir.mkdir(exist_ok=False)
    return run_id, run_dir


def resolve_run_dir(run_root: Path, run_id: str) -> Path:
    if not RUN_ID_PATTERN.fullmatch(run_id):
        raise PathPolicyError("run_id does not match a server-generated compile/stage/sim ID")
    root = run_root.resolve(strict=True)
    candidate = (root / run_id).resolve(strict=True)
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise PathPolicyError("run_id escapes run_root") from exc
    if not candidate.is_dir():
        raise EvidenceError(f"run does not exist: {run_id}")
    return candidate


def _is_unc_text(value: str) -> bool:
    normalized = value.replace("/", "\\")
    return normalized.startswith("\\\\")


def _has_reparse_component(path: Path, stop_at: Path) -> bool:
    current = path
    stop = stop_at.resolve(strict=True)
    while True:
        try:
            info = os.lstat(current)
        except OSError:
            return True
        attributes = getattr(info, "st_file_attributes", 0)
        reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
        if stat.S_ISLNK(info.st_mode) or attributes & reparse_flag:
            return True
        if current == stop:
            return False
        if current.parent == current:
            return True
        current = current.parent


def resolve_allowed_existing(
    value: str | Path,
    roots: Iterable[Path],
    *,
    expected: str = "file",
) -> Path:
    raw = str(value)
    if _is_unc_text(raw):
        raise PathPolicyError("UNC paths are not allowed")
    candidate = Path(value).expanduser()
    if not candidate.is_absolute():
        raise PathPolicyError("path must be absolute")
    resolved = candidate.resolve(strict=True)
    allowed_root: Path | None = None
    for root_value in roots:
        root = root_value.resolve(strict=True)
        try:
            resolved.relative_to(root)
            allowed_root = root
            break
        except ValueError:
            continue
    if allowed_root is None:
        raise PathPolicyError(f"path is outside all allowed roots: {resolved}")
    if _has_reparse_component(candidate, allowed_root):
        raise PathPolicyError("symlink/junction/reparse components are not allowed")
    if expected == "file" and not resolved.is_file():
        raise PathPolicyError(f"expected a file: {resolved}")
    if expected == "dir" and not resolved.is_dir():
        raise PathPolicyError(f"expected a directory: {resolved}")
    return resolved


def copy_tree_frozen(
    source: Path,
    destination: Path,
    *,
    max_files: int,
    max_bytes: int,
) -> dict[str, int]:
    source = source.resolve(strict=True)
    if not source.is_dir():
        raise EvidenceError(f"not a directory: {source}")
    if destination.exists():
        raise EvidenceError(f"destination already exists: {destination}")
    destination.mkdir(parents=True)
    file_count = 0
    byte_count = 0
    for root_text, dir_names, file_names in os.walk(source, followlinks=False):
        root = Path(root_text)
        dir_names[:] = sorted(
            name for name in dir_names if name.casefold() not in SKIP_DIRECTORY_NAMES
        )
        relative_root = root.relative_to(source)
        target_root = destination / relative_root
        target_root.mkdir(parents=True, exist_ok=True)
        for directory_name in list(dir_names):
            directory = root / directory_name
            if _has_reparse_component(directory, source):
                raise PathPolicyError(f"reparse directory rejected: {directory}")
        for file_name in sorted(file_names):
            source_file = root / file_name
            if _has_reparse_component(source_file, source):
                raise PathPolicyError(f"reparse file rejected: {source_file}")
            if source_file.suffix.lower() in FORBIDDEN_STAGE_SUFFIXES:
                raise PathPolicyError(
                    f"forbidden binary/RAW/archive in staged workspace: {source_file}"
                )
            file_count += 1
            byte_count += source_file.stat().st_size
            if file_count > max_files or byte_count > max_bytes:
                raise EvidenceError("staging limit exceeded")
            shutil.copy2(source_file, target_root / file_name)
    return {"file_count": file_count, "byte_count": byte_count}


def copy_file_exclusive(source: Path, destination: Path) -> None:
    source = source.resolve(strict=True)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        if destination.is_file() and sha256_file(destination) == sha256_file(source):
            return
        raise EvidenceError(f"staging collision with different content: {destination}")
    shutil.copy2(source, destination)


def records_under(root: Path, *, exclude_names: set[str] | None = None) -> list[dict[str, Any]]:
    excluded = exclude_names or set()
    return [
        file_record(path, relative_to=root)
        for path in sorted(root.rglob("*"))
        if path.is_file() and not (path.parent == root and path.name in excluded)
    ]


def assert_regular_tree(root: Path) -> None:
    """Reject symlinks/junctions and any resolved path that escapes an evidence tree."""
    resolved_root = root.resolve(strict=True)
    for path in resolved_root.rglob("*"):
        info = os.lstat(path)
        attributes = getattr(info, "st_file_attributes", 0)
        reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
        if stat.S_ISLNK(info.st_mode) or attributes & reparse_flag:
            raise PathPolicyError(f"reparse output rejected: {path}")
        path.resolve(strict=True).relative_to(resolved_root)


def verify_records_exact(
    root: Path,
    records: Any,
    *,
    exclude_names: set[str] | None = None,
) -> None:
    if not isinstance(records, list):
        raise EvidenceError("manifest files must be a list")
    resolved_root = root.resolve(strict=True)
    excluded = exclude_names or set()
    expected: dict[str, dict[str, Any]] = {}
    for item in records:
        if not isinstance(item, dict) or not isinstance(item.get("path"), str):
            raise EvidenceError("invalid manifest file record")
        relative = item["path"]
        candidate = Path(relative)
        if candidate.is_absolute() or ".." in candidate.parts:
            raise EvidenceError(f"manifest path escapes evidence root: {relative}")
        normalized = candidate.as_posix()
        if normalized in expected:
            raise EvidenceError(f"duplicate manifest path: {normalized}")
        expected[normalized] = item

    actual = {
        path.relative_to(resolved_root).as_posix(): path
        for path in resolved_root.rglob("*")
        if path.is_file() and not (path.parent == resolved_root and path.name in excluded)
    }
    if set(actual) != set(expected):
        missing = sorted(set(expected) - set(actual))
        extra = sorted(set(actual) - set(expected))
        raise EvidenceError(f"manifest file-set mismatch; missing={missing}, extra={extra}")
    assert_regular_tree(resolved_root)
    for relative, item in expected.items():
        path = actual[relative]
        if path.stat().st_size != item.get("size") or sha256_file(path) != item.get("sha256"):
            raise EvidenceError(f"manifest identity mismatch: {relative}")


def _terminate_recorded_process_tree(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            shell=False,
        )
    else:
        process.kill()


def run_process(
    argv: list[str],
    *,
    cwd: Path,
    timeout_seconds: int,
    stdout_path: Path,
    stderr_path: Path,
) -> dict[str, Any]:
    if not argv or any(not isinstance(item, str) or "\x00" in item for item in argv):
        raise EvidenceError("invalid process argument array")
    started = time.monotonic()
    creationflags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0) if os.name == "nt" else 0
    process = subprocess.Popen(
        argv,
        cwd=cwd,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        shell=False,
        creationflags=creationflags,
    )
    timed_out = False
    try:
        stdout_data, stderr_data = process.communicate(timeout=timeout_seconds)
    except subprocess.TimeoutExpired as first_timeout:
        timed_out = True
        _terminate_recorded_process_tree(process)
        try:
            stdout_data, stderr_data = process.communicate(timeout=10)
        except subprocess.TimeoutExpired as second_timeout:
            _terminate_recorded_process_tree(process)
            for stream in (process.stdout, process.stderr):
                if stream is not None:
                    stream.close()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
            stdout_data = second_timeout.output or first_timeout.output or b""
            stderr_data = second_timeout.stderr or first_timeout.stderr or b""
    stdout_path.write_bytes(stdout_data)
    stderr_path.write_bytes(stderr_data)
    return {
        "argv": argv,
        "cwd": str(cwd),
        "pid": process.pid,
        "exit_code": process.returncode,
        "timed_out": timed_out,
        "duration_seconds": round(time.monotonic() - started, 6),
        "stdout": stdout_path.name,
        "stderr": stderr_path.name,
    }


def decode_preview(path: Path, limit: int = 4000) -> str:
    data = path.read_bytes()[:limit]
    for encoding in ("utf-8", "gb18030", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("utf-8", errors="replace")
