from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

SCRIPT_DIR = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPT_DIR))

from package_policy import PackagePolicyError, inventory  # noqa: E402


def test_inventory_rejects_raw_and_generated_cache(tmp_path: Path) -> None:
    root = tmp_path / "package"
    root.mkdir()
    (root / "case.raw").write_bytes(b"synthetic")
    with pytest.raises(PackagePolicyError, match="forbidden"):
        inventory(root)

    (root / "case.raw").unlink()
    cache = root / "__pycache__"
    cache.mkdir()
    (cache / "module.pyc").write_bytes(b"synthetic")
    with pytest.raises(PackagePolicyError, match="generated/runtime directory"):
        inventory(root)


def test_inventory_rejects_renamed_executable_magic(tmp_path: Path) -> None:
    root = tmp_path / "package"
    root.mkdir()
    (root / "notes.txt").write_bytes(b"MZsynthetic executable")
    with pytest.raises(PackagePolicyError, match="content"):
        inventory(root)


def test_inventory_rejects_pdf_material(tmp_path: Path) -> None:
    root = tmp_path / "package"
    root.mkdir()
    (root / "manual.pdf").write_bytes(b"%PDF-synthetic")
    with pytest.raises(PackagePolicyError, match="forbidden"):
        inventory(root)


def test_manifest_scripts_do_not_self_pollute_or_bind_directory_name(
    tmp_path: Path,
) -> None:
    root = tmp_path / "renamed_module"
    scripts = root / "scripts"
    scripts.mkdir(parents=True)
    for name in ("package_policy.py", "build_manifest.py", "verify_package.py"):
        shutil.copy2(SCRIPT_DIR / name, scripts / name)
    (root / "README.md").write_text("synthetic module\n", encoding="utf-8")
    manifest_path = root / "package-manifest.json"

    built = subprocess.run(
        [sys.executable, str(scripts / "build_manifest.py"), str(root), str(manifest_path)],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
    )
    assert built.returncode == 0, built.stderr
    verified = subprocess.run(
        [sys.executable, str(scripts / "verify_package.py"), str(manifest_path), str(root)],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
    )
    assert verified.returncode == 0, verified.stdout + verified.stderr
    assert json.loads(verified.stdout)["status"] == "PASS"
    assert not list(root.rglob("__pycache__"))
