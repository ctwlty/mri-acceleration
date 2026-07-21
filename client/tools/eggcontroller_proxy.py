"""Thin process boundary around eggcontrollerV2's verified one-shot entry."""

import argparse
import contextlib
import json
import os
import sys
from pathlib import Path


_application = None


def _create_original_ui():
    global _application
    from PyQt5.QtWidgets import QApplication
    from MainWindow_V3 import Ui_MainWindow

    _application = QApplication.instance() or QApplication(sys.argv[:1])
    return Ui_MainWindow()


def _snapshot_raw_files(output_path):
    output = Path(output_path).resolve()
    if not output.is_dir():
        return {}
    return {
        path.resolve(): (path.stat().st_size, path.stat().st_mtime_ns)
        for path in output.glob("*.raw")
        if path.is_file()
    }


def _resolve_output(root, value, label):
    if not value:
        raise RuntimeError(f"eggcontrollerV2 did not return {label}")
    path = Path(value)
    if not path.is_absolute():
        path = root / path
    path = path.resolve()
    if not path.is_file() or path.stat().st_size <= 0:
        raise RuntimeError(f"eggcontrollerV2 {label} is missing or empty: {path}")
    return path


def run_once(egg_root, ui_factory=None):
    """Call samplingBtn_click_sync exactly once and return its existing artifacts."""
    root = Path(egg_root).resolve()
    if not root.is_dir():
        raise RuntimeError(f"eggcontrollerV2 root does not exist: {root}")

    previous_directory = Path.cwd()
    try:
        os.chdir(root)
        if str(root) not in sys.path:
            sys.path.insert(0, str(root))

        ui = ui_factory() if ui_factory else _create_original_ui()
        ui.demoCheckbox.setChecked(False)
        success_before = int(getattr(ui, "samplingSuccessCount", 0))
        raw_before = _snapshot_raw_files(ui.hc.c.OUTPUT_PATH)

        ui.samplingBtn_click_sync()

        success_after = int(getattr(ui, "samplingSuccessCount", 0))
        if success_after != success_before + 1:
            status = ui.status_label.text() if hasattr(ui, "status_label") else "unknown"
            raise RuntimeError(f"eggcontrollerV2 entry did not complete successfully: {status}")
        if not ui.registerList:
            raise RuntimeError("eggcontrollerV2 returned no task record")

        record = ui.registerList[0]
        kspace_values = record.get("kspace_image") or []
        final_values = record.get("rgb_image") or []
        kspace_path = _resolve_output(root, kspace_values[0] if kspace_values else "", "K-space image")
        final_path = _resolve_output(root, final_values[0] if final_values else "", "final image")

        raw_after = _snapshot_raw_files(ui.hc.c.OUTPUT_PATH)
        changed_raw = [
            path for path, metadata in raw_after.items()
            if path not in raw_before or raw_before[path] != metadata
        ]
        if not changed_raw:
            raise RuntimeError("eggcontrollerV2 entry returned without a new or updated RAW file")
        raw_path = max(changed_raw, key=lambda path: path.stat().st_mtime_ns)

        return {
            "task_id": str(record.get("id", "")),
            "raw_path": str(raw_path),
            "kspace_image_path": str(kspace_path),
            "final_image_path": str(final_path),
        }
    finally:
        os.chdir(previous_directory)


def _emit(event):
    print(json.dumps(event, ensure_ascii=False), flush=True)


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--egg-root", required=True)
    arguments = parser.parse_args(argv)

    _emit({"event": "stage", "stage": "starting"})
    try:
        with contextlib.redirect_stdout(sys.stderr):
            result = run_once(arguments.egg_root)
        _emit({"event": "stage", "stage": "automation-entry-returned"})
        _emit({"event": "result", **result})
        return 0
    except Exception as error:
        _emit({"event": "error", "message": str(error)})
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
