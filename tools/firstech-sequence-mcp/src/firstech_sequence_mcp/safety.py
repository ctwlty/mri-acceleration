from __future__ import annotations

import hashlib
import ipaddress
import json
import os
import re
import subprocess
from pathlib import Path
from typing import Any

from .config import Settings
from .core import assert_regular_tree, file_record, read_json, sha256_file
from .errors import EvidenceError, SafetyBlocked
from .pe import inspect_pe

PROCESS_NAME_PATTERN = re.compile(r"^[A-Za-z0-9_.-]+$")


def _directory_identity(path: Path) -> dict[str, Any]:
    resolved = path.resolve(strict=True)
    assert_regular_tree(resolved)
    digest = hashlib.sha256()
    file_count = 0
    total_bytes = 0
    for file_path in sorted(resolved.rglob("*")):
        if not file_path.is_file():
            continue
        file_count += 1
        total_bytes += file_path.stat().st_size
        if file_count > 20_000 or total_bytes > 2 * 1024 * 1024 * 1024:
            raise EvidenceError("configured directory exceeds static identity limits")
        relative = file_path.relative_to(resolved).as_posix()
        item_sha = sha256_file(file_path)
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(str(file_path.stat().st_size).encode("ascii"))
        digest.update(b"\0")
        digest.update(item_sha.encode("ascii"))
        digest.update(b"\n")
    return {
        "path": str(resolved),
        "kind": "directory",
        "file_count": file_count,
        "total_bytes": total_bytes,
        "tree_sha256": digest.hexdigest(),
    }


def _windows_file_metadata(paths: list[Path]) -> dict[str, Any]:
    if os.name != "nt" or not paths:
        return {}
    env = os.environ.copy()
    env["FIRSTECH_PROBE_FILES"] = json.dumps([str(path) for path in paths])
    script = r"""
$ErrorActionPreference = 'Stop'
$paths = @($env:FIRSTECH_PROBE_FILES | ConvertFrom-Json)
$items = @()
foreach ($path in $paths) {
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
  $item = Get-Item -LiteralPath $path -ErrorAction Stop
  $signatureStatus = "unavailable"
  $signerSubject = $null
  try {
    $signature = Get-AuthenticodeSignature -LiteralPath $path -ErrorAction Stop
    $signatureStatus = [string]$signature.Status
    if ($null -ne $signature.SignerCertificate) {
      $signerSubject = [string]$signature.SignerCertificate.Subject
    }
  } catch {}
  $items += @([ordered]@{
    path = [string]$item.FullName
    file_version = [string]$item.VersionInfo.FileVersion
    product_version = [string]$item.VersionInfo.ProductVersion
    company_name = [string]$item.VersionInfo.CompanyName
    signature_status = $signatureStatus
    signer_subject = $signerSubject
  })
}
@($items) | ConvertTo-Json -Depth 5 -Compress
"""
    completed = subprocess.run(
        ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=env,
        shell=False,
        timeout=30,
    )
    if completed.returncode != 0 or not completed.stdout.strip():
        return {"_probe_error": completed.stderr[:500] or "empty metadata output"}
    try:
        items = json.loads(completed.stdout)
    except json.JSONDecodeError:
        return {"_probe_error": "invalid Windows file-metadata JSON"}
    if isinstance(items, dict):
        items = [items]
    return {str(item.get("path", "")).lower(): item for item in items}


def _parse_target(value: str) -> tuple[str, int]:
    host_text, separator, port_text = value.rpartition(":")
    if not separator:
        raise EvidenceError(f"spectrometer target must be IP:port: {value!r}")
    host = str(ipaddress.ip_address(host_text.strip("[]")))
    port = int(port_text)
    if not 1 <= port <= 65535:
        raise EvidenceError(f"invalid target port: {value!r}")
    return host, port


def _windows_state(settings: Settings) -> dict[str, Any]:
    if os.name != "nt":
        return {
            "supported": False,
            "reason": "Windows network/process inspection is target-only",
            "processes": [],
            "connections": [],
            "routes": [],
            "adapters": [],
        }
    for process_name in settings.inspection.forbidden_process_names:
        if not PROCESS_NAME_PATTERN.fullmatch(process_name):
            raise EvidenceError(f"invalid configured process name: {process_name!r}")
    targets = [
        dict(zip(("host", "port"), _parse_target(value), strict=True))
        for value in settings.inspection.spectrometer_targets
    ]
    env = os.environ.copy()
    env["FIRSTECH_PROBE_TARGETS"] = json.dumps(targets)
    env["FIRSTECH_PROBE_PROCESSES"] = json.dumps(
        list(settings.inspection.forbidden_process_names)
    )
    script = r"""
$ErrorActionPreference = 'Stop'
$targets = @($env:FIRSTECH_PROBE_TARGETS | ConvertFrom-Json)
$names = @($env:FIRSTECH_PROBE_PROCESSES | ConvertFrom-Json)
$processes = @()
foreach ($name in $names) {
  $processes += @(Get-Process -Name $name -ErrorAction SilentlyContinue |
    Select-Object Name, Id, Path, StartTime)
}
$connections = @()
$routes = @()
$routeChecks = @()
$adapters = @(Get-NetAdapter -ErrorAction Stop |
  Select-Object Name, InterfaceDescription, Status, LinkSpeed, ifIndex, MacAddress)
foreach ($target in $targets) {
  $connections += @(Get-NetTCPConnection -RemoteAddress $target.host -ErrorAction SilentlyContinue |
    Where-Object { $_.RemotePort -eq [int]$target.port } |
    Select-Object LocalAddress, LocalPort, RemoteAddress, RemotePort, State, OwningProcess)
  try {
    $foundRoutes = @(Find-NetRoute -RemoteIPAddress $target.host -ErrorAction Stop |
      Select-Object InterfaceIndex, InterfaceAlias, NextHop, DestinationPrefix, RouteMetric)
    $routes += @($foundRoutes)
    $routeChecks += @([ordered]@{
      target = "$($target.host):$($target.port)"
      status = "ok"
      route_count = @($foundRoutes).Count
    })
  } catch {
    $routeChecks += @([ordered]@{
      target = "$($target.host):$($target.port)"
      status = "error"
      error = $_.Exception.Message
    })
  }
}
[ordered]@{
  supported = $true
  processes = @($processes)
  connections = @($connections)
  routes = @($routes)
  route_checks = @($routeChecks)
  adapters = @($adapters)
} | ConvertTo-Json -Depth 6 -Compress
"""
    completed = subprocess.run(
        ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=env,
        shell=False,
        timeout=30,
    )
    if completed.returncode != 0:
        return {
            "supported": False,
            "reason": f"PowerShell probe failed: {completed.stderr[:500]}",
            "processes": [],
            "connections": [],
            "routes": [],
            "route_checks": [],
            "adapters": [],
        }
    try:
        return json.loads(completed.stdout)
    except json.JSONDecodeError as exc:
        raise EvidenceError("PowerShell probe returned invalid JSON") from exc


