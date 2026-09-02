from __future__ import annotations

from pathlib import Path

import pytest

from firstech_sequence_mcp.core import (
    copy_tree_frozen,
    create_run_dir,
    resolve_allowed_existing,
    resolve_run_dir,
)
from firstech_sequence_mcp.errors import PathPolicyError


def test_rejects_path_outside_allowed_root(tmp_path: Path) -> None:
    allowed = tmp_path / "allowed"
    allowed.mkdir()
    outside = tmp_path / "outside.src"
    outside.write_text("x", encoding="utf-8")
    with pytest.raises(PathPolicyError, match="outside"):
        resolve_allowed_existing(outside, (allowed,))


def test_rejects_symlink_component(tmp_path: Path) -> None:
    allowed = tmp_path / "allowed"
    real = allowed / "real"
    real.mkdir(parents=True)
    source = real / "scan.src"
    source.write_text("x", encoding="utf-8")
    link = allowed / "link"
    try:
        link.symlink_to(real, target_is_directory=True)
    except OSError:
        pytest.skip("symlink creation unavailable")
    with pytest.raises(PathPolicyError, match="reparse"):
        resolve_allowed_existing(link / "scan.src", (allowed,))


def test_new_run_never_overwrites(tmp_path: Path) -> None:
    first_id, first_path = create_run_dir(tmp_path / "runs", "compile")
    second_id, second_path = create_run_dir(tmp_path / "runs", "compile")
    assert first_id != second_id
    assert first_path != second_path
    assert first_path.is_dir() and second_path.is_dir()


def test_invalid_run_id_cannot_resolve_run_root(tmp_path: Path) -> None:
    run_root = tmp_path / "runs"
    run_root.mkdir()
    with pytest.raises(PathPolicyError, match="server-generated"):
        resolve_run_dir(run_root, ".")


def test_staging_rejects_raw_and_binary_files(tmp_path: Path) -> None:
    source = tmp_path / "source"
    source.mkdir()
    (source / "case.raw").write_bytes(b"synthetic")
    with pytest.raises(PathPolicyError, match="forbidden binary/RAW/archive"):
        copy_tree_frozen(
            source,
            tmp_path / "frozen",
            max_files=10,
            max_bytes=1024,
        )


def test_staging_skips_casefolded_cache_and_rejects_platform_binary(
    tmp_path: Path,
) -> None:
    source = tmp_path / "source"
    source.mkdir()
    (source / "keep.src").write_text("synthetic\n", encoding="utf-8")
    cache = source / ".VENV"
    cache.mkdir()
    (cache / "hidden.txt").write_text("ignore\n", encoding="utf-8")
    destination = tmp_path / "frozen"
    copy_tree_frozen(source, destination, max_files=10, max_bytes=1024)
    assert (destination / "keep.src").is_file()
    assert not (destination / ".VENV").exists()

    (source / "extension.PYD").write_bytes(b"synthetic")
    with pytest.raises(PathPolicyError, match="forbidden binary/RAW/archive"):
        copy_tree_frozen(
            source,
            tmp_path / "frozen-2",
            max_files=10,
            max_bytes=1024,
        )
