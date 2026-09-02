from __future__ import annotations

import asyncio

import pytest

from firstech_sequence_mcp import seqsimu
from firstech_sequence_mcp.errors import SafetyBlocked
from firstech_sequence_mcp.seqsimu import probe_simulation_backend


def test_simulation_is_blocked_by_default(settings_factory) -> None:
    settings = settings_factory()
    result = probe_simulation_backend(settings)
    assert result["status"] == "blocked"
    assert result["dynamic_process_started"] is False


def test_simulate_tool_cannot_start_dynamic_backend(settings_factory, monkeypatch) -> None:
    settings = settings_factory()
    monkeypatch.setattr(seqsimu, "_load_stage", lambda *args: (None, {}))
    with pytest.raises(SafetyBlocked, match="remains blocked in V1"):
        seqsimu.simulate_sequence(settings, "stage-synthetic", "0" * 64)


def test_tool_surface_has_no_device_or_arbitrary_execution() -> None:
    from firstech_sequence_mcp import server

    allowed = {
        "inspect_toolchain",
        "compile_sequence",
        "stage_simulation_input",
        "probe_simulation_backend",
        "simulate_sequence",
        "analyze_simulation",
        "read_channel_slice",
        "compare_simulations",
        "compute_logical_gradient_moment",
    }
    decorated = {
        name
        for name, value in vars(server).items()
        if callable(value) and name in allowed
    }
    assert decorated == allowed
    forbidden = {"run_device", "abort_scan", "run_shell", "load_dll", "read_raw"}
    assert forbidden.isdisjoint(vars(server))


def test_sdk_registers_exact_tool_allowlist_and_annotations() -> None:
    from firstech_sequence_mcp.server import mcp

    tools = asyncio.run(mcp.list_tools())
    by_name = {tool.name: tool for tool in tools}
    assert set(by_name) == {
        "inspect_toolchain",
        "compile_sequence",
        "stage_simulation_input",
        "probe_simulation_backend",
        "simulate_sequence",
        "analyze_simulation",
        "read_channel_slice",
        "compare_simulations",
        "compute_logical_gradient_moment",
    }
    assert by_name["inspect_toolchain"].annotations.read_only_hint is True
    assert by_name["compile_sequence"].annotations.read_only_hint is False
    assert by_name["simulate_sequence"].annotations.idempotent_hint is False