def inspect_toolchain(settings: Settings) -> dict[str, Any]:
    configured_paths = {
        "p2f": settings.p2f.exe,
        "include_dir": settings.p2f.include_dir,
        "system_sel": settings.p2f.system_sel,
        "current_dll": settings.inspection.current_dll,
        **{
            f"optional_binary_{index}": path
            for index, path in enumerate(settings.inspection.optional_binaries)
        },
    }
    metadata = _windows_file_metadata(
        [path for path in configured_paths.values() if path is not None and path.is_file()]
    )
    files: dict[str, Any] = {}
    for name, path in configured_paths.items():
        if path is None:
            files[name] = {"configured": False}
        elif not path.exists():
            files[name] = {"configured": True, "exists": False, "path": str(path)}
        elif path.is_dir():
            try:
                identity = _directory_identity(path)
            except EvidenceError as exc:
                identity = {"path": str(path.resolve()), "identity_error": str(exc)}
            files[name] = {"configured": True, "exists": True, **identity}
        else:
            record: dict[str, Any] = {"configured": True, "exists": True, **file_record(path)}
            windows_metadata = metadata.get(str(path.resolve()).lower())
            if windows_metadata:
                record["windows_metadata"] = windows_metadata
            if path.suffix.lower() in {".dll", ".exe"}:
                try:
                    record["pe"] = inspect_pe(path)
                except EvidenceError as exc:
                    record["pe_error"] = str(exc)
            files[name] = record
    return {
        "schema": "firstech-toolchain-inspection/v1",
        "mode": settings.mode,
        "inspection_method": "static-read-only-no-loadlibrary",
        "files": files,
        "system_state": _windows_state(settings),
        "limitations": [
            "file presence/export name does not prove supported ABI",
            "system snapshot does not execute or validate Simulate",
        ],
    }


def verify_simulation_gate(settings: Settings) -> dict[str, Any]:
    if settings.simulation.backend == "blocked":
        raise SafetyBlocked("simulation.backend is blocked by design")
    bridge = settings.simulation.bridge_exe
    gate_file = settings.simulation.gate_file
    current_dll = settings.inspection.current_dll
    if bridge is None or gate_file is None or current_dll is None:
        raise SafetyBlocked("bridge, gate file, and current DLL must all be configured")
    for path in (bridge, gate_file, current_dll):
        if not path.is_file():
            raise SafetyBlocked(f"required simulation-gate file is missing: {path}")
    gate = read_json(gate_file)
    required_true = (
        "authorized",
        "target_env_verified",
        "loopback_only",
        "no_route_to_spectrometer_verified",
    )
    if gate.get("schema") != "firstech-simulation-gate/v1":
        raise SafetyBlocked("unsupported simulation gate schema")
    if any(gate.get(field) is not True for field in required_true):
        raise SafetyBlocked("simulation gate is not fully authorized and target-verified")
    actual_bridge_sha = sha256_file(bridge)
    actual_dll_sha = sha256_file(current_dll)
    if gate.get("bridge_sha256") != actual_bridge_sha:
        raise SafetyBlocked("bridge SHA does not match the accepted gate")
    if gate.get("current_dll_sha256") != actual_dll_sha:
        raise SafetyBlocked("current DLL SHA does not match the accepted gate")
    return {
        "gate": gate,
        "bridge_sha256": actual_bridge_sha,
        "current_dll_sha256": actual_dll_sha,
    }


def require_offline_preflight(settings: Settings) -> dict[str, Any]:
    gate = verify_simulation_gate(settings)
    state = _windows_state(settings)
    if not state.get("supported"):
        raise SafetyBlocked(f"cannot prove Windows offline state: {state.get('reason', 'unknown')}")
    if state.get("processes"):
        raise SafetyBlocked("a forbidden vendor process is already running")
    if state.get("connections"):
        raise SafetyBlocked("an active spectrometer TCP connection exists")
    route_checks = state.get("route_checks", [])
    if len(route_checks) != len(settings.inspection.spectrometer_targets) or any(
        check.get("status") != "ok" for check in route_checks
    ):
        raise SafetyBlocked("route inspection was incomplete or failed")
    non_loopback_routes = [
        route
        for route in state.get("routes", [])
        if "loopback" not in str(route.get("InterfaceAlias", "")).lower()
    ]
    if non_loopback_routes:
        raise SafetyBlocked("a non-loopback route to a configured spectrometer target exists")
    return {"gate": gate, "state": state}
