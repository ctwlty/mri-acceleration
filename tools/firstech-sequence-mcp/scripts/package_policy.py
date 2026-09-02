from __future__ import annotations

import hashlib
import os
import stat
from pathlib import Path, PurePosixPath
from typing import Any

MANIFEST_NAME = "package-manifest.json"
PACKAGE_ID = "firstech-sequence-mcp/v0.1.0"
BANNED_DIRECTORY_NAMES = {
    ".git",
    ".mypy_cache",
    ".pytest_cache",
    ".ruff_cache",
    ".venv",
    "__pycache__",
    "node_modules",
    "runs",
}
BANNED_FILE_NAMES = {".DS_Store", "Thumbs.db"}
BANNED_SUFFIXES = {
    ".7z",
    ".app",
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
    ".pdf",
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
FORBIDDEN_MAGIC_PREFIXES = (
    b"MZ",
    b"\x7fELF",
    b"PK\x03\x04",
    b"PK\x05\x06",
    b"PK\x07\x08",
    b"\xfe\xed\xfa\xce",
    b"\xfe\xed\xfa\xcf",
    b"\xce\xfa\xed\xfe",
    b"\xcf\xfa\xed\xfe",
    b"\xca\xfe\xba\xbe",
)


class PackagePolicyError(RuntimeError):
    pass


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _is_reparse(path: Path) -> bool:
    info = os.lstat(path)
    attributes = getattr(info, "st_file_attributes", 0)
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    return stat.S_ISLNK(info.st_mode) or bool(attributes & reparse_flag)


def validate_relative_path(relative: str) -> None:
    try:
        relative.encode("ascii")
    except UnicodeEncodeError as exc:
        raise PackagePolicyError(f"non-ASCII package path: {relative}") from exc
    candidate = PurePosixPath(relative)
    if candidate.is_absolute() or not candidate.parts or ".." in candidate.parts:
        raise PackagePolicyError(f"unsafe package path: {relative}")
    lowered_parts = {part.casefold() for part in candidate.parts[:-1]}
    forbidden_dirs = lowered_parts & {name.casefold() for name in BANNED_DIRECTORY_NAMES}
    if forbidden_dirs:
        raise PackagePolicyError(
            f"forbidden generated/runtime directory in package: {relative}"
        )
    if candidate.name.casefold() in {name.casefold() for name in BANNED_FILE_NAMES}:
        raise PackagePolicyError(f"forbidden metadata file in package: {relative}")
    if candidate.suffix.casefold() in BANNED_SUFFIXES:
        raise PackagePolicyError(f"forbidden binary/RAW/archive suffix: {relative}")


def _validate_file_content(path: Path, relative: str) -> None:
    with path.open("rb") as handle:
        prefix = handle.read(8)
    if any(prefix.startswith(magic) for magic in FORBIDDEN_MAGIC_PREFIXES):
        raise PackagePolicyError(f"forbidden executable/archive content: {relative}")


def inventory(
    root: Path,
    *,
    excluded: set[str] | None = None,
) -> list[dict[str, Any]]:
    root = root.resolve(strict=True)
    if not root.is_dir() or _is_reparse(root):
        raise PackagePolicyError("package root must be a regular local directory")
    excluded_paths = excluded or set()
    records: list[dict[str, Any]] = []
    seen_casefolded: set[str] = set()
    for root_text, directory_names, file_names in os.walk(root, followlinks=False):
        current = Path(root_text)
        directory_names.sort()
        file_names.sort()
        for directory_name in directory_names:
            directory = current / directory_name
            relative = directory.relative_to(root).as_posix()
            validate_relative_path(f"{relative}/placeholder")
            if _is_reparse(directory):
                raise PackagePolicyError(f"reparse directory in package: {relative}")
        for file_name in file_names:
            path = current / file_name
            relative = path.relative_to(root).as_posix()
            validate_relative_path(relative)
            if relative in excluded_paths:
                continue
            info = os.lstat(path)
            if _is_reparse(path) or not stat.S_ISREG(info.st_mode):
                raise PackagePolicyError(f"non-regular file in package: {relative}")
            folded = relative.casefold()
            if folded in seen_casefolded:
                raise PackagePolicyError(
                    f"case-insensitive duplicate package path: {relative}"
                )
            seen_casefolded.add(folded)
            _validate_file_content(path, relative)
            records.append(
                {
                    "path": relative,
                    "size": info.st_size,
                    "sha256": sha256_file(path),
                }
            )
    records.sort(key=lambda item: item["path"])
    return records
